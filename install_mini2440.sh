#!/bin/sh
#modify on 2015/10/20
SOURCE_CODE="1-mini2440.tgz"

TOOLCHAIN="./${SOURCE_CODE}/arm-linux-gcc-4.4.3.tar.gz"
SAMPLECODE="./${SOURCE_CODE}/examples-20100108.tar.gz"
KERNEL="./${SOURCE_CODE}/linux-2.6.32.2-mini2440-20100921.tar.gz"
ROOTFS="./${SOURCE_CODE}/rootfs_qtopia_qt4-20100816.tar.gz"

setup_toolchain()
{
	[ -d /opt/FriendlyARM/toolschain/4.4.3 ] && {
		echo "/opt/FriendlyARM/toolschain/4.4.3 already exist"
	} || {
		tar zxvf ${TOOLCHAIN} -C /
	}
}

setup_samplecode()
{
	[ -d /opt/FriendlyARM/mini2440/examples/ ] && {
		echo "/opt/FriendlyARM/mini2440/examples/ already exist"
	}||{
		tar zxvf ${SAMPLECODE} -C /opt/FriendlyARM/mini2440/
	}
}

setup_kernel()
{
	[ -d /opt/FriendlyARM/mini2440/linux-2.6.32.2/ ] && {
		echo "/opt/FriendlyARM/mini2440/linux-2.6.32.2/ already exist"
	} || {
		tar zxvf ${KERNEL} -C /opt/FriendlyARM/mini2440/
		rm -rf /opt/FriendlyARM/mini2440/linux-2.6.32.2/REPORTING-BUGS
		rm -rf /opt/FriendlyARM/mini2440/linux-2.6.32.2/COPYING
		rm -rf /opt/FriendlyARM/mini2440/linux-2.6.32.2/CREDITS
		rm -rf /opt/FriendlyARM/mini2440/linux-2.6.32.2/MAINTAINERS
		rm -rf /opt/FriendlyARM/mini2440/linux-2.6.32.2/README
		rm -rf /opt/FriendlyARM/mini2440/linux-2.6.32.2/config_mini2440_*
	}
}

setup_rootfs()
{
	[ -d /opt/FriendlyARM/mini2440/rootfs_qtopia_qt4/ ] && {
		echo "/opt/FriendlyARM/mini2440/rootfs_qtopia_qt4/ already exist"
	} || {
		tar zxvf ${ROOTFS} -C /opt/FriendlyARM/mini2440/
	}
}

[ -d /opt/FriendlyARM ] && {
	echo "start install to /opt/FriendlyARM"
	#setup_toolchain
	#setup_samplecode
	setup_kernel
	#setup_rootfs
} || {
	echo "pls create dir :/opt/FriendlyARM"
}

echo "Add: export PATH=/opt/FriendlyARM/toolschain/4.4.3/bin:\$PATH =>to .bashrc"
