CC=arm-none-eabi-gcc
AS=arm-none-eabi-as
LD=arm-none-eabi-ld

BSP_HERE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CFLAGS := -g \
	-fno-unwind-tables \
	-T $(BSP_HERE)/bsp/linker.ld \
	-mcpu=arm1176jzf-s \
	--freestanding \
	-static \
	-fno-pic \
	-I$(BSP_HERE)/../api

include $(BSP_HERE)/bsp/objects.mk
BSP_OBJECTS:=$(addprefix $(BSP_HERE)/bsp/, $(BSP_OBJECTS))

.PHONY: bsp

bsp:
	$(MAKE) -C $(BSP_HERE)/bsp all
