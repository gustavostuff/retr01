#include "retr01_sim/bus.h"
#include "sn74hc157.h"
#include "test_common.h"

int main(void) {
    R01sSn74hc157 chip;
    R01sEntity *e;

    r01s_sn74hc157_init(&chip, "U30");
    e = r01s_sn74hc157_entity(&chip);
    expect_true(e->pin_count == 16, "16 pins");

    r01s_entity_drive(e, "1A", R01S_LVL_H);
    r01s_entity_drive(e, "1B", R01S_LVL_L);
    r01s_entity_drive(e, "2A", R01S_LVL_L);
    r01s_entity_drive(e, "2B", R01S_LVL_H);
    r01s_entity_drive(e, "3A", R01S_LVL_H);
    r01s_entity_drive(e, "3B", R01S_LVL_H);
    r01s_entity_drive(e, "4A", R01S_LVL_L);
    r01s_entity_drive(e, "4B", R01S_LVL_L);

    /* G#=L, AB=L => Y = A */
    r01s_entity_drive(e, "G#", R01S_LVL_L);
    r01s_entity_drive(e, "AB", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_H, "Y1=A");
    expect_true(r01s_entity_sense(e, "2Y") == R01S_LVL_L, "Y2=A");
    expect_true(r01s_entity_sense(e, "3Y") == R01S_LVL_H, "Y3=A");
    expect_true(r01s_entity_sense(e, "4Y") == R01S_LVL_L, "Y4=A");

    /* AB=H => Y = B */
    r01s_entity_drive(e, "AB", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "Y1=B");
    expect_true(r01s_entity_sense(e, "2Y") == R01S_LVL_H, "Y2=B");
    expect_true(r01s_entity_sense(e, "3Y") == R01S_LVL_H, "Y3=B");
    expect_true(r01s_entity_sense(e, "4Y") == R01S_LVL_L, "Y4=B");

    /* G# high forces Y low */
    r01s_entity_drive(e, "G#", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "G force 1Y L");
    expect_true(r01s_entity_sense(e, "2Y") == R01S_LVL_L, "G force 2Y L");
    expect_true(r01s_entity_sense(e, "3Y") == R01S_LVL_L, "G force 3Y L");
    expect_true(r01s_entity_sense(e, "4Y") == R01S_LVL_L, "G force 4Y L");

    return test_done("test_sn74hc157");
}
