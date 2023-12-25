CC=arm-none-eabi-gcc
AS=arm-none-eabi-as
LD=arm-none-eabi-ld
AR=arm-none-eabi-ar

BSP_HERE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CFLAGS := -g -Wall -Wextra \
	-fno-unwind-tables \
	-T $(BSP_HERE)/bsp/linker.ld \
	-mcpu=arm1176jzf-s \
	-nostdlib \
	--freestanding \
	-static \
	-fno-pic \
	-I$(BSP_HERE)/../include \
	-I$(BSP_HERE)/libs \
	-L$(BSP_HERE)/libs \
	-L$(BSP_HERE)/bsp/newlib \

include $(BSP_HERE)/bsp/objects.mk
BSP_OBJECTS:=$(addprefix $(BSP_HERE)/bsp/, $(BSP_OBJECTS))

BSP_LIBS := -lc -lg -lgcc -lm

.PHONY: bsp

bsp:
	$(MAKE) -C $(BSP_HERE)/bsp all
