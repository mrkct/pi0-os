#define __STRINGIFY(s) #s
#define STRINGIFY(s) __STRINGIFY(s)

asm(
".section .bundle.kernel, \"ao\" \n"
    ".incbin \"../kernel.bin\" \n"
);

#ifdef CONFIG_BUNDLED_DTB
asm(
".section .bundle.dtb, \"ao\" \n"
    ".incbin \"" STRINGIFY(CONFIG_BUNDLED_DTB) "\" \n"
);
#endif
