#include "test_harness.h"

int test_failures;

void test_expect(int cond, const char *file, int line, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, msg);
        test_failures++;
    }
}
