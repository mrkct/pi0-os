SOURCES_ROOT=../../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy
CXXFLAGS=-MMD -MP -mcpu=arm1176jzf-s -fpic -ffreestanding -nostdlib -lgcc -fno-exceptions -fno-rtti -std=c++2a
CXXFLAGS+=-I$(SOURCES_ROOT)
CXXFLAGS+=-Wall -Wextra -g

QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp -M raspi0 -serial mon:stdio -kernel Prekernel -sd $(SOURCES_ROOT)/disks/test.sd

LIBS_SOURCES=\
	kernel/lib/libc/string.cpp \
	kernel/lib/memory.cpp \
	kernel/lib/psf/default_font.cpp \
	kernel/lib/psf/psf.cpp

SOURCES=\
	$(LIBS_SOURCES) \
	kernel/device/gpio.cpp \
	kernel/device/sd.cpp \
	kernel/device/systimer.cpp \
	kernel/device/uart.cpp \
	kernel/device/videoconsole.cpp \
	kernel/device/videocore.cpp \
	kernel/interrupt.cpp \
	kernel/kprintf.cpp \
	kernel/memory/mmu.cpp \
	kernel/memory/pagealloc.cpp \
	kernel/prekernel/main.cpp \
	kernel/prekernel/start.S \
	kernel/prekernel/vector_table.S

DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
DEPENDS:= $(addprefix $(SOURCES_ROOT), $(DEPENDS))
OBJS:=$(addsuffix .o, $(SOURCES))
OBJS:=$(addprefix $(SOURCES_ROOT), $(OBJS))

.PHONY: all clean qemu qemu-gdb

all: Prekernel

qemu: Prekernel
	qemu-system-arm $(QEMU_FLAGS)

qemu-gdb: Prekernel
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	qemu-system-arm $(QEMU_FLAGS) -s -S

kernel7.img: Prekernel
	$(OBJCOPY) Prekernel -O binary kernel7.img

Prekernel: $(OBJS)
	$(CXX) -T linker.ld -o Prekernel $(OBJS) $(CXXFLAGS) -lgcc

-include $(DEPENDS)

%.cpp.o: %.cpp Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.S.o: %.S Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

clean:
	$(RM) -f $(OBJS) $(DEPENDS)
	$(RM) -f Prekernel
	$(RM) -f Prekernel.elf
