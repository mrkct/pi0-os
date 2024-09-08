export LOAD_ADDRESS := 0x8000
export ARCH := ARMV6

export CONFIG_BUNDLED_DTB := bcm2708-rpi-zero-w.dtb
QEMU_CFG_FLAGS := -M raspi0 -serial null -serial stdio

RECORDING_FILENAME:=raspi0.recording

export QEMU_BOARD_SPECIFIC_TARGETS := snapshot-drive.qcow2
snapshot-drive.qcow2:
	qemu-img create -f qcow2 snapshot-drive.qcow2 4G

ifeq ($(RECORD), 1)
QEMU_CFG_FLAGS+= \
	-icount shift=auto,rr=record,rrfile=$(RECORDING_FILENAME),rrsnapshot=start \
	-device sd-card,drive=snapshot-drive -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2
else ifeq ($(REPLAY), 1)
QEMU_CFG_FLAGS += \
	-icount shift=auto,rr=replay,rrfile=$(RECORDING_FILENAME),rrsnapshot=start \
	-device sd-card,drive=snapshot-drive -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2
endif

export QEMU_CFG_FLAGS := $(QEMU_CFG_FLAGS)
