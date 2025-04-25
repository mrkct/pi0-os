#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <api/syscalls.h>

#include "libsstring.h"

#define MAX_ARGS 16
#define MAX_LINE_LEN 256

extern int exit_main(int argc, const char *argv[]);
extern int echo_main(int argc, const char *argv[]);
extern int ls_main(int argc, const char *argv[]);
extern int cat_main(int argc, const char *argv[]);
extern int clear_main(int argc, const char *argv[]);
extern int cd_main(int argc, const char *argv[]);
extern int pwd_main(int argc, const char *argv[]);
extern int mkdir_main(int argc, const char *argv[]);
extern int rm_main(int argc, const char *argv[]);

static struct { const char *name; int (*main)(int argc, const char *argv[]); } builtins[] = {
    { "cat", cat_main },
    { "clear", clear_main },
    { "echo", echo_main },
    { "ls", ls_main },
    { "mkdir", mkdir_main },
    { "rm", rm_main },
};

static size_t tokenize(char *line);
static size_t argv_from_tokenized_line(const char *tokenized_line,
    size_t tokens, const char *argv[], size_t max_args);

static void read_line(const char *prompt, char *buf, size_t len)
{
    printf("%s", prompt);

    int count = sys_read(STDIN_FILENO, buf, len - 1);
    if (count < 0) {
        fprintf(stderr, "Failed to read line: %d", count);
        exit(-1);
    }
    buf[count] = '\0';
    char *c = strrchr(buf, '\n');
    if (c)
        *c = '\0';
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
    char procpath[256];
    char * const emptyenv[] = { NULL };
    int pid;
    (void) argc;

    if (argv[0][0] == '/') {
        strncpy(procpath, argv[0], sizeof(procpath));
    } else {
        strncpy(procpath, "/bina/", sizeof(procpath));
        strncat(procpath, argv[0], sizeof(procpath));
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork() failed: %d\n", pid);
        return -1;
    }

    if (pid == 0) {
        execve(procpath, (char *const *) argv, emptyenv);
        fprintf(stderr, "execv() failed: %d\n", pid);
        exit(-1);
    } 
    
    sys_waitexit(pid);
    return 0;
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    char line[MAX_LINE_LEN];
    const char *command_argv[MAX_ARGS];

    setvbuf(stdout, NULL, _IONBF, 0);
    
    while (1) {
        read_line("$ ", line, sizeof(line));
        size_t tokens = tokenize(line);
        size_t command_argc = argv_from_tokenized_line(line, tokens, command_argv, MAX_ARGS);

        if (tokens > 0) {
            if (run_builtin_command(command_argv[0], command_argc, command_argv)) {
                if (run_program(command_argc, command_argv)) {
                    fprintf(stderr, "error: Could not find '%s'\n", line);
                }
            }
        }
    }

    return 0;
}
