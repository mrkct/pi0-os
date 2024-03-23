include ../toolchain.mk 

OBJECTS = main.c.o

.PHONY: clean install

yes: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -lapp -o yes $(BSP_LIBS)

clean:
	$(RM) yes
	$(RM) $(OBJECTS)

install: yes
	cp yes $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
