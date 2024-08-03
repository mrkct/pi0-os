SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-fpic -nostdlib -lgcc -lc -lg -lgcc -lm
CXXFLAGS+=-MMD -MP -I$(SOURCES_ROOT) -I../include
CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

ifeq ($(ARCH), ARMV6)
	CXXFLAGS+=-mcpu=arm1176jzf-s -DCONFIG_ARMV6
else
	CXXFLAGS+=-mcpu=cortex-a7 -DCONFIG_ARMV7
endif

ARCH_SOURCES=\
	kernel/arch/arm/start.S \
	kernel/arch/arm/armv6mmu.cpp

SOURCES=\
	$(ARCH_SOURCES) \
	kernel/main.cpp \

DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
DEPENDS:=$(DEPENDS) linker.ld 
DEPENDS:= $(addprefix $(SOURCES_ROOT), $(DEPENDS))
OBJS:=$(addsuffix .o, $(SOURCES))
OBJS:=$(addprefix $(SOURCES_ROOT), $(OBJS))

.PHONY: all clean qemu qemu-gdb 

all: boot/boot.elf

boot/boot.elf: kernel.elf
	$(OBJCOPY) kernel.elf -O binary kernel.bin
	$(MAKE) -e -C boot

kernel.elf: $(OBJS)
	$(CXX) -T linker.ld -o kernel.elf $(OBJS) $(CXXFLAGS) -lgcc

-include $(DEPENDS)

%.cpp.o: %.cpp Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.S.o: %.S Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

clean:
	$(RM) -f $(OBJS) $(DEPENDS)
	$(RM) -f kernel.elf
	$(RM) -f kernel.bin
	$(MAKE) -e -C boot clean
