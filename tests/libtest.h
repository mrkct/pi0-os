// Based on https://github.com/PalmeseMattia/Xtal/tree/main

#ifndef XTAL_H
# define XTAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>


# define BUFFER_SIZE 10
# define TEST(name) \
    void name(); \
    __attribute__((constructor)) void register_##name() { register_test(#name, name); } \
    void name()

/*
 * PRINT UTILITIES 
 */
# define GREEN "\x1B[32m"
# define RED "\x1B[31m"
# define YELLOW "\x1B[033m"
# define NRM "\x1B[0m"
# define CHECK "\u2713"
# define CROSS "\u2715"
# define PRINT_PASSED printf("%sTest passed %s%s\n\n", GREEN, CHECK, NRM)
# define PRINT_FAILED_SIG printf("%sTest failed with signal %d and status %d %s%s\n\n", RED, WTERMSIG(status), WEXITSTATUS(status), CROSS, NRM)
# define PRINT_SEG_FAULT printf("%sTest failed due to segmentation fault %s%s\n\n", RED, CROSS, NRM)
# define PRINT_FAILED printf("%sTest failed with status %d %s%s\n\n", RED, WEXITSTATUS(status), CROSS, NRM)

typedef void (*testcase)();

typedef struct {
    const char *name;
    testcase function;
} test_t;

static int test_count = 0;
static int register_size = 0;
static test_t *tests = NULL;

void register_test(const char *name, testcase function)
{
    if (test_count >= (register_size - 1)) {
        tests = (test_t*) realloc(tests, sizeof(test_t) * (register_size += BUFFER_SIZE));
    }
    tests[test_count].name = name;
    tests[test_count].function = function;
    test_count++;
}

static void run_test(test_t const *test, bool run_in_forked_thread)
{
    int pid = 0;
    int status;

    if (run_in_forked_thread && (pid = fork()) < 0) {
        perror("Fork failed");
        exit(1);
    }
    if (pid == 0) {
        test->function();
        exit(0);
    } else {
        printf("%sTEST %ld: %s%s\n", YELLOW, (test - tests) + 1, test->name, NRM);
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) {
            if (WTERMSIG(status) == SIGSEGV)
                PRINT_SEG_FAULT;
            else
                PRINT_FAILED_SIG;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            PRINT_PASSED;
        else
            PRINT_FAILED;
    }
}

void run_tests(int argc, char **argv)
{
    if (argc == 2) {
        for (int i = 0; i < test_count; i++) {
            if (strcmp(tests[i].name, argv[1]) == 0) {
                run_test(&tests[i], false);
                return;
            }
        }
        fprintf(stderr, "Test %s not found\n", argv[1]);
        exit(1);
    }

    for (int i = 0; i < test_count; i++) {
        run_test(&tests[i], true);
    }
    free(tests);
    tests = NULL;
}

/*
 * ASSERTIONS
 */
template<typename T>
static void ASSERT_EQUAL_INT(T expected, T actual)
{
    if (expected != actual) {
        std::cerr << "Expected " << expected << " but got " << actual << std::endl;
        exit(EXIT_FAILURE);
    }
}



#define ASSERT_FAIL(format, ...) \
    do { \
        fprintf(stderr, "Assertion failed: "); \
        fprintf(stderr, format, ##__VA_ARGS__); \
        fprintf(stderr, "\n"); \
        exit(EXIT_FAILURE); \
    } while (0)

# define ASSERT_EQUAL_STR(expected, actual) \
    for (int i = 0; expected[i]; i++) { \
        if (expected[i] != actual[i] || !actual[i]) { \
            fprintf(stderr, "Assertion failed: Strings differ at index %d.\nExpected \"%s\", got \"%s\"\n", i, expected, actual); \
            exit(EXIT_FAILURE); \
        } \
    }

# define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        fprintf(stderr, "Assertion failed: Condition is false\n"); \
        fprintf(stderr, "At %s:%d " #condition "\n", __FILE__, __LINE__);  \
        exit(EXIT_FAILURE); \
    }

# define ASSERT_NOT_NULL(stuff) \
    if ((stuff) == NULL) { \
        fprintf(stderr, "Assertion failed: Memory points to null\n"); \
        fprintf(stderr, "%s:%d " #stuff, __FILE__, __LINE__);  \
        exit(EXIT_FAILURE); \
    }

# define ASSERT_NULL(stuff) \
    if ((stuff) != NULL) { \
        fprintf(stderr, "Assertion failed: Memory do not points to null\n"); \
        exit(EXIT_FAILURE); \
    }

/*
 *  UTILITY FUNCTIONS
 */

static void load_file(const char *filename, uint8_t **buffer, size_t *size)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    *buffer = (uint8_t*) malloc(*size);
    if (fread(*buffer, 1, *size, file) != *size) {
        perror("Failed to read file");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

#endif
