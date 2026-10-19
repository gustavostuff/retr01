#include "compositor.h"
#include "test_common.h"

int main(void) {
    R01sCompositor chip;

    r01s_compositor_init(&chip, "UPLDV");

    r01s_compositor_set_bg(&chip, 5);
    r01s_compositor_set_sprite(&chip, 0, 0);
    expect_true(r01s_compositor_out(&chip) == 5, "BG when sprite disabled");

    r01s_compositor_set_sprite(&chip, 0, 1);
    expect_true(r01s_compositor_out(&chip) == 5, "transparent sprite (0) defers to BG");

    r01s_compositor_set_sprite(&chip, 17, 1);
    expect_true(r01s_compositor_out(&chip) == 17, "opaque sprite wins");

    r01s_compositor_set_sprite(&chip, 17, 0);
    expect_true(r01s_compositor_out(&chip) == 5, "disable sprite restores BG");

    r01s_compositor_set_bg(&chip, 0x7F);
    expect_true(r01s_compositor_out(&chip) == 0x3F, "BG masked to 6 bits");

    return test_done("test_compositor");
}
