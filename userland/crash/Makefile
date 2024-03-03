include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install

crash: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -o crash $(BSP_LIBS)

clean:
	$(RM) crash
	$(RM) $(OBJECTS)

install: crash
	cp crash $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
