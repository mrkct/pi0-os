include ../toolchain.mk
include objects.mk

.PHONY: all clean

all: $(BSP_OBJECTS)

clean:
	$(RM) $(BSP_OBJECTS)

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@