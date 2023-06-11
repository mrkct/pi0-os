SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-mcpu=arm1176jzf-s -fpic -ffreestanding -nostdlib -lgcc
CXXFLAGS+=-MMD -MP -I$(SOURCES_ROOT)
CXXFLAGS+=-fno-exceptions -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

QEMU:=qemu-system-arm
QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp -M raspi0 -serial stdio -kernel Kernel
QEMU_REPLAY_FILENAME:=replay.bin
QEMU_RECORD_FLAGS:=-icount shift=auto,rr=record,rrfile=$(QEMU_REPLAY_FILENAME)
QEMU_REPLAY_FLAGS:=-icount shift=auto,rr=replay,rrfile=$(QEMU_REPLAY_FILENAME)
ifdef SD_IMAGE
QEMU_FLAGS+=-sd $(SD_IMAGE)
endif

LIBS_SOURCES=\
	kernel/lib/libc/string.cpp \
	kernel/lib/memory.cpp \
	kernel/lib/psf/default_font.cpp \
	kernel/lib/psf/psf.cpp \
	kernel/lib/string.cpp \

SOURCES=\
	$(LIBS_SOURCES) \
	kernel/datetime.cpp \
	kernel/device/gpio.cpp \
	kernel/device/sd.cpp \
	kernel/device/systimer.cpp \
	kernel/device/uart.cpp \
	kernel/device/videoconsole.cpp \
	kernel/device/videocore.cpp \
	kernel/filesystem/fat32/fat32.cpp \
	kernel/filesystem/filesystem.cpp \
	kernel/interrupt.cpp \
	kernel/kprintf.cpp \
	kernel/locking/reentrant.cpp \
	kernel/locking/spinlock.cpp \
	kernel/memory/kheap.cpp \
	kernel/memory/physicalalloc.cpp \
	kernel/memory/vm.cpp \
	kernel/prekernel/main.cpp \
	kernel/prekernel/start.S \
	kernel/prekernel/vector_table.S \
	kernel/syscall/syscalls.cpp \
	kernel/task/scheduler.cpp \

DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
DEPENDS:=$(DEPENDS) linker.ld 
DEPENDS:= $(addprefix $(SOURCES_ROOT), $(DEPENDS))
OBJS:=$(addsuffix .o, $(SOURCES))
OBJS:=$(addprefix $(SOURCES_ROOT), $(OBJS))

.PHONY: all clean qemu qemu-gdb

all: Kernel

qemu: Kernel
	$(QEMU) $(QEMU_FLAGS)

qemu-record: Kernel
	$(QEMU) $(QEMU_FLAGS) $(QEMU_RECORD_FLAGS)

qemu-gdb: Kernel
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) $(QEMU_RECORD_FLAGS) -s -S

qemu-gdb-replay: Kernel
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S $(QEMU_REPLAY_FLAGS)

kernel7.img: Kernel
	$(OBJCOPY) Kernel -O binary kernel7.img

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
