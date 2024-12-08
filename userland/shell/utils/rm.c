#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int rm_main(int argc, char *argv[])
{
    int rc;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    rc = unlink(argv[1]);
    if (rc < 0) {
        perror("unlink");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
