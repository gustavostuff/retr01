#ifndef retr01_STUDIO_TEST_HARNESS_H
#define retr01_STUDIO_TEST_HARNESS_H

#include <stdio.h>

extern int test_failures;

void test_expect(int cond, const char *file, int line, const char *msg);

#define EXPECT(cond, msg) test_expect((cond), __FILE__, __LINE__, (msg))

#define TEST_MAIN() int main(void)

#define TEST_EXIT()                                                                                \
    do {                                                                                           \
        if (test_failures) {                                                                       \
            fprintf(stderr, "%d test(s) failed\n", test_failures);                                 \
            return 1;                                                                              \
        }                                                                                          \
        puts("ok");                                                                                \
        return 0;                                                                                    \
    } while (0)

#endif
