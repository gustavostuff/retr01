#include "as6c62256.h"
#include "s1_lab.h"
#include "test_common.h"

int main(void) {
    R01aAs6c62256 field;

    r01a_as6c62256_init(&field, "U41");
    r01c_s1_vblank_field(&field, 0);
    expect_true(r01a_field_kit_at(&field, 84, 56) == 33, "player body pixel (example_01 pal)");
    expect_true(r01a_field_kit_at(&field, 23, 98) == 17, "slime prop pixel");
    r01c_s1_hblank_bg0_line(&field, 10, 0);
    expect_true(field.mem[R01C_BG0_LINE_BASE + 10u * 128u + 80u] == 14, "BG0 line sample");
    return test_done("test_tier_c_field");
}
