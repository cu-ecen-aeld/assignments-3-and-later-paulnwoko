#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

# OUTDIR=/tmp/aeld
OUTDIR=/media/usbpc/cc2986ac-1789-4375-ad4a-b28d7804e953/coursera/qemu-assignment
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

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

    # TODO: Add your kernel build steps here
    #install build toolsif not already installed - see Installing crosstool-NG in page 21 of the Mastering_Embedded_Linux_Programming_ElectroVolt.ir_.pdf
    #sudo apt-get install automake bison chrpath flex g++ git gperf gawk libexpat1-dev libncurses5-dev libsdl1.2-dev libtool python2.7-dev texinfo

    #create seperate build directory
    BUILD_DIR=/media/usb-pc/cc2986ac-1789-4375-ad4a-b28d7804e953/coursera/qemu-assignment/linux-build/
    #clean kernel build tree removing .config file with any existing configurations
    # make -j8 O=${BUILD_DIR} ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    # #defconfig build: setup defconfig to configure for our virt: arm dev board we will simulate in qemu
    # make -j8 O=${BUILD_DIR} ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    # # Build vmlinux - kernel image for booting with qemu
    # make -j10 O=${BUILD_DIR} ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    # #build the module and device tree
    # make -j10 O=${BUILD_DIR} ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules #build kernel module
    # make -j$(nproc-7) O=${BUILD_DIR} ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs #build the device tree
    
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-build/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
# git clone https://github.com/mirror/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make -j8 CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install #copy busybox and create all the symlinks for us

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# busybox lib dependencies
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/
cp -a ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp -a ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 1 5

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cd ${OUTDIR}/rootfs
cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/writer.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/  #Modify the finder-test.sh script to reference conf/assignment.txt instead of ../conf/assignment.txt.
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp -r ${FINDER_APP_DIR}/conf/ ${OUTDIR}/rootfs/home/

# TODO: Chown the root directory
echo "Chown the root directory"
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
