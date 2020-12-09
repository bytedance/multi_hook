#!/bin/bash

echo "multi_hook_dkms_before_remove.sh"
echo "arg0: $0"
echo "arg1: $1"
echo "arg2: $2"
echo "arg3: $3"

# remove dkms
PACKAGE_NAME="multi-hook-dkms"
PACKAGE_VERSION="1.3.0"

case "$1" in
    remove|upgrade|deconfigure)

        echo "1"
        modprobe -r multi_hook


        echo "2"
        rm -f /etc/modprobe.d/multi_hook.conf

        echo "2"
        rm -f /usr/lib/modules-load.d/multi_hook.conf

        echo "3"
        if [  "$(dkms status -m $PACKAGE_NAME -v $PACKAGE_VERSION)" ]; then
            dkms remove -m $PACKAGE_NAME -v $PACKAGE_VERSION --all
        fi

        echo "4"
        lsmod | grep multi_hook

        echo "5"
    ;;
esac
