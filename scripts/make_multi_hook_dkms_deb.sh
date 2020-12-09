#!/bin/bash


PACKAGE_NAME="multi-hook-dkms"
PACKAGE_VERSION="1.0.0"


POJ_DIR=..

SRC_DIR=$POJ_DIR/multi_hook
SCRIPTS_DIR=$POJ_DIR/scripts

DEB_DIR=$POJ_DIR/deb
DKMS_DIR=$DEB_DIR/usr/src/$PACKAGE_NAME-$PACKAGE_VERSION

mkdir -p $DEB_DIR
mkdir -p $DKMS_DIR



cp $SRC_DIR/const_func_hook.h        $DKMS_DIR
cp $SRC_DIR/const_func_hook.c        $DKMS_DIR
cp $SRC_DIR/multi_hook.h             $DKMS_DIR
cp $SRC_DIR/multi_hook_lib.c         $DKMS_DIR
cp $SRC_DIR/multi_hook_manager.c     $DKMS_DIR
cp $SRC_DIR/Makefile                 $DKMS_DIR
cp $SCRIPTS_DIR/multi_hook_dkms.conf $DKMS_DIR/dkms.conf



# make deb package
# apt install ruby rubygems ruby-dev
# gem install fpm
fpm -s dir \
    -t deb \
    -p "$PACKAGE_NAME"_VERSION_ARCH.deb \
    --description 'multi_hook' \
    -n $PACKAGE_NAME \
    -v $PACKAGE_VERSION \
    --after-install $SCRIPTS_DIR/multi_hook_dkms_after_install.sh \
    --before-remove $SCRIPTS_DIR/multi_hook_dkms_before_remove.sh \
    -C $DEB_DIR usr/


rm -rf $DEB_DIR
