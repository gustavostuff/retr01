#include "beam_xy.h"
#include "osc_dot.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sOscDot osc;
    R01sBeamXy beam;
    R01sEntity *o;
    R01sEntity *b;
    int i;
    int saw_hblank = 0;
    int saw_vblank = 0;
    int saw_wrap_x = 0;

    r01s_osc_dot_init(&osc, "Y2");
    r01s_beam_xy_init(&beam, "UPLD");
    o = r01s_osc_dot_entity(&osc);
    b = r01s_beam_xy_entity(&beam);

    r01s_entity_drive(o, "VDD", R01S_LVL_H);
    r01s_entity_drive(o, "OE#", R01S_LVL_H);
    r01s_entity_drive(b, "RES#", R01S_LVL_H);

    /* One full line = 341 dots = 682 half-cycles */
    for (i = 0; i < 682; i++) {
        r01s_entity_tick(o);
        r01s_entity_drive(b, "DOT", r01s_entity_sense(o, "DOT"));
        r01s_entity_tick(b);
        if (r01s_beam_xy_x(&beam) == 0 && i > 10) {
            saw_wrap_x = 1;
        }
        if (r01s_beam_xy_hblank(&beam)) {
            saw_hblank = 1;
        }
    }
    expect_true(saw_wrap_x, "X wrapped after 341");
    expect_true(r01s_beam_xy_y(&beam) == 1, "Y advanced one line");
    expect_true(saw_hblank, "HBlank during line");

    /* Advance to VBlank: need 239 more full lines from y=1 → y=240 */
    for (i = 0; i < 239 * 682 && !saw_vblank; i++) {
        r01s_entity_tick(o);
        r01s_entity_drive(b, "DOT", r01s_entity_sense(o, "DOT"));
        r01s_entity_tick(b);
        if (r01s_beam_xy_vblank(&beam)) {
            saw_vblank = 1;
        }
    }
    expect_true(saw_vblank, "VBlank at Y>=240");
    expect_true(r01s_beam_xy_y(&beam) >= R01S_BEAM_VISIBLE_H, "Y in VBlank");

    return test_done("test_beam_xy");
}
