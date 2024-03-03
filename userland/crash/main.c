#include <stdio.h>


int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    printf("Goodbye :(\n");

    int *x = (int*) 0xbffffffc;
    *x = 1234;

    return 0;
}
