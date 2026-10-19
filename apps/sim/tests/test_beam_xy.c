#include "atf22v10.h"
#include "beam_xy.h"
#include "osc_dot.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

static void step_half(R01sEntity *osc, R01sEntity *x, R01sEntity *y) {
    r01s_entity_tick(osc);
    r01s_entity_drive(x, "CLK", r01s_entity_sense(osc, "DOT"));
    r01s_entity_tick(x);
    r01s_entity_drive(y, "CLK", r01s_entity_sense(x, "HSYNC"));
    r01s_entity_tick(y);
}

static void pulse_y(R01sEntity *y) {
    r01s_entity_drive(y, "CLK", R01S_LVL_L);
    r01s_entity_tick(y);
    r01s_entity_drive(y, "CLK", R01S_LVL_H);
    r01s_entity_tick(y);
}

int main(void) {
    R01sOscDot osc;
    R01sBeamXy beam;
    R01sAtf22v10 beam_y;
    R01sEntity *o;
    R01sEntity *b;
    R01sEntity *y;
    int i;
    int saw_hblank = 0;
    int saw_wrap_x = 0;
    int saw_hsync = 0;

    r01s_osc_dot_init(&osc, "Y2");
    r01s_beam_xy_init(&beam, "UPLDX");
    r01s_atf22v10_init(&beam_y, "UPLDY", R01S_PLD_BEAM_Y);
    o = r01s_osc_dot_entity(&osc);
    b = r01s_beam_xy_entity(&beam);
    y = r01s_atf22v10_entity(&beam_y);

    r01s_entity_drive(o, "VDD", R01S_LVL_H);
    r01s_entity_drive(o, "OE#", R01S_LVL_H);
    r01s_entity_drive(b, "RES", R01S_LVL_H);
    r01s_entity_drive(y, "RES", R01S_LVL_H);

    /* One line is 341 rising DOT edges. HSYNC rises on the edge that enters 264. */
    for (i = 0; i < 682; i++) {
        step_half(o, b, y);
        if (r01s_beam_xy_x(&beam) == 0 && i > 10) {
            saw_wrap_x = 1;
        }
        if (r01s_beam_xy_hblank(&beam)) {
            saw_hblank = 1;
        }
        if (r01s_beam_xy_hsync(&beam)) {
            saw_hsync = 1;
        }
    }
    expect_true(saw_wrap_x, "X wrapped after 341");
    expect_true(saw_hblank, "HBlank during line");
    expect_true(saw_hsync, "HSYNC during 264..288");
    expect_true(r01s_atf22v10_beam_y(&beam_y) == 1, "Y advanced on HSYNC");
    expect_true(r01s_beam_xy_x(&beam) == 0, "X back at 0");

    r01s_entity_reset(b);
    r01s_entity_drive(b, "RES", R01S_LVL_H);
    for (i = 0; i < 264 * 2; i++) {
        step_half(o, b, y);
    }
    expect_true(r01s_beam_xy_x(&beam) == 264, "count 264");
    expect_true(r01s_entity_sense(b, "HSYNC") == R01S_LVL_H, "HSYNC high at 264");

    r01s_atf22v10_beam_rewind(&beam_y);
    r01s_entity_drive(y, "RES", R01S_LVL_H);
    for (i = 0; i < 243; i++) {
        pulse_y(y);
    }
    expect_true(r01s_atf22v10_beam_y(&beam_y) == 243, "Y count 243");
    expect_true(r01s_entity_sense(y, "VSYNC") == R01S_LVL_H, "VSYNC high at 243");
    pulse_y(y);
    pulse_y(y);
    expect_true(r01s_atf22v10_beam_y(&beam_y) == 245, "Y count 245");
    expect_true(r01s_entity_sense(y, "VSYNC") == R01S_LVL_H, "VSYNC high at 245");
    pulse_y(y);
    expect_true(r01s_atf22v10_beam_y(&beam_y) == 246, "Y count 246");
    expect_true(r01s_entity_sense(y, "VSYNC") == R01S_LVL_L, "VSYNC low at 246");
    while (r01s_atf22v10_beam_y(&beam_y) != 0) {
        pulse_y(y);
    }
    expect_true(r01s_atf22v10_beam_y(&beam_y) == 0, "Y wrapped to 0");

    return test_done("test_beam_xy");
}
