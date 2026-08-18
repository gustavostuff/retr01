#include "test_harness.h"

#include "retr01/screen.h"

#include <string.h>

TEST(attr_corners)
{
    uint8_t attrs[RETR01_SCREEN_ATTR_BYTES];
    memset(attrs, 0, sizeof(attrs));

    retr01_attr_set(attrs, 0, 0, 1);
    retr01_attr_set(attrs, 31, 0, 2);
    retr01_attr_set(attrs, 0, 29, 3);
    retr01_attr_set(attrs, 31, 29, 0);

    ASSERT_EQ(retr01_attr_get(attrs, 0, 0), 1);
    ASSERT_EQ(retr01_attr_get(attrs, 31, 0), 2);
    ASSERT_EQ(retr01_attr_get(attrs, 0, 29), 3);
    ASSERT_EQ(retr01_attr_get(attrs, 31, 29), 0);
}

TEST_RUNNER_BEGIN("test_screen")
RUN_TEST_RC(attr_corners);
TEST_RUNNER_END()
