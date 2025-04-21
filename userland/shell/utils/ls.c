#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>


int ls_main(int argc, const char *argv[])
{
    struct DIR *dir;
    struct dirent *entry;

    if (argc != 2) {
        fprintf(stderr, "ls: Invalid number of arguments.\n");
        return -1;
    }

    dir = opendir(argv[1]);
    if (dir == NULL) {
        fprintf(stderr, "ls: Cannot open directory '%s'\n", argv[1]);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip the "." and ".." entries
        if (entry->d_name[0] == '.' && 
            (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        printf("%s\n", entry->d_name);
    }

    closedir(dir);

    return 0;
}
