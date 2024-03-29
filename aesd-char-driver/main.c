/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

/*
 * ref: https://github.com/cu-ecen-aeld/ldd3/blob/master/scull/main.c
 *
 * locking: mutex based, single resource circular buffer design.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>    // https://elixir.bootlin.com/linux/v5.15/source/include/linux/types.h
#include <linux/cdev.h>     // https://elixir.bootlin.com/linux/v5.15/source/include/linux/cdev.h
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/string.h>

#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Shifting5164"); /** DONE: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
struct aesd_circular_buffer buffer;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    struct aesd_dev *dev = filp->private_data;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; /* for other methods */
    filp->f_pos = 0;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    ssize_t retval = 0;

    struct aesd_dev *dev = filp->private_data;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    struct aesd_buffer_entry *psData = NULL;
    size_t pOffset;
    if ( (psData = aesd_circular_buffer_find_entry_offset_for_fpos(&buffer, *f_pos, &pOffset )) != NULL){
        if ( count > psData->size - pOffset ){
            count = psData->size - pOffset;
        }

        if (copy_to_user(buf, &psData->buffptr[pOffset], count )) {
            retval = -EFAULT;
            goto exit;
        }

        *f_pos += count;
        retval = count;

    }else{
        retval = 0;
    }

exit:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,loff_t *f_pos){

    struct aesd_dev *dev = filp->private_data;
    char *dst_user_data = NULL;
    char *old_entry = NULL;
    ssize_t retval = -ENOMEM;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    if (count <= 0 ){
        retval = 0;
        goto exit;
    }

    /* Reserve or expand memory for receiving data */
    if ( dev->new_entry.buffptr == NULL ){

        PDEBUG("new aesd_buffer_entry");

        /* Alloc memory for new data */
        if ((dev->new_entry.buffptr = kzalloc(count, GFP_KERNEL)) == NULL) {
            retval = -ENOMEM;
            goto exit;
        }

        dst_user_data = dev->new_entry.buffptr;
        dev->new_entry.size = 0;

    }else{
        PDEBUG("old aesd_buffer_entry");

        /* Already got part of a message, resize old entry and append new data chunk */
        if ((dev->new_entry.buffptr = krealloc(dev->new_entry.buffptr, dev->new_entry.size + count, GFP_KERNEL)) == NULL){
            kfree(dev->new_entry.buffptr);
            retval = -ENOMEM;
            goto exit;
        }

        dst_user_data = &dev->new_entry.buffptr[dev->new_entry.size];
    }

    /* Here:
     * - dst_user_data pointer is defined
     * - new_entry->size = 0 || size of previous chunks
    */

    /* Copy from user */
    if (copy_from_user(dst_user_data, buf, count)){
        kfree(dev->new_entry.buffptr);
        retval = -EFAULT;
        goto exit;
    }

    /* Total message size, 1 chuck, or multiple */
    dev->new_entry.size += count;
    PDEBUG("new data :%ld", dev->new_entry.size);

    /* Complete message ? */
    if ( (memchr(dev->new_entry.buffptr, '\n', dev->new_entry.size)) == NULL ){
        /* First chuck, more to follow */
        PDEBUG("Part of message:%ld:%s", dev->new_entry.size,dev->new_entry.buffptr);
    } else {
        /* Add new entry, when buffer is full it will start to overwrite. Catch the old message
         * and free memory. */
        if ( (old_entry = aesd_circular_buffer_add_entry(&buffer,&dev->new_entry)) != NULL){
            PDEBUG("release old data:%s", old_entry);
            kfree(old_entry);
        }
        PDEBUG("Written to buffer:%ld", dev->new_entry.size);

        dev->new_entry.buffptr = NULL;
        dev->new_entry.size = 0;
    }

    retval = count;

exit:
    mutex_unlock(&dev->lock);
    return retval;
}


loff_t aesd_lseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t newpos;

    PDEBUG("aesd_lseek");

    PDEBUG("off:%lld, whence:%d", off, whence);

    switch(whence) {
        case 0: /* SEEK_SET */
            newpos = off;
            break;

        case 1: /* SEEK_CUR */
            newpos = filp->f_pos + off;
            break;

        case 2: /* SEEK_END */
            newpos = buffer.size + off;
            break;

        default: /* can't happen */
            return -EINVAL;
    }
    if (newpos < 0) return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {

    int retval, err = 0;
    struct aesd_seekto user_arg;

    struct aesd_dev *dev = filp->private_data;

    PDEBUG("aesd_unlocked_ioctl");

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) {
        return -ENOTTY;
    }

    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) {
        return -ENOTTY;
    }

    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if (_IOC_DIR(cmd) & _IOC_READ) {
        err = !access_ok((void __user*)arg, _IOC_SIZE(cmd));
    } else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        err = !access_ok((void __user*)arg, _IOC_SIZE(cmd));
    }

    if (err) {
        return -EFAULT;
    }

    switch(cmd) {

        case AESDCHAR_IOCSEEKTO:

            PDEBUG("AESDCHAR_IOCSEEKTO");

            if (copy_from_user(&user_arg, (const void __user *)arg, sizeof(struct aesd_seekto))) {
                retval = -EINVAL;
            }

            PDEBUG("aesd_seekto: write_cmd:%d write_cmd_offset:%d", user_arg.write_cmd, user_arg.write_cmd_offset);

            if (mutex_lock_interruptible(&dev->lock)) {
                return -ERESTARTSYS;
            }

            if (buffer.entry[user_arg.write_cmd].buffptr != NULL){
                PDEBUG("got entry");
                if (user_arg.write_cmd_offset < buffer.size ){
                    PDEBUG("got offset");

                    aesd_buffer_entry *entry;
                    uint8_t index;
                    filp->f_pos = 0;

                    AESD_CIRCULAR_BUFFER_FOREACH(entry, &buffer, index){
                        if (index != user_arg.write_cmd) {
                            filp->f_pos += entry->size;
                            PDEBUG("filp->f_pos:%d",filp->f_pos);
                        } else {
                            filp->f_pos += user_arg.write_cmd_offset;
                            PDEBUG("filp->f_pos:%d", filp->f_pos);
                            PDEBUG("return 0");
                            retval =  0;
                            goto exit;
                        }
                    }
                }
            }

            PDEBUG("-1");
            retval =  -1;
            break;

        default:  /* redundant, as cmd was checked against MAXNR */
            retval -ENOTTY;
    }
exit:
    mutex_unlock(&dev->lock);
    return retval;

}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek  = aesd_lseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,"aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /* Buffer mutex */
    mutex_init(&aesd_device.lock);

    /* Actual buffer */
    aesd_circular_buffer_init(&buffer);

    aesd_device.new_entry.buffptr = NULL;
    aesd_device.new_entry.size = 0;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    if (aesd_device.new_entry.buffptr != NULL){
        kfree(aesd_device.new_entry.buffptr);
    }

    cdev_del(&aesd_device.cdev);

    aesd_buffer_entry *entry;
    uint8_t index;
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&buffer,index){
        kfree(entry->buffptr);
    };

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
