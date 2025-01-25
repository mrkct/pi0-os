#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <api/syscalls.h>
#include <libgfx/libgfx.h>
#include <libutil/moretime.h>

#include "view.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define STDIN_FDPOS         0
#define STDOUT_STDERR_FDPOS 1


static int framebuffer_refresh(Display *display)
{
    return sys_ioctl((int) display->opaque, FBIO_REFRESH, 0);
}

static int open_framebuffer_display(const char *path, Display *display)
{
    int rc = 0, fd = 0;
    FramebufferDisplayInfo fbinfo;
    uint32_t *f;

    fd = sys_open(path, OF_RDWR, 0);
    if (fd < 0) {
        fprintf(stderr, "Failed to open framebuffer '%s'\n", path);
        return -1;
    }

    if (0 != (rc = sys_ioctl(fd, FBIO_GET_DISPLAY_INFO, &fbinfo))) {
        fprintf(stderr, "sys_ioctl(FBIO_GET_DISPLAY_INFO) failed\n");
        goto cleanup;
    }

    // This is completely arbitrary...
    f = (uint32_t*) 0x80000000;
    if (0 != (rc = sys_mmap(fd, f, fbinfo.pitch * fbinfo.height, 0))) {
        fprintf(stderr, "sys_ioctl(FBIO_MAP) failed\n");
        goto cleanup;
    }

    *display = (Display) {
        .framebuffer = f,
        .pitch = fbinfo.pitch,
        .width = fbinfo.width,
        .height = fbinfo.height,
        .opaque = (void*) fd,
        .ops = {
            .refresh = framebuffer_refresh,
        }
    };

    return 0;

cleanup:
    return rc;
}

static int open_framebuffer_view(const char *path, struct View *view)
{
    Display *display = malloc(sizeof(Display));
    if (display == NULL) {
        fprintf(stderr, "Failed to allocate display\n");
        return -1;
    }

    if (open_framebuffer_display(path, display)) {
        fprintf(stderr, "Failed to open framebuffer '%s'\n", path);
        return -1;
    }

    view_init(view, display);
    return 0;
}

static void on_terminal_response(int process_fd, const char *response)
{
    sys_write(process_fd, response, strlen(response));
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    int rc = 0;
    int count;
    PollFd fds[2];
    char buf[1024];
    struct DateTime datetime;
    Clock clock;
    View view;
    int stdin_sender, stdin_receiver;
    int stdout_stderr_sender, stdout_stderr_receiver;

    /* Some initialization */
    if (0 != (rc = clock_init(&clock))) {
        fprintf(stderr, "clock_init() failed\n");
        exit(-1);
    }
    if (0 != (rc = open_framebuffer_view("/dev/framebuffer0", &view))) {
        fprintf(stderr, "framebuffer_open() failed\n");
        exit(-1);
    }


    /* Create pipes for IPC between shell and wm */
    if (sys_mkpipe(&stdin_sender, &stdin_receiver) < 0) {
        fprintf(stderr, "sys_mkpipe() for stdin failed\n");
        exit(-1);
    }
    if (sys_mkpipe(&stdout_stderr_sender, &stdout_stderr_receiver) < 0) {
        fprintf(stderr, "sys_mkpipe() for stdout+stderr failed\n");
        exit(-1);
    }

    view_set_terminal_response_cb(&view, on_terminal_response, stdin_sender);

    /* Spawn the shell process */
    if (0 == sys_fork()) {
        sys_dup2(stdin_receiver, STDIN_FILENO);
        sys_dup2(stdout_stderr_sender, STDOUT_FILENO);
        sys_dup2(stdout_stderr_sender, STDERR_FILENO);

        /* Close the other ends of the pipes, we don't want to leak them*/
        sys_close(stdin_sender);
        sys_close(stdout_stderr_receiver);

        const char *argv[] = { "/bina/shell", NULL };
        const char *envp[] = { NULL };
        sys_execve("/bina/shell", argv, envp);
        fprintf(stderr, "execve() failed\r\n");
        sys_exit(-1);
    }

    fds[STDIN_FDPOS].fd = sys_open("/dev/uart0", OF_RDONLY, 0);
    if (fds[STDIN_FDPOS].fd < 0) {
        fprintf(stderr, "Failed to open /dev/uart0\n");
        exit(-1);
    }
    fds[STDIN_FDPOS].events = F_POLLIN;
    fds[STDOUT_STDERR_FDPOS].fd = stdout_stderr_receiver;
    fds[STDOUT_STDERR_FDPOS].events = F_POLLIN;

    while (true) {
        int updated = sys_poll(fds, ARRAY_SIZE(fds), 1000);

        /* Always update the clock */
        if (clock_get_datetime(&clock, &datetime) == 0) {
            view_update_clock(&view, &datetime);
        }

        if (updated == -ERR_TIMEDOUT) {
            view_idle_tick(&view);
        }

        if (updated < 0 && updated != -ERR_TIMEDOUT) {
            fprintf(stderr, "sys_poll() failed: %d\n", updated);
            exit(-1);
        }

        if (updated >= 0) {
            switch (updated) {
                case STDIN_FDPOS: {
                    count = sys_read(fds[updated].fd, buf, sizeof(buf));
                    if (count > 0) {
                        /* Convert '\r' to '\n\r' */
                        for (int i = 0; i < count; i++) {
                            if (buf[i] == '\r') {
                                sys_write(stdin_sender, "\n\r", 2);
                            } else {
                                sys_write(stdin_sender, buf + i, 1);
                            }
                        }
                    }
                    break;
                }

                case STDOUT_STDERR_FDPOS: {
                    count = sys_read(fds[updated].fd, buf, sizeof(buf));
                    if (count < 0) {
                        fprintf(stderr, "Failed to read from stdout/stderr\n");
                        continue;
                    }

                    /* Convert '\n' to '\r\n' */
                    for (int i = 0; i < count; i++) {
                        if (buf[i] == '\n') {
                            view_terminal_write(&view, "\r\n", 2);
                        } else {
                            view_terminal_write(&view, buf + i, 1);
                        }
                    }

                    break;
                }

                default: {
                    fprintf(stderr, "Unexpected fd from sys_select(): %d\n", updated);
                    break;
                }
            }
        }

        view_refresh_display(&view);
    }
}
