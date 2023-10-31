include ../toolchain.mk 

OBJECTS = libdatetime.c.o libgfx.c.o

.PHONY: clean install

all: libapp.a

libapp.a: bsp $(OBJECTS)
	$(AR) rcs libapp.a $(OBJECTS)

clean:
	$(RM) libapp.a
	$(RM) $(OBJECTS)

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@
