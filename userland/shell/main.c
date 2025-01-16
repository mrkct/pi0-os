#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <api/syscalls.h>
#include "libsstring.h"

#define MAX_ARGS 16

extern int exit_main(int argc, const char *argv[]);
extern int echo_main(int argc, const char *argv[]);
extern int ls_main(int argc, const char *argv[]);
extern int cat_main(int argc, const char *argv[]);
extern int cd_main(int argc, const char *argv[]);
extern int pwd_main(int argc, const char *argv[]);
extern int mkdir_main(int argc, const char *argv[]);
extern int rm_main(int argc, const char *argv[]);

static struct { const char *name; int (*main)(int argc, const char *argv[]); } builtins[] = {
    { "cat", cat_main },
    { "echo", echo_main },
    { "ls", ls_main },
    { "mkdir", mkdir_main },
    { "rm", rm_main },
};

static char *read_line(const char *prompt);
static size_t tokenize(char *line);
static size_t argv_from_tokenized_line(const char *tokenized_line,
    size_t tokens, const char *argv[], size_t max_args);

static char *read_line(const char *prompt)
{
    int rc;
    char c;
    char *line = malloc(8);
    size_t length = 0, allocated = 8;
    PollFd pollfds[1];
    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = F_POLLIN;
    
    printf("%s", prompt);

    do {
        rc = sys_poll(pollfds, 1, -1);
        if (rc < 0) {
            fprintf(stderr, "poll() failed: %d\n", rc);
            exit(-1);
        }
        sys_read(STDIN_FILENO, &c, 1);

        if (c == '\r')
            continue;

        putchar(c);

        bool is_eol = c == '\n';
        if (c < 0 || (is_eol && length == 0))
            continue;
        if (is_eol && length > 0)
            break;

        if (length == allocated - 1) {
            allocated += 8;
            line = realloc(line, allocated);
        }
        line[length++] = (char) c;
        line[length] = '\0';
    } while (true);

    return line;
}

static size_t tokenize(char *line)
{
    size_t count = 0;
    while (*line) {
        // Consume all leading whitespace
        while (*line == ' ')
            line++;
        if (*line == '\0')
            break;

        while (*line && *line != ' ')
            line++;
        
        count++;
        if (*line == ' ') {
            *line = '\0';
            line++;
        }
    }

    return count;
}

static size_t argv_from_tokenized_line(const char *tokenized_line,
    size_t tokens, const char *argv[], size_t max_args)
{
    size_t argc = tokens < max_args ? tokens : max_args;
    for (size_t i = 0; i < argc; i++) {
        argv[i] = tokenized_line;
        tokenized_line += strlen(tokenized_line) + 1;
    }
    argv[argc] = NULL;

    return argc;
}

static int run_builtin_command(const char *command, size_t argc, const char *argv[])
{
    (void) argc;
    (void) argv;

    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(builtins[i].name, command) == 0) {
            builtins[i].main(argc, argv);
            return 0;
        }
    }

    return -1;
}

static int run_program(size_t argc, const char *argv[])
{
    char * const emptyenv[] = { NULL };
    int pid, status;
    (void) argc;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork() failed: %d\n", pid);
        return -1;
    }

    if (pid == 0) {
        execve(argv[0], (char *const *) argv, emptyenv);
        fprintf(stderr, "execv() failed: %d\n", pid);
        exit(-1);
    } 
    
    waitpid(pid, &status, WNOHANG);
    return 0;
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    char *line = NULL;
    const char *command_argv[MAX_ARGS];

    setvbuf(stdout, NULL, _IONBF, 0);
    
    while (1) {
        line = read_line("$: ");
        size_t tokens = tokenize(line);
        size_t command_argc = argv_from_tokenized_line(line, tokens, command_argv, MAX_ARGS);

        if (tokens > 0) {
            if (run_builtin_command(command_argv[0], command_argc, command_argv)) {
                if (run_program(command_argc, command_argv)) {
                    fprintf(stderr, "error: Could not find '%s'\n", line);
                }
            }
        }

        free(line);
    }

    return 0;
}
