#!/bin/bash


PACKAGE_NAME="multi-hook-dkms"
PACKAGE_VERSION="1.3.0"


POJ_DIR=..

SRC_DIR=$POJ_DIR/multi_hook
SCRIPTS_DIR=$POJ_DIR/scripts

DEB_DIR=$POJ_DIR/deb
DKMS_DIR=$DEB_DIR/usr/src/$PACKAGE_NAME-$PACKAGE_VERSION

mkdir -p $DEB_DIR/DEBIAN
mkdir -p $DKMS_DIR



cp $SRC_DIR/const_func_hook.h        $DKMS_DIR/
cp $SRC_DIR/const_func_hook.c        $DKMS_DIR/
cp $SRC_DIR/multi_hook.h             $DKMS_DIR/
cp $SRC_DIR/multi_hook_lib.c         $DKMS_DIR/
cp $SRC_DIR/multi_hook_manager.c     $DKMS_DIR/
cp $SRC_DIR/Makefile                 $DKMS_DIR/
cp $SCRIPTS_DIR/multi_hook_dkms.conf $DKMS_DIR/dkms.conf

cp $SCRIPTS_DIR/multi_hook_dkms_after_install.sh $DEB_DIR/DEBIAN/postinst
cp $SCRIPTS_DIR/multi_hook_dkms_before_remove.sh $DEB_DIR/DEBIAN/prerm
cp $SCRIPTS_DIR/debian_control                   $DEB_DIR/DEBIAN/control

(cd $DEB_DIR && md5sum `find usr -type f` > DEBIAN/md5sums)


dpkg-deb -b $DEB_DIR .

rm -rf $DEB_DIR
