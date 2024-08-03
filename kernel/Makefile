SOURCES_ROOT=../

CXX=arm-none-eabi-g++
OBJCOPY=arm-none-eabi-objcopy

CXXFLAGS=-fpic -nostdlib -lgcc -lc -lg -lgcc -lm
CXXFLAGS+=-MMD -MP -I$(SOURCES_ROOT) -I../include
CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics -fno-rtti -std=c++2a
CXXFLAGS+=-Wall -Wextra -g

BOARD:=raspi0
ifeq ($(BOARD), raspi0)
	CXXFLAGS+=-mcpu=arm1176jzf-s -DCONFIG_ARMV6
else
	CXXFLAGS+=-mcpu=cortex-a7 -DBOARD_RASPI2
endif

ifdef RAMDISK
	CXXFLAGS+=-DRAMDISK_FILE=$(RAMDISK)
endif

QEMU:=qemu-system-arm
QEMU_FLAGS:=-d mmu,cpu_reset,guest_errors,unimp -M $(BOARD) -serial null -serial stdio -kernel boot/boot.elf
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
	$(MAKE) -C boot

snapshot-drive.qcow2:
	qemu-img create -f qcow2 snapshot-drive.qcow2 4G

qemu: all
	$(QEMU) $(QEMU_FLAGS) $(QEMU_SD_FLAGS)

qemu-record: all snapshot-drive.qcow2
	$(QEMU) $(QEMU_FLAGS) $(QEMU_RECORD_FLAGS)

qemu-gdb: all
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) $(QEMU_SD_FLAGS) -s -S

qemu-gdb-replay: all snapshot-drive.qcow2
	@echo "Run 'gdb' in another terminal and type 'target remote localhost:1234'"
	$(QEMU) $(QEMU_FLAGS) -s -S $(QEMU_REPLAY_FLAGS)

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
	$(RM) -f kernel7.img
