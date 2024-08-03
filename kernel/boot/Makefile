SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-fpic -nostdlib -ffreestanding -lgcc
CXXFLAGS+=-MMD -MP -I../../
CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

BOARD:=raspi0
-include board/$(BOARD)/deps.mk
CXXFLAGS+=$(EXTRA_FLAGS)
LINKER_SCRIPT:=board/$(BOARD)/linker.ld

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
	$(CXX) -c $(CXXFLAGS) -o bundle.cpp.o bundle.cpp
	$(CXX) -T $(LINKER_SCRIPT) -o boot.elf $(OBJS) $(CXXFLAGS) -lgcc

-include $(DEPENDS)

%.cpp.o: %.cpp Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

%.S.o: %.S Makefile
	$(CXX) $< -c -o $@ $(CXXFLAGS)

clean:
	$(RM) -f $(OBJS) $(DEPENDS)
	$(RM) -f boot.elf
