#!/bin/sh

# This script is used on the CI to build the kernel, userland and
# create the release artifacts.

printf "\033[1;32mBuilding kernel...\n\033[0m"
make -j$(nproc) kernel

printf "\033[1;32mBuilding userland...\n\033[0m"
make -j$(nproc) userland

printf "\033[1;32mCreating release artifacts...\n\033[0m"
mkdir -p ci-artifacts
cp kernel/boot/boot.elf ci-artifacts/boot.elf
cp userland/_disk_image.qcow2 ci-artifacts/disk.qcow2

echo "#!/bin/sh" > ci-artifacts/run.sh
KERNEL=boot.elf DISK=disk.qcow2 make qemu-ci-script >> ci-artifacts/run.sh
chmod +x ci-artifacts/run.sh
