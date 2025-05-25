#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


int touch_main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_CREAT | O_WRONLY | O_TRUNC);
        if (fd < 0)
            perror("touch");
        else
            close(fd);
    }

    return 0;
}
