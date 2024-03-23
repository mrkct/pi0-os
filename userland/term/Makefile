include ../toolchain.mk 

OBJECTS = main.c.o gfx_terminal.c.o

.PHONY: clean install

term: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -lapp -o term $(BSP_LIBS)

clean:
	$(RM) term
	$(RM) $(OBJECTS)

install: term
	cp term $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
