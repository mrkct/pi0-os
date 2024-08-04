ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

export BOARD ?= raspi0
include board/$(BOARD)/config.mk

ifdef CONFIG_BUNDLED_DTB
	export CONFIG_BUNDLED_DTB := $(ROOT_DIR)/board/$(BOARD)/$(CONFIG_BUNDLED_DTB)
endif

QEMU:=qemu-system-arm
QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp -M $(BOARD) -serial null -serial stdio -kernel kernel/boot/boot.elf
QEMU_REPLAY_FILENAME:=replay.bin
QEMU_RECORD_FLAGS:=\
	-icount shift=auto,rr=record,rrfile=$(QEMU_REPLAY_FILENAME),rrsnapshot=start \
	-device sd-card,drive=snapshot-drive -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2
QEMU_REPLAY_FLAGS:=\
	-icount shift=auto,rr=replay,rrfile=$(QEMU_REPLAY_FILENAME),rrsnapshot=start \
	-device sd-card,drive=snapshot-drive -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2

.PHONY: all kernel userland clean qemu qemu-gdb 

all: kernel userland

kernel:
	$(MAKE) -e -C kernel

userland:
	# $(MAKE) -e -C userland

snapshot-drive.qcow2:
	qemu-img create -f qcow2 snapshot-drive.qcow2 4G

qemu: all
	$(QEMU) $(QEMU_FLAGS) $(QEMU_SD_FLAGS)

qemu-record: all snapshot-drive.qcow2
	$(QEMU) $(QEMU_FLAGS) $(QEMU_RECORD_FLAGS)

qemu-gdb: all
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) $(QEMU_SD_FLAGS) -s -S

qemu-gdb-replay: all snapshot-drive.qcow2
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S $(QEMU_REPLAY_FLAGS)

clean:
	$(MAKE) -e -C kernel clean
	# $(MAKE) -e -C userland clean
