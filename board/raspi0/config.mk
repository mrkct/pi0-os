export LOAD_ADDRESS := 0x8000
export ARCH := ARMV6

export CONFIG_BUNDLED_DTB := bcm2708-rpi-zero-w.dtb
export QEMU_CFG_FLAGS := -M raspi0 -serial null -serial stdio
