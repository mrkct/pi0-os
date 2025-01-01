#include <stdio.h>
#include <stdlib.h>
#include <api/syscalls.h>


int main(int argc, char **argv)
{
    printf("Starting init...\n");

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    while (1) {
        printf("tick\n");
        sys_millisleep(1000);
    }

    return 0;
}
