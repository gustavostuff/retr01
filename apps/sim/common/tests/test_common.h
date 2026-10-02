#ifndef R01A_TEST_H
#define R01A_TEST_H

#include "discrete_ic/entity.h"

#include <stdio.h>

static int g_test_fail;

static void r01a_test_pitch_ic(NsEntity *e) {
    if (e && e->visual == NS_ENTITY_VIS_IC) {
        e->pkg_pitch_px = 6;
        ns_entity_refresh_body(e);
    }
}

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
