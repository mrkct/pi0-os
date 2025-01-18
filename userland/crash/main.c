#include <stdio.h>


int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    printf("Goodbye :(\n");

    int *x = (int*) 0xfffffffc;
    *x = 1234;

    printf("Uh oh, this should not be printed\n");

    return 0;
}
