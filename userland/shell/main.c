#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <api/syscalls.h>
#include "libsstring.h"

#define MAX_ARGS 16

static char *read_line(const char *prompt);
static size_t tokenize(char *line);
static size_t argv_from_tokenized_line(const char *tokenized_line,
    size_t tokens, const char *argv[], size_t max_args);

static char *read_line(const char *prompt)
{
    printf("%s", prompt);

    char *line = malloc(8);
    size_t length = 0, allocated = 8;
    
    do {
        int c = getchar();
        if (c > 0)
            putchar(c);

        bool is_eol = c == '\n' || c == '\r';
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
    argv[argc] = "\0";

    return argc;
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
    PID pid;
    int32_t fds[] = {0, 1, 2}; // Inherit stdin, stdout and stderr
    SpawnProcessConfig cfg = {
        .args = argv,
        .args_len = argc,
        .descriptors = fds,
        .descriptors_len = 3,
        .flags = 0
    };

    if (spawn_process(argv[0], &cfg, &pid) != 0)
        return false;

    await_process(pid);

    return true;
}

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    char *line = NULL;
    const char *command_argv[MAX_ARGS];

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
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
