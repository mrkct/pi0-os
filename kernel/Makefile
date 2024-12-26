SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-fpic -nostdlib -lgcc -lc -lg -lgcc -lstdc++ -lm
CXXFLAGS+=-MMD -MP -I$(SOURCES_ROOT) -I../include
CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

ifeq ($(ARCH), ARMV6)
	CXXFLAGS+=-mcpu=arm1176jzf-s -DCONFIG_ARMV6
else
	CXXFLAGS+=-mcpu=cortex-a7 -DCONFIG_ARMV7
endif

ARCH_SOURCES=\
	kernel/arch/arm/start.S 	 	\
	kernel/arch/arm/armirq.cpp   	\
	kernel/arch/arm/armv6mmu.cpp 	\
	kernel/arch/arm/irq.S

SOURCES=\
	$(ARCH_SOURCES) \
	kernel/kprintf.cpp \
	kernel/main.cpp \
	kernel/drivers/bus/virtio/virtio.cpp \
	kernel/drivers/char/bcm2835_aux_uart.cpp \
	kernel/drivers/char/bcm2835_gpio.cpp \
	kernel/drivers/char/pl011.cpp \
	kernel/drivers/char/pl031.cpp \
	kernel/drivers/char/virtioinput.cpp \
	kernel/drivers/block/ramdisk.cpp \
	kernel/drivers/block/virtioblk.cpp \
	kernel/drivers/irqc/bcm2835_irqc.cpp \
	kernel/drivers/irqc/gic2.cpp \
	kernel/drivers/timer/armv7timer.cpp \
	kernel/drivers/timer/bcm2835_systimer.cpp \
	kernel/drivers/device.cpp \
	kernel/drivers/devicemanager.cpp \
	kernel/lib/more_time.c \
	kernel/locking/irqlock.cpp \
	kernel/locking/mutex.cpp \
	kernel/locking/spinlock.cpp \
	kernel/memory/bootalloc.cpp \
	kernel/memory/kheap.cpp \
	kernel/memory/physicalalloc.cpp \
	kernel/memory/vm.cpp \
	kernel/task/elfloader.cpp \
	kernel/vfs/devfs/devfs.cpp \
	kernel/vfs/fat32/fat32.cpp \
	kernel/vfs/fs.cpp \
	kernel/vfs/vfs.cpp \
	kernel/irq.cpp \
	kernel/newlib.cpp \
	kernel/timer.cpp \
	kernel/scheduler.cpp \
	kernel/syscall.cpp \



DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.c, %.c.d, $(DEPENDS))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
DEPENDS:=$(DEPENDS) linker.ld 
DEPENDS:= $(addprefix $(SOURCES_ROOT), $(DEPENDS))
OBJS:=$(addsuffix .o, $(SOURCES))
OBJS:=$(addprefix $(SOURCES_ROOT), $(OBJS))

.PHONY: all clean qemu qemu-gdb 

all: kernel.elf bootloader 

bootloader: kernel.elf
	$(OBJCOPY) kernel.elf -O binary kernel.bin
	$(MAKE) -e -C boot

kernel.elf: $(OBJS)
	$(CXX) -T linker.ld -o kernel.elf $(OBJS) $(CXXFLAGS) -lgcc

-include $(DEPENDS)

%.cpp.o: %.cpp Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.c.o: %.c Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.S.o: %.S Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

clean:
	$(RM) -f $(OBJS) $(DEPENDS)
	$(RM) -f kernel.elf
	$(RM) -f kernel.bin
	$(MAKE) -e -C boot clean
