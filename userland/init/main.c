#include <stdio.h>
#include <stdlib.h>
#include <api/syscalls.h>


void wait(int seconds)
{
    syscall(SYS_MilliSleep, 1000 * seconds, 0, 0, 0, 0, 0);    
}

int main(int argc, char **argv)
{
    printf("Starting init...\n");

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    while (1) {
        printf("tick\n");
        wait(1);
    }

    return 0;
}
