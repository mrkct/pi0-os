#include <stdio.h>
#include "libdatetime.h"


int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    int i = 0;
    while(1) {
        printf("%d: Yes\n", i++);
        os_sleep(1000);
    }
    
    return 0;
}
