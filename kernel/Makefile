SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-fpic -ffreestanding -nostdlib -lgcc
CXXFLAGS+=-MMD -MP -I$(SOURCES_ROOT) -I../include
CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

BOARD:=raspi0
ifeq ($(BOARD), raspi0)
	CXXFLAGS+=-mcpu=arm1176jzf-s
else
	CXXFLAGS+=-mcpu=cortex-a7 -DBOARD_RASPI2
endif

ifdef RAMDISK
	CXXFLAGS+=-DRAMDISK_FILE=$(RAMDISK)
endif

QEMU:=qemu-system-arm
QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp -M $(BOARD) -serial null -serial stdio -kernel Kernel
QEMU_REPLAY_FILENAME:=replay.bin
QEMU_RECORD_FLAGS:=\
	-icount shift=auto,rr=record,rrfile=$(QEMU_REPLAY_FILENAME),rrsnapshot=start \
	-device sd-card,drive=snapshot-drive -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2
QEMU_REPLAY_FLAGS:=\
	-icount shift=auto,rr=replay,rrfile=$(QEMU_REPLAY_FILENAME),rrsnapshot=start \
	-device sd-card,drive=snapshot-drive -drive id=snapshot-drive,if=none,file=snapshot-drive.qcow2
ifdef SD_IMAGE
QEMU_SD_FLAGS+=-sd $(SD_IMAGE)
else
QEMU_SD_FLAGS:=
endif

LIBS_SOURCES=\
	kernel/lib/libc/string.cpp \
	kernel/lib/memory.cpp \
	kernel/lib/string.cpp \

SOURCES=\
	$(LIBS_SOURCES) \
	kernel/datetime.cpp \
	kernel/device/gpio.cpp \
	kernel/device/framebuffer.cpp \
	kernel/device/keyboard.cpp \
	kernel/device/miniuart.cpp \
	kernel/device/ramdisk.cpp \
	kernel/device/sd.cpp \
	kernel/device/systimer.cpp \
	kernel/device/uart.cpp \
	kernel/device/videocore.cpp \
	kernel/filesystem/fat32/fat32.cpp \
	kernel/filesystem/filesystem.cpp \
	kernel/interrupt.cpp \
	kernel/kprintf.cpp \
	kernel/locking/reentrant.cpp \
	kernel/locking/spinlock.cpp \
	kernel/memory/armv6mmu.cpp \
	kernel/memory/kheap.cpp \
	kernel/memory/physicalalloc.cpp \
	kernel/memory/vm.cpp \
	kernel/prekernel/early.cpp \
	kernel/prekernel/main.cpp \
	kernel/prekernel/start.S \
	kernel/prekernel/vector_table.S \
	kernel/syscall/syscalls.cpp \
	kernel/task/elf_loader.cpp \
	kernel/task/scheduler.cpp \
	kernel/timer.cpp \

DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
DEPENDS:=$(DEPENDS) linker.ld 
DEPENDS:= $(addprefix $(SOURCES_ROOT), $(DEPENDS))
OBJS:=$(addsuffix .o, $(SOURCES))
OBJS:=$(addprefix $(SOURCES_ROOT), $(OBJS))

.PHONY: all clean qemu qemu-gdb 

all: Kernel

snapshot-drive.qcow2:
	qemu-img create -f qcow2 snapshot-drive.qcow2 4G

qemu: Kernel
	$(QEMU) $(QEMU_FLAGS) $(QEMU_SD_FLAGS)

qemu-record: Kernel snapshot-drive.qcow2
	$(QEMU) $(QEMU_FLAGS) $(QEMU_RECORD_FLAGS)

qemu-gdb: Kernel
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) $(QEMU_SD_FLAGS) -s -S

qemu-gdb-replay: Kernel snapshot-drive.qcow2
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S $(QEMU_REPLAY_FLAGS)

kernel.img: Kernel
	$(OBJCOPY) Kernel -O binary _kernel.img
	# Note that the RPi loads the binary at 0x8000,
	# so we need to trim the first 0x8000 bytes from the binary
	dd if=_kernel.img of=kernel.img bs=1024 skip=32

Kernel: $(OBJS)
	$(CXX) -T linker.ld -o Kernel $(OBJS) $(CXXFLAGS) -lgcc

-include $(DEPENDS)

%.cpp.o: %.cpp Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.S.o: %.S Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

clean:
	$(RM) -f $(OBJS) $(DEPENDS)
	$(RM) -f Kernel
	$(RM) -f kernel7.img
