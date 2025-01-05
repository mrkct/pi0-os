include ../Toolchain.mk

OBJECTS = \
	crt0.c.o \
	newlib.c.o \
	libdatetime.c.o \
	libgfx/default_font.c.o \
	libgfx/libgfx.c.o \
	libutil/moretime.c.o \
	libsstring.c.o

.PHONY: all clean

all: libbsp.a

libbsp.a: $(OBJECTS)
	$(AR) rcs libbsp.a $(OBJECTS)

%.c.o: %.c
	@echo "\033[36mCompiling $<\033[0m"
	$(CC) $(CFLAGS) -c $< -MMD -MP -MF $(@:.o=.d) -o $@

clean:
	$(RM) $(OBJECTS)
	$(RM) $(OBJECTS:.o=.d)
	$(RM) libapp.a

-include $(OBJECTS:.o=.d)
