#include <stdint.h>
#include <api/arm/crt0util.h>


extern void __libc_init_array(void);
extern int main(int argc, char *argv[]);
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

void _cstart(ArmCrt0InitialStackState *sp)
{
    __libc_init_array();
    exit(main(sp->argc, sp->argv));
}
