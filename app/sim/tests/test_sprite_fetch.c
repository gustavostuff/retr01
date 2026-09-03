#include "sprite_fetch.h"
#include "test_common.h"

int main(void) {
    R01sSpriteFetch chip;

    r01s_sprite_fetch_init(&chip, "UPLDN");
    expect_true(r01s_sprite_fetch_fill_count(&chip) == 0, "fresh fill count");

    r01s_sprite_fetch_note_fill(&chip, 40, 3, 24, 10, 34);
    expect_true(r01s_sprite_fetch_fill_count(&chip) == 1, "one fill");
    expect_true(r01s_sprite_fetch_pixel_count(&chip) == 24, "pixels");
    expect_true(r01s_sprite_fetch_last_ly(&chip) == 40, "ly");
    expect_true(r01s_sprite_fetch_last_hit_x(&chip) == 10, "hit x");
    expect_true(r01s_sprite_fetch_last_hit_color(&chip) == 34, "hit color");

    r01s_sprite_fetch_note_fill(&chip, 41, 1, 8, 0, 0);
    expect_true(r01s_sprite_fetch_fill_count(&chip) == 2, "two fills");
    expect_true(r01s_sprite_fetch_pixel_count(&chip) == 32, "pixel accumulate");
    expect_true(r01s_sprite_fetch_last_hit_color(&chip) == 34, "hit color kept when 0");

    return test_done("test_sprite_fetch");
}
