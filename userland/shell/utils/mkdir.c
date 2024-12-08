#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>


int mkdir_main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dirname>\n", argv[0]);
        return EXIT_FAILURE;
    }

    mkdir(argv[1], 0755);
    return EXIT_SUCCESS;
}
