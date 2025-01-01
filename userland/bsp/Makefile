include ../Toolchain.mk

OBJECTS = \
	crt0.c.o \
	newlib.c.o \
	default_font.c.o \
	libdatetime.c.o \
	libgfx.c.o \
	libsstring.c.o

.PHONY: all clean

all: libbsp.a

libbsp.a: $(OBJECTS)
	$(AR) rcs libbsp.a $(OBJECTS)

%.c.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	$(RM) $(OBJECTS)
	$(RM) libapp.a
