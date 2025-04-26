#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/termios.h>

#include <api/syscalls.h>
#include "flanterm/flanterm.h"
#include "flanterm/backends/fb.h"


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define KEYBOARD_FDPOS  0
#define PROCESS_FDPOS   1

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
        case FLANTERM_CB_BELL:
            break;
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

static void open_pty(int *ptym, int *ptys)
{
    *ptym = sys_open("/dev/pty/ptmx", OF_RDWR, 0);
    if (*ptym < 0) {
        fprintf(stderr, "Failed to open /dev/pty/ptmx\n");
        sys_exit(-1);
    }

    int slaveid = sys_ioctl(*ptym, PTYIO_GETSLAVE, NULL);
    if (slaveid < 0) {
        fprintf(stderr, "Failed to get slave id\n");
        sys_exit(-1);
    }

    char buf[32];
    snprintf(buf, sizeof(buf) - 1, "/dev/pty/pty%d", slaveid);
    *ptys = sys_open(buf, OF_RDWR, 0);
    if (*ptys < 0) {
        fprintf(stderr, "Failed to open %s\n", buf);
        sys_exit(-1);
    }
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    int rc = 0;
    int count;
    PollFd fds[2];
    char buf[1024];
    int ptym, ptys;

    open_pty(&ptym, &ptys);

    /* Setup the display and terminal emulator */
    if (0 != (rc = open_framebuffer_display("/dev/framebuffer0"))) {
        fprintf(stderr, "framebuffer_open() failed\n");
        exit(-1);
    }
    flanterm_set_autoflush(ft_ctx, true);
    flanterm_set_callback(ft_ctx, terminal_callback, (void*) ptym);

    /* Spawn the shell process */
    if (0 == sys_fork()) {
        sys_dup2(ptys, STDIN_FILENO);
        sys_dup2(ptys, STDOUT_FILENO);
        sys_dup2(ptys, STDERR_FILENO);

        /* Close to avoid leaking fds */
        sys_close(ptym);
        sys_close(ptys);

        const char *argv[] = { "/bina/shell", NULL };
        const char *envp[] = { NULL };
        sys_execve("/bina/shell", argv, envp);
        fprintf(stderr, "execve() failed\r\n");
        sys_exit(-1);
    }
    sys_close(ptys);

    fds[KEYBOARD_FDPOS].events = F_POLLIN;
    fds[KEYBOARD_FDPOS].fd = sys_open("/dev/ttyS0", OF_RDONLY, 0);
    if (fds[KEYBOARD_FDPOS].fd < 0) {
        fprintf(stderr, "Failed to open /dev/ttyS0\n");
        exit(-1);
    }
    struct termios termios = {};
    tcsetattr(fds[KEYBOARD_FDPOS].fd, 0, &termios);

    fds[PROCESS_FDPOS].events = F_POLLIN;
    fds[PROCESS_FDPOS].fd = ptym;

    while (true) {
        int updated = sys_poll(fds, ARRAY_SIZE(fds), 100000);

        if (updated < 0 && updated != -ERR_TIMEDOUT) {
            fprintf(stderr, "sys_poll() failed: %d\n", updated);
            exit(-1);
        }

        if (updated >= 0) {
            switch (updated) {
                case KEYBOARD_FDPOS: {
                    count = sys_read(fds[updated].fd, buf, sizeof(buf));
                    sys_write(ptym, buf, count);
                    break;
                }

                case PROCESS_FDPOS: {
                    count = sys_read(fds[updated].fd, buf, sizeof(buf));
                    if (count < 0) {
                        fprintf(stderr, "Failed to read from stdout/stderr\n");
                        continue;
                    }
                    buf[count] = '\0';
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
