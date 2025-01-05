#include <stdio.h>
#include <stdlib.h>
#include <api/syscalls.h>


int main(int argc, char **argv)
{
    printf("Starting init...\n");

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    printf("Starting 'wm'...\n");
    if (0 == sys_fork()) {
        const char *args[] = { "/bina/wm", NULL };
        const char *envp[] = { NULL };
        sys_execve("/bina/wm", args, envp);
        fprintf(stderr, "Failed to start 'wm'\n");
        sys_exit(1);
    }

    while (1) {
        printf("tick\n");
        sys_millisleep(1000);
        sys_yield();
    }

    return 0;
}
