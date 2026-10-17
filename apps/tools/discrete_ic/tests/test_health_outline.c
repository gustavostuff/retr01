#include "discrete_ic/health.h"
#include "discrete_ic/outline.h"

#include <stdio.h>

static int g_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail = 1;
    }
}

int main(void) {
    NsOutlineRgb ok = ns_outline_rgb(NS_HEALTH_OK);
    NsOutlineRgb warn = ns_outline_rgb(NS_HEALTH_WARN);
    NsOutlineRgb fail = ns_outline_rgb(NS_HEALTH_FAIL);

    expect_true(ok.g > ok.r && ok.g > ok.b, "ok is greenish");
    expect_true(warn.r > 100 && warn.g > 100, "warn is yellowish");
    expect_true(fail.r > fail.g && fail.r > fail.b, "fail is reddish");
    expect_true(ns_health_worst(NS_HEALTH_OK, NS_HEALTH_WARN) == NS_HEALTH_WARN, "worst warn");
    expect_true(ns_health_worst(NS_HEALTH_WARN, NS_HEALTH_FAIL) == NS_HEALTH_FAIL, "worst fail");

    if (g_fail) {
        fprintf(stderr, "test_ns_health_outline: FAILED\n");
        return 1;
    }
    printf("test_ns_health_outline: ok\n");
    return 0;
}
