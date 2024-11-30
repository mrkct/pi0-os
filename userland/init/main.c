#include <stdio.h>
#include <api/syscalls.h>


void wait(int seconds)
{
    syscall(SYS_Sleep, seconds, 0, 0, 0, 0, 0);    
}

int main(int argc, char **argv)
{
    printf("Starting init...\n");
    while (1) {
        printf("tick\n");
        wait(1);
    }

    return 0;
}
