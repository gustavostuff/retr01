#include "bg_fetch.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

#include <stdio.h>

int main(void) {
    R01sBgFetch chip;
    r01s_bg_fetch_init(&chip, "UTEST");

    r01s_bg_fetch_set_scroll(&chip, 0, 0);
    r01s_bg_fetch_set_cpu_phase(&chip, 1);
    r01s_bg_fetch_set_beam(&chip, 0, 0, 0, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    expect_true(!r01s_bg_fetch_active(&chip), "idle on CPU phase");

    r01s_bg_fetch_set_cpu_phase(&chip, 0);
    r01s_bg_fetch_set_beam(&chip, 0, 0, 0, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    expect_true(r01s_bg_fetch_active(&chip), "fetch on PPU visible");
    expect_true(r01s_bg_fetch_va(&chip) == 0, "tile addr cell0");
    expect_true(!r01s_bg_fetch_attr_cycle(&chip), "even X = tile");

    r01s_bg_fetch_set_beam(&chip, 1, 0, 0, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    expect_true(r01s_bg_fetch_va(&chip) == R01S_BG_ATTR_OFF, "odd X = attr0");
    expect_true(r01s_bg_fetch_attr_cycle(&chip), "odd X = attr cycle");

    /* Logical (8,0) with SCALE2 => beam X=16 → tile col 1 */
    r01s_bg_fetch_set_beam(&chip, 16, 0, 0, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    expect_true(r01s_bg_fetch_va(&chip) == 1, "tile col1");

    /* Scroll into slot 1: scroll_x=120 + lx=16 (beam X=32) => sx=136 */
    r01s_bg_fetch_set_scroll(&chip, 120, 0);
    r01s_bg_fetch_set_beam(&chip, 32, 0, 0, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    expect_true(r01s_bg_fetch_va(&chip) == R01S_BG_SLOT_BYTES + 1, "slot1 tile col1");

    r01s_bg_fetch_set_beam(&chip, 0, 0, 1, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    expect_true(!r01s_bg_fetch_active(&chip), "idle in HBlank");

    r01s_bg_fetch_set_beam(&chip, 0, 0, 0, 0);
    r01s_entity_eval(r01s_bg_fetch_entity(&chip));
    r01s_bg_fetch_capture_dq(&chip, 0x42);
    expect_true(r01s_bg_fetch_last_tile(&chip) == 0x42, "tile latch");

    return test_done("test_bg_fetch");
}
