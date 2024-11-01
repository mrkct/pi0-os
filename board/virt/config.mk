export LOAD_ADDRESS := 0x40000000
export ARCH := ARMV7
QEMU_CFG_FLAGS := -M virt -smp 1 -serial stdio \
	-global virtio-mmio.force-legacy=false \
	-device virtio-blk-device,drive=hd,bus=virtio-mmio-bus.1 -drive id=hd,if=none,file=$(DISK)


RECORDING_FILENAME:=virt.recording

export QEMU_BOARD_SPECIFIC_TARGETS := snapshot-drive.qcow2
snapshot-drive.qcow2:
	qemu-img create -f qcow2 snapshot-drive.qcow2 4G

ifeq ($(RECORD), 1)
QEMU_CFG_FLAGS+= \
	-icount shift=auto,rr=record,rrfile=$(RECORDING_FILENAME),rrsnapshot=start \
	-device virtio-blk-device,drive=snapshot-drive,bus=virtio-mmio-bus.0 -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2

else ifeq ($(REPLAY), 1)
QEMU_CFG_FLAGS += \
	-icount shift=auto,rr=replay,rrfile=$(RECORDING_FILENAME),rrsnapshot=start \
	-device virtio-blk-device,drive=snapshot-drive,bus=virtio-mmio-bus.0 -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2
endif

export QEMU_CFG_FLAGS := $(QEMU_CFG_FLAGS)

