#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>


int ls_main(int argc, const char *argv[])
{
    struct DIR *dir;
    struct dirent *entry;

    const char *path = ".";
    if (argc >= 2)
        path = argv[1];

    dir = opendir(path);
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

        if (entry->d_type == DT_DIR) {
            printf("\x1b[31m%s\x1b[0m\n", entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);

    return 0;
}
