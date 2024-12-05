include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install

init: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -o init $(BSP_LIBS)

clean:
	$(RM) init
	$(RM) $(OBJECTS)

install: init
	cp init $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
