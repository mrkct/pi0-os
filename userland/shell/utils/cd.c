#include <stdio.h>
#include <unistd.h>


int cd_main(int argc, const char *argv[])
{
    const char *dir = "/";
    if (argc >= 2)
        dir = argv[1];

    chdir(dir);

    return 0;
}
