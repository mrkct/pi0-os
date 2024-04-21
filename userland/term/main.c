#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <api/syscalls.h>
#include "gfx_terminal.h"


#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#define READ_END        0
#define WRITE_END       1

#define ARRAY_SIZE(e) (sizeof(e) / sizeof((e)[0]))

#define EXIT_ON_FAIL(e)                                                                     \
    do {                                                                                    \
        int _retcode = (e);                                                                 \
        if (_retcode != 0) {                                                                \
            printf("FATAL: Expression %s failed with status code %d\n", #e, _retcode);      \
            exit(-1);                                                                       \
        }                                                                                   \
    } while(0)

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    gfx_terminal_init();

    int32_t kbd_fd = open("/sys/kbd", 0, O_RDONLY);

    int32_t stdin_fds[2];
    int32_t stdout_fds[2];
    int32_t stderr_fds[2];

    EXIT_ON_FAIL(create_pipe(stdin_fds));
    EXIT_ON_FAIL(create_pipe(stdout_fds));
    EXIT_ON_FAIL(create_pipe(stderr_fds));

    const char *args[] = {"/bina/shell"};
    int32_t descriptors[] = {stdin_fds[READ_END], stdout_fds[WRITE_END], stderr_fds[WRITE_END]};
    SpawnProcessConfig cfg = {
        .flags = 0,
        .args = args,
        .args_len = ARRAY_SIZE(args),
        .descriptors = descriptors,
        .descriptors_len = ARRAY_SIZE(descriptors)
    };
    PID shell_pid;
    EXIT_ON_FAIL(spawn_process("/bina/shell", &cfg, &shell_pid));

    while (1) {
        char buf[257];
        ssize_t size;

        while (0 != (size = read(stderr_fds[READ_END], buf, sizeof(buf) - 1))) {
            buf[size] = '\0';
            gfx_terminal_print(buf, COL_BLACK, COL_RED);
        }

        while (0 != (size = read(stdout_fds[READ_END], buf, sizeof(buf) - 1))) {
            buf[size] = '\0';
            gfx_terminal_print(buf, COL_BLUE, COL_WHITE);
        }

        size = 0;
        KeyEvent event;
        while(sizeof(event) == read(kbd_fd, &event, sizeof(event))) {
            if (event.pressed == false || event.character == '\0')
                continue;
            
            buf[size++] = event.character;
        }
        if (size > 0) {
            buf[size] = '\0';
            write(stdin_fds[WRITE_END], buf, size);
        }

        gfx_update_window();
    }

    return 0;
}
