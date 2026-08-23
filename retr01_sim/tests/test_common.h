#ifndef RETR01_SIM_TEST_H
#define RETR01_SIM_TEST_H

#include <stdio.h>

static int g_test_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_test_fail = 1;
    }
}

static int test_done(const char *name) {
    if (g_test_fail) {
        fprintf(stderr, "%s: FAILED\n", name);
        return 1;
    }
    printf("%s: ok\n", name);
    return 0;
}

#endif
