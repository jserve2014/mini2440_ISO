#!/bin/sh

DIR="01-mini2440_tgz"

TOOLCHAIN="./${DIR}/arm-linux-gcc-4.4.3.tar.gz"
SAMPLECODE="./${DIR}/examples-20100108.tar.gz"
KERNEL="./${DIR}/linux-2.6.32.2-mini2440-20100921.tar.gz"
ROOTFS="./${DIR}/rootfs_qtopia_qt4-20100816.tar.gz"

toolchain()
{
	sudo tar zxvf ${TOOLCHAIN} -C /
	ls -l /opt/FriendlyARM/toolschain/4.4.3/
}

samplecode()
{
	sudo mkdir /opt/FriendlyARM/mini2440/
	sudo tar zxvf ${SAMPLECODE} -C /opt/FriendlyARM/mini2440/
	ls -l /opt/FriendlyARM/mini2440/examples/
}

kernel()
{
	sudo tar zxvf ${KERNEL} -C /opt/FriendlyARM/mini2440/
	ls /opt/FriendlyARM/mini2440/
}

rootfs()
{
	sudo tar zxvf ${ROOTFS} -C /opt/FriendlyARM/mini2440/
	ls /opt/FriendlyARM/mini2440/
}

[ -d /opt/FriendlyARM ] && {
	echo "start install to /opt/FriendlyARM"
	toolchain
	samplecode
	kernel
	rootfs
} || {
	echo "pls create dir :/opt/FriendlyARM"
}


echo "Add: export PATH=/opt/FriendlyARM/toolschain/4.4.3/bin:\$PATH =>to .bashrc"
