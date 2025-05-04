CC=arm-none-eabi-gcc
AS=arm-none-eabi-as
LD=arm-none-eabi-ld
AR=arm-none-eabi-ar

BSP_HERE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CFLAGS := -g \
	-fno-unwind-tables \
	-T $(BSP_HERE)/bsp/linker.ld \
	-nostdlib \
	--freestanding \
	-static \
	-fno-pic \
	-I$(BSP_HERE)/../include \
	-I$(BSP_HERE)/bsp \
	-L$(BSP_HERE)/bsp \

ifeq ($(ARCH), ARMV6)
	CFLAGS+=-mcpu=arm1176jzf-s -DCONFIG_ARMV6
else
	CFLAGS+=-mcpu=cortex-a7 -DCONFIG_ARMV7
endif
