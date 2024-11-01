ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

export BOARD ?= virt
export RECORD ?= 0
export REPLAY ?= 0
export DISK ?= $(ROOT_DIR)/_disk_image.bin

include board/$(BOARD)/config.mk

ifdef CONFIG_BUNDLED_DTB
	export CONFIG_BUNDLED_DTB := $(ROOT_DIR)/board/$(BOARD)/$(CONFIG_BUNDLED_DTB)
endif

QEMU:=qemu-system-arm

QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp $(QEMU_CFG_FLAGS) -kernel kernel/boot/boot.elf
QEMU_BOARD_SPECIFIC_TARGETS ?= # empty

.PHONY: all kernel userland clean qemu qemu-gdb 

all: kernel userland

kernel:
	$(MAKE) -e -C kernel

userland:
	# $(MAKE) -e -C userland

qemu: all $(QEMU_BOARD_SPECIFIC_TARGETS)
	$(QEMU) $(QEMU_FLAGS)

qemu-gdb: all $(QEMU_BOARD_SPECIFIC_TARGETS)
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S

clean:
	$(MAKE) -e -C kernel clean
	# $(MAKE) -e -C userland clean
