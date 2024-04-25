#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


bool print_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return false;
    }
    char buffer[1024 + 1];
    ssize_t bytes;
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
            printf("cat: Failed to open '%s'\n", argv[i]);
        }
    }

    return 0;
}  