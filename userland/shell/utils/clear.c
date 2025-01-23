#include <stdio.h>


int clear_main(int argc, const char *argv[])
{
    (void) argc;
    (void) argv;

    // Clear the screen and move the cursor to the top left corner
    printf("\033[2J\033[H");

    return 0;
}
