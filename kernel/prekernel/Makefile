SOURCES_ROOT=../../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy
CXXFLAGS=-MMD -MP -mcpu=arm1176jzf-s -fpic -ffreestanding -nostdlib -lgcc -fno-exceptions
CXXFLAGS+=-I$(SOURCES_ROOT)
CXXFLAGS+=-Wall -Wextra -g

SOURCES=\
	kernel/device/gpio.cpp \
	kernel/device/uart.cpp \
	kernel/kprintf.cpp \
	kernel/prekernel/main.cpp \
	kernel/prekernel/start.S \
DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
DEPENDS:= $(addprefix $(SOURCES_ROOT), $(DEPENDS))
OBJS:=$(addsuffix .o, $(SOURCES))
OBJS:=$(addprefix $(SOURCES_ROOT), $(OBJS))

.PHONY: all clean qemu qemu-gdb

all: Prekernel

qemu: Prekernel
	qemu-system-arm -M raspi0 -serial mon:stdio -kernel Prekernel

qemu-gdb: Prekernel
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	qemu-system-arm -M raspi0 -serial mon:stdio -kernel Prekernel -s -S

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
