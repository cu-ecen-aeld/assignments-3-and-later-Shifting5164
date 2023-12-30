#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

# a)
if [ -n "${1:-}" ]; then
	OUTDIR="$(realpath "$1")"
else
	OUTDIR=/tmp/aeld	
fi
# b)
mkdir -p "${OUTDIR}"

KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"

(
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # DONE: Add your kernel build steps here
    make distclean
    make defconfig
    make -j$(nproc)
    make -j$(nproc) dtbs
fi
)

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
(
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# DONE: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs/"{bin,dev,etc,home,lib,opt,proc,run,sbin,sys,tmp,usr,var,var/log,usr/bin,usr/lib,usr/bin,lib64}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # DONE:  Configure busybox

else
    cd busybox
fi

# DONE: Make and install busybox
if [ ! -f busybox ]; then
	make distclean
	make defconfig
	make -j$(nproc)
fi

make CONFIG_PREFIX="${OUTDIR}/rootfs" install
)

(
echo "Library dependencies"
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# DONE: Add library dependencies to rootfs
sudo cp "${SYSROOT}/lib/ld-linux-aarch64.so.1" lib/
sudo cp "${SYSROOT}/lib64/"{libm.so.6,libresolv.so.2,libc.so.6} lib64/

# DONE: Make device nodes
sudo mknod -m 666 dev/null c 1 3 
sudo mknod -m 666 dev/zero c 1 5
sudo mknod -m 666 dev/random c 1 8
sudo mknod -m 600 dev/console c 5 3
)

# DONE: Clean and build the writer utility
make clean
make

# DONE: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
sudo cp {./finder-test.sh,autorun-qemu.sh,writer,writer.sh,finder.sh} "${OUTDIR}/rootfs/home/"
mkdir -p "${OUTDIR}/rootfs/home/conf"
sudo cp {conf/username.txt,conf/assignment.txt} "${OUTDIR}/rootfs/home/conf"

# DONE: Chown the root directory
sudo chown -R root:root "${OUTDIR}/rootfs/"

# DONE: Create initramfs.cpio.gz
(
cd "${OUTDIR}/rootfs/"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio
)

