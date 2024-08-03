#define __STRINGIFY(s) #s
#define STRINGIFY(s) __STRINGIFY(s)

asm(
".section .bundle, \"ao\" \n"
    ".incbin \"../kernel.bin\" \n"
);
