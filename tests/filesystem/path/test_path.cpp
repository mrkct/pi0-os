#include "libtest.h"
#include <kernel/vfs/vfs.cpp>


template<size_t Size>
static inline void ASSERT_PATH_COMPONENTS(const char *actual, const char* (&expected)[Size])
{
    for (size_t i = 0; i < Size; i++) {
        if (strcmp(actual, expected[i]) != 0) {
            ASSERT_FAIL("At index %lu: Expected to read '%s', actual is '%s'\n", i, expected[i], actual);
        }
        actual += strlen(actual) + 1;
    }

    if (*actual != '\0') {
        ASSERT_FAIL("Expected to read %lu components, but actual has more than that", Size);
    }
}

TEST(canonicalize_already_canonical)
{
    const char *path = "/hello/world/how/are/you";
    const char *expected[5] = {
        "hello", "world", "how", "are", "you"
    };

    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_many_leading_slashes)
{
    const char *path = "/////hello/world";
    const char *expected[2] = {
        "hello", "world"
    };

    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_many_trailing_slashes)
{
    const char *path = "hello/world////";
    const char *expected[2] = {
        "hello", "world"
    };
    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_many_slashes)
{
    const char *path = "hello////world";
    const char *expected[2] = {
        "hello", "world"
    };
    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_single_dot)
{
    const char *path = "hello/./world";
    const char *expected[2] = {
        "hello", "world"
    };
    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_double_dot)
{
    const char *path = "hello/../world";
    const char *expected[1] = {
        "world"
    };
    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_double_dot_double_dot)
{
    const char *path = "hello/../../world";
    const char *expected[1] = {
        "world"
    };
    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

TEST(canonicalize_empty_path)
{
    const char *path = "";
    char *result = canonicalize_path(path);
    ASSERT_EQUAL_STR(result, "");
    free(result);
}

TEST(double_dot_to_root)
{
    const char *path = "hello/../../world";
    const char *expected[1] = {
        "world"
    };
    char *result = canonicalize_path(path);
    ASSERT_PATH_COMPONENTS(result, expected);
    free(result);
}

int main(int argc, char **argv)
{
	run_tests(argc, argv);
	return 0;
}
