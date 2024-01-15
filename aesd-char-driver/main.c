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

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Shifting5164"); /** DONE: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
struct aesd_circular_buffer buffer;

int aesd_trim(struct aesd_dev *dev)
{

    dev->message_part = NULL;

    return 0;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    struct aesd_dev *dev = filp->private_data;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; /* for other methods */

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    /* TODO */

    mutex_unlock(&dev->lock);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    struct aesd_dev *dev = filp->private_data;

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    aesd_trim(filp->private_data);

    mutex_unlock(&dev->lock);
    return 0;
}

/**
 * TODO: handle read
 */
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
        if ( count > psData->size ){
            count = psData->size;
        }

        if (copy_to_user(buf, psData->buffptr, count )) {
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

/**
 * TODO: handle write
 */
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{

    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *new_entry;
    struct aesd_buffer_entry *old_entry;
    struct aesd_buffer_entry *tmp_entry;

    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    /* Alloc memory for new entry */
    if ( (new_entry = kzalloc(sizeof(new_entry), GFP_KERNEL)) == NULL ){
        goto exit;
    }

    /* Alloc memory for new data */
    if ( (new_entry->buffptr = kzalloc(count, GFP_KERNEL)) == NULL) {
        kfree(new_entry);
        goto exit;
    }

    /* Copy from user */
    if (copy_from_user(new_entry->buffptr, buf, count)){
        kfree(new_entry->buffptr);
        kfree(new_entry);
        retval = -EFAULT;
        goto exit;
    }

    new_entry->size = count;

    /* Complete message ? */
    if (strstr(new_entry->buffptr, "\n") == NULL ){
        /* Not a complete message received */

        /* Is this a first chuck or not ?*/
        if (dev->message_part == NULL ){
            /* First chuck */

            /* Archive for next run */
            dev->message_part = new_entry;
            retval = count;

            PDEBUG("Part of message:%s", new_entry->buffptr);
            goto exit;
        } else {
            /* No, not first chunk */

            /* Keep track of received data */
            tmp_entry = new_entry;

            if ( (new_entry = kmalloc(sizeof(new_entry), GFP_KERNEL)) == NULL ){
                retval = -ENOMEM;
                goto exit;
            }

            /* Alloc memory for new data */
            if ( (new_entry->buffptr = kmalloc(count + tmp_entry->size, GFP_KERNEL)) == NULL) {
                kfree(new_entry);
                retval = -ENOMEM;
                goto exit;
            }

            /* copy data old */
            memcpy(new_entry->buffptr, tmp_entry->buffptr, tmp_entry->size);
            /* add new */
            memcpy(&new_entry->buffptr[tmp_entry->size], new_entry->buffptr, new_entry->size);

            new_entry->size = tmp_entry->size + count;

            kfree(tmp_entry);
            PDEBUG("Full message:%s", new_entry->buffptr);
        }
    }

    /* Free oldest data when full in buffer */
    if (buffer.full){
        old_entry = &buffer.entry[buffer.in_offs];
        kfree(old_entry->buffptr);
        kfree(old_entry);
    }

    /* Add new entry */
    aesd_circular_buffer_add_entry(&buffer,new_entry);
    retval = count;

    PDEBUG("Written to buffer:%s", new_entry->buffptr);


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
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    /* Buffer mutex */
    mutex_init(&aesd_device.lock);

    /* Actual buffer */
    aesd_circular_buffer_init(&buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
