#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


bool print_file(const char *filename)
{
    FILE *fp;
    char buffer[1024 + 1];
    ssize_t bytes;
    
    fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;
    }
    
    while ((bytes = fread(buffer, 1, sizeof(buffer) - 1, fp)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }

    fclose(fp);
    return true;
}

int cat_main(int argc, const char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (!print_file(argv[i])) {
            fprintf(stderr, "cat: Failed to open '%s'\n", argv[i]);
        }
    }

    return 0;
}  