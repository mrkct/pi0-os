include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install

all: clock

clock: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -lapp -lm -o clock 

clean:
	$(RM) clock
	$(RM) $(OBJECTS)

install: clock
	cp clock $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
