#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <api/syscalls.h>
#include "flanterm/flanterm.h"
#include "flanterm/backends/fb.h"


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define STDIN_FDPOS         0
#define STDOUT_STDERR_FDPOS 1

static struct flanterm_context *ft_ctx;
static int display_fd;


static void terminal_callback(struct flanterm_context *ctx, void *data, uint64_t type, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)ctx;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    char reply[32];
    int fd = (int) data;

    switch (type) {
        case FLANTERM_CB_POS_REPORT: {
            sys_write(fd, reply, snprintf(reply, sizeof(reply), "\x1b[%llu;%lluR", arg2, arg1));
            break;
        }
        /*
        case FLANTERM_CB_DEC:
            printf("TERM_CB_DEC");
            goto values;
        case FLANTERM_CB_MODE:
            printf("TERM_CB_MODE");
            goto values;
        case FLANTERM_CB_LINUX:
            printf("TERM_CB_LINUX");
            values:
                printf("(count=%llu, values={", arg1);
                for (uint64_t i = 0; i < arg1; i++) {
                    printf(i == 0 ? "%lu" : ", %lu", ((uint32_t*)((uint32_t) arg2 & 0xffffffff))[i]);
                }
                printf("}, final='%c')\n", (int)arg3);
                break;
        case FLANTERM_CB_BELL:
            printf("TERM_CB_BELL()\n");
            break;
        case FLANTERM_CB_PRIVATE_ID: printf("TERM_CB_PRIVATE_ID()\n"); break;
        case FLANTERM_CB_STATUS_REPORT: printf("TERM_CB_STATUS_REPORT()\n"); break;
        
        case FLANTERM_CB_KBD_LEDS:
            printf("TERM_CB_KBD_LEDS(state=");
            switch (arg1) {
                case 0: printf("CLEAR_ALL"); break;
                case 1: printf("SET_SCRLK"); break;
                case 2: printf("SET_NUMLK"); break;
                case 3: printf("SET_CAPSLK"); break;
            }
            printf(")\n");
            break;
        */
        default:
            printf("Unknown callback type %llu: %llx, %llx, %llx\n", type, arg1, arg2, arg3);
            break;
    }
}

static int open_framebuffer_display(const char *path)
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

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        f, fbinfo.width, fbinfo.height, fbinfo.pitch,
        8, 16, 8, 8, 8, 0,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );
    if (!ft_ctx) {
        fprintf(stderr, "flanterm_fb_init() failed\n");
        goto cleanup;
    }
    

    display_fd = fd;

    return 0;

cleanup:
    return rc;
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    int rc = 0;
    int count;
    PollFd fds[2];
    char buf[1024];
    int stdin_sender, stdin_receiver;
    int stdout_stderr_sender, stdout_stderr_receiver;

    /* Create pipes for IPC between shell and wm */
    if (sys_mkpipe(&stdin_sender, &stdin_receiver) < 0) {
        fprintf(stderr, "sys_mkpipe() for stdin failed\n");
        exit(-1);
    }
    if (sys_mkpipe(&stdout_stderr_sender, &stdout_stderr_receiver) < 0) {
        fprintf(stderr, "sys_mkpipe() for stdout+stderr failed\n");
        exit(-1);
    }

    /* Setup the display and terminal emulator */
    if (0 != (rc = open_framebuffer_display("/dev/framebuffer0"))) {
        fprintf(stderr, "framebuffer_open() failed\n");
        exit(-1);
    }
    flanterm_set_autoflush(ft_ctx, true);
    flanterm_set_callback(ft_ctx, terminal_callback, (void*) stdin_sender);

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

    fds[STDIN_FDPOS].fd = sys_open("/dev/ttyS0", OF_RDONLY, 0);
    if (fds[STDIN_FDPOS].fd < 0) {
        fprintf(stderr, "Failed to open /dev/ttyS0\n");
        exit(-1);
    }
    fds[STDIN_FDPOS].events = F_POLLIN;
    fds[STDOUT_STDERR_FDPOS].fd = stdout_stderr_receiver;
    fds[STDOUT_STDERR_FDPOS].events = F_POLLIN;

    while (true) {
        int updated = sys_poll(fds, ARRAY_SIZE(fds), 100000);

        if (updated < 0 && updated != -ERR_TIMEDOUT) {
            fprintf(stderr, "sys_poll() failed: %d\n", updated);
            exit(-1);
        }

        if (updated >= 0) {
            switch (updated) {
                case STDIN_FDPOS: {
                    count = sys_read(fds[updated].fd, buf, sizeof(buf));

                    sys_write(stdin_sender, buf, count);
                    break;
                }

                case STDOUT_STDERR_FDPOS: {
                    count = sys_read(fds[updated].fd, buf, sizeof(buf));
                    if (count < 0) {
                        fprintf(stderr, "Failed to read from stdout/stderr\n");
                        continue;
                    }
                    flanterm_write(ft_ctx, buf, count);
                    break;
                }

                default: {
                    fprintf(stderr, "Unexpected fd from sys_select(): %d\n", updated);
                    break;
                }
            }
        }

        sys_ioctl(display_fd, FBIO_REFRESH, 0);
    }
}
