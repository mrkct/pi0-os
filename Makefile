ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

export KERNEL ?= kernel/boot/boot.elf
export BOARD ?= virt
export RECORD ?= 0
export REPLAY ?= 0
export DISK ?= $(ROOT_DIR)/userland/_disk_image.qcow2

include board/$(BOARD)/config.mk

ifdef CONFIG_BUNDLED_DTB
	export CONFIG_BUNDLED_DTB := $(ROOT_DIR)/board/$(BOARD)/$(CONFIG_BUNDLED_DTB)
endif

QEMU:=qemu-system-arm # /home/marco/Desktop/qemu/build/qemu-system-arm

QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp -display gtk,zoom-to-fit=off $(QEMU_CFG_FLAGS) -kernel $(KERNEL)
QEMU_BOARD_SPECIFIC_TARGETS ?= # empty

.PHONY: all kernel userland clean qemu qemu-gdb 

all: kernel userland

kernel:
	$(MAKE) -e -C kernel

userland:
	$(MAKE) -e -C userland

qemu: all $(QEMU_BOARD_SPECIFIC_TARGETS)
	$(QEMU) $(QEMU_FLAGS)

qemu-ci-script:
	@echo $(QEMU) $(QEMU_FLAGS)

qemu-gdb: all $(QEMU_BOARD_SPECIFIC_TARGETS)
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S

clean:
	$(MAKE) -e -C kernel clean
	$(MAKE) -e -C userland clean
