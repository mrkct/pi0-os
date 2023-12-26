#include <stdint.h>


extern void __libc_init_array(void);
extern int main(int argc, const char *argv[]);
extern void exit(int status);

void _init()
{
}

void _fini()
{
}

asm (
    ".global _start \n"
    "_start: \n"
    "  mov r0, sp \n"
    "  b _cstart \n"
);

void _cstart(void *sp)
{
    int argc = *(int32_t*)sp;
    sp += sizeof(int32_t);

    const char **argv = *(const char ***)sp;

    __libc_init_array();

    exit(main(argc, argv));
}
