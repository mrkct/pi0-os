#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <api/syscalls.h>
#include <libgfx/libgfx.h>
#include <libutil/moretime.h>

#include "view.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))


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

    if (0 != (rc = sys_ioctl(fd, FBIO_MAP, &f))) {
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

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    int rc = 0;
    int count;
    PollFd fds[3];
    char buf[1024];
    struct DateTime datetime;
    Clock clock;
    View view;
    int stdin_sender, stdin_receiver;
    int stdout_sender, stdout_receiver;
    int stderr_sender, stderr_receiver;

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
    if (sys_mkpipe(&stdout_sender, &stdout_receiver) < 0) {
        fprintf(stderr, "sys_mkpipe() for stdout failed\n");
        exit(-1);
    }
    if (sys_mkpipe(&stderr_sender, &stderr_receiver) < 0) {
        fprintf(stderr, "sys_mkpipe() for stderr failed\n");
        exit(-1);
    }

    /* Spawn the shell process */
    if (0 == sys_fork()) {
        sys_close(STDIN_FILENO);
        sys_close(STDOUT_FILENO);
        sys_close(STDERR_FILENO);
        sys_movefd(stdin_receiver, STDIN_FILENO);
        sys_movefd(stdout_sender, STDOUT_FILENO);
        sys_movefd(stderr_sender, STDERR_FILENO);
        sys_execve("/bin/shell", NULL, NULL);
        fprintf(stderr, "execve() failed\n");
        sys_exit(-1);
    }

    fds[STDIN_FILENO].fd = sys_open("/dev/uart0", OF_RDONLY, 0);
    if (fds[STDIN_FILENO].fd < 0) {
        fprintf(stderr, "Failed to open /dev/uart0\n");
        exit(-1);
    }
    fds[STDIN_FILENO].events = F_POLLIN;
    fds[STDOUT_FILENO].fd = stdout_receiver;
    fds[STDOUT_FILENO].events = F_POLLOUT;
    fds[STDERR_FILENO].fd = stderr_receiver;
    fds[STDERR_FILENO].events = F_POLLOUT;

    while (true) {
        int updated = sys_poll(fds, ARRAY_SIZE(fds), 1000);

        /* Always update the clock */
        if (clock_get_datetime(&clock, &datetime) == 0) {
            view_update_clock(&view, &datetime);
        }

        if (updated < 0) {
            if (updated == -ERR_TIMEDOUT) {
                continue;
            } else {
                fprintf(stderr, "sys_select() failed: %d\n", updated);
                exit(-1);
            }
        }

        switch (updated) {
            case STDIN_FILENO: {
                count = sys_read(fds[updated].fd, buf, sizeof(buf));
                if (count > 0) {
                    sys_write(stdin_sender, buf, count);
                }
                break;
            }

            case STDOUT_FILENO:
            case STDERR_FILENO: {
                count = sys_read(fds[updated].fd, buf, sizeof(buf));
                if (count < 0) {
                    fprintf(stderr, "Failed to read from stdout/stderr\n");
                    continue;
                }

                view_terminal_write(&view, buf, count);
                break;
            }

            default: {
                fprintf(stderr, "Unexpected fd from sys_select(): %d\n", updated);
                break;
            }
        }

        view_refresh_display(&view);
    }
}
