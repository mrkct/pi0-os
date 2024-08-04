SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-fpic -nostdlib -ffreestanding -lgcc
CXXFLAGS+=-MMD -MP -I../../
CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

-include board/$(BOARD)/deps.mk

ifeq ($(ARCH), ARMV6)
	CXXFLAGS+=-mcpu=arm1176jzf-s -DCONFIG_ARMV6
else
	CXXFLAGS+=-mcpu=cortex-a7 -DCONFIG_ARMV7
endif

BOARD_SOURCES:=$(addprefix board/$(BOARD)/, $(BOARD_SOURCES))
SOURCES=$(BOARD_SOURCES) start.S bundle.cpp boot.cpp misc.cpp
DEPENDS:=$(patsubst %.cpp, %.cpp.d, $(SOURCES))
DEPENDS:=$(patsubst %.S, %.S.d, $(DEPENDS))
# DEPENDS:=$(DEPENDS) linker.ld 
OBJS:=$(addsuffix .o, $(SOURCES))


.PHONY: all clean o

all: boot.elf

boot.elf: $(OBJS) ../kernel.elf
	$(RM) kernel.S.o
	$(CXX) -c $(CXXFLAGS) -o bundle.cpp.o bundle.cpp -DCONFIG_BUNDLED_DTB=$(CONFIG_BUNDLED_DTB)
	$(RM) linker.preprocessed.ld
	$(CXX) -E -P -x c -o linker.preprocessed.ld linker.ld -DLOAD_ADDRESS=$(LOAD_ADDRESS)
	$(CXX) -T linker.preprocessed.ld -o boot.elf $(OBJS) -DLOAD_ADDRESS=$(LOAD_ADDRESS) $(CXXFLAGS) -lgcc

-include $(DEPENDS)

%.cpp.o: %.cpp Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.S.o: %.S Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

clean:
	$(RM) -f $(OBJS) $(DEPENDS)
	$(RM) -f boot.elf
	$(RM) linker.preprocessed.ld
