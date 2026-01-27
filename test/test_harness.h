#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Current test name for error reporting */
static const char *current_test_name = NULL;

/* Assertion macros */
#define TEST_ASSERT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "  FAIL: %s:%d: assertion failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define TEST_ASSERT_MSG(condition, msg) do { \
    if (!(condition)) { \
        fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while (0)

#define TEST_ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        fprintf(stderr, "  FAIL: %s:%d: expected %lld, got %lld\n", \
                __FILE__, __LINE__, (long long)(expected), (long long)(actual)); \
        return 1; \
    } \
} while (0)

#define TEST_ASSERT_STR_EQ(expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        fprintf(stderr, "  FAIL: %s:%d: expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, (expected), (actual)); \
        return 1; \
    } \
} while (0)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "  FAIL: %s:%d: expected non-NULL\n", \
                __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define TEST_ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "  FAIL: %s:%d: expected NULL\n", \
                __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

/* Run a single test */
#define RUN_TEST(test_func) do { \
    current_test_name = #test_func; \
    printf("  Running %s...", #test_func); \
    fflush(stdout); \
    tests_run++; \
    int result = test_func(); \
    if (result == 0) { \
        printf(" OK\n"); \
        tests_passed++; \
    } else { \
        tests_failed++; \
    } \
} while (0)

/* Print test summary */
#define TEST_SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Tests run: %d, Passed: %d, Failed: %d\n", \
           tests_run, tests_passed, tests_failed); \
    printf("========================================\n"); \
} while (0)

/* Return overall test result (0 = all passed) */
#define TEST_RESULT() (tests_failed > 0 ? 1 : 0)

#endif /* TEST_HARNESS_H */
