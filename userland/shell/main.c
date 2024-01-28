#include <stdio.h>
#include <stdbool.h>
#include <api/syscalls.h>
#include "libterm.h"
#include "libsstring.h"

#define MAX_ARGS 16

static size_t tokenize(char *line);
static size_t argv_from_tokenized_line(const char *tokenized_line,
    size_t tokens, const char *argv[], size_t max_args);

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
    argv[argc] = "\0";

    return argc;
}

static bool sys_spawn_process(const char *path, size_t argc, const char *argv[], pid_t *pid)
{
    int rc = syscall(SYS_SpawnProcess, (uint32_t*) pid, (uint32_t) path, argc, (uint32_t) argv, 0, 0);
    return rc == 0;
}

static bool sys_await_process(pid_t pid)
{
    int rc = syscall(SYS_AwaitProcess, NULL, (uint32_t) pid, 0, 0, 0, 0);
    return rc == 0;
}

static int run_builtin_command(const char *command, size_t argc, const char *argv[])
{
    (void) argc;
    (void) argv;

    if (strcmp(command, "help") == 0) {
        printf("There is no help\r\n");
        return 0;
    }

    return -1;
}

static bool run_program(size_t argc, const char *argv[])
{
    pid_t pid;
    if (sys_spawn_process(argv[0], argc, argv, &pid) != 0)
        return false;
    
    sys_await_process(pid);

    return true;
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    char *line = NULL;
    const char *command_argv[MAX_ARGS];
    

    terminal_init();

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
