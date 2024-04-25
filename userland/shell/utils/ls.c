#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "api/files.h"


int ls_main(int argc, const char *argv[])
{
    if (argc != 2) {
        printf("ls: Invalid number of arguments.\n");
        return -1;
    }

    int fd = open(argv[1], 0, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ls: Could not open %s\n", argv[1]);
        return -1;
    }

    DirectoryEntry dirent;
    while (sys_read_direntry(fd, &dirent, 1) > 0) {
        char ctype;
        switch (dirent.filetype) {
        case RegularFile: ctype = 'f'; break;
        case Directory: ctype = 'd'; break;
        case CharacterDevice: ctype = 'c'; break;
        case Pipe: ctype = 'p'; break;
        default: ctype = '?'; break;
        }
        printf("[%c] %s %llu\n", ctype, dirent.name, dirent.size);
    }

    return 0;
}
