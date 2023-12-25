include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install clock

all: clock

clock: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -lapp -o clock $(BSP_LIBS) 

clean:
	$(RM) clock
	$(RM) $(OBJECTS)

install: clock
	cp clock $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
