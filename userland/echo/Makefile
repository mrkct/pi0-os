include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install

echo: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -o echo $(BSP_LIBS)

clean:
	$(RM) echo
	$(RM) $(OBJECTS)

install: echo
	cp echo $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
