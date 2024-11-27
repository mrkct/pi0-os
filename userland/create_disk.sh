#!/bin/sh

DIR=/tmp/myos-disk-image-temp-storage

rm -rf $DIR
mkdir -p $DIR
cp -r base/* $DIR

make -C libs libapp.a

TO_INSTALL="init"
for software in $TO_INSTALL; do
    echo "Installing '$software'..."
    sudo MOUNT_POINT=$DIR make -C $software install
done

rm -r _disk_image.img
dd if=/dev/zero of=_disk_image.img bs=1M count=16
mkfs.fat -F 32 _disk_image.img
mcopy -i _disk_image.img $DIR/* ::/
qemu-img convert -p -O qcow2 _disk_image.img _disk_image.qcow2
rm -rf $DIR
