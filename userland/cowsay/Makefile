include ../toolchain.mk 

OBJECTS = cowsay.c.o

.PHONY: clean install

cowsay: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -o cowsay $(BSP_LIBS)

clean:
	$(RM) cowsay
	$(RM) $(OBJECTS)

install: cowsay
	cp cowsay $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
