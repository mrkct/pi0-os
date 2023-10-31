#!/bin/sh

rm -r disk.img
dd if=/dev/zero of=disk.img bs=1M count=32
mkfs.fat -F 32 disk.img

mkdir -p mp
sudo mount disk.img mp
sudo cp -r base/* mp/

TO_INSTALL="hello clock"
for software in $TO_INSTALL; do
    echo "Installing $software..."
    sudo MOUNT_POINT=$(pwd)/mp make -C $software install
done

sudo umount mp
rm -r mp
