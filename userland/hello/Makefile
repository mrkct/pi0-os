include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install

hello: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -o hello $(BSP_LIBS)

clean:
	$(RM) hello
	$(RM) $(OBJECTS)

install: hello
	cp hello $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
