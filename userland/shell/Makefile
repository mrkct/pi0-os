include ../toolchain.mk 

OBJECTS = main.c.o utils/cat.c.o utils/echo.c.o utils/ls.c.o utils/mkdir.c.o utils/rm.c.o

.PHONY: clean install

shell: bsp $(OBJECTS)
	$(CC) $(CFLAGS) $(BSP_OBJECTS) $(OBJECTS) -lapp -o shell $(BSP_LIBS)

clean:
	$(RM) shell
	$(RM) $(OBJECTS)

install: shell
	cp shell $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
