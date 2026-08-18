#pragma once

#include <stdio.h>
#include <stdlib.h>

static int g_test_failures = 0;

#define TEST(name) static void name(void)

#define ASSERT(cond)                                                     \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "\n    ASSERT failed: %s (%s:%d)\n", #cond,  \
                    __FILE__, __LINE__);                                 \
            g_test_failures++;                                           \
            return;                                                      \
        }                                                                \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))

#define RUN_TEST(name)                                                   \
    do {                                                                 \
        g_test_failures = 0;                                             \
        printf("  %s ... ", #name);                                      \
        fflush(stdout);                                                  \
        name();                                                          \
        if (g_test_failures) {                                           \
            printf("FAIL\n");                                            \
        } else {                                                         \
            printf("ok\n");                                              \
        }                                                                \
    } while (0)

#define TEST_RUNNER_BEGIN(title)                                         \
    int main(void)                                                       \
    {                                                                    \
        int rc = 0;                                                      \
        printf("%s\n", title);

#define TEST_RUNNER_END()                                                \
        return rc;                                                       \
    }

#define RUN_TEST_RC(name)                                                \
    do {                                                                 \
        g_test_failures = 0;                                             \
        printf("  %s ... ", #name);                                      \
        fflush(stdout);                                                  \
        name();                                                          \
        if (g_test_failures) {                                           \
            printf("FAIL\n");                                            \
            rc = 1;                                                      \
        } else {                                                         \
            printf("ok\n");                                              \
        }                                                                \
    } while (0)
