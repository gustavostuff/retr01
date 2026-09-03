#include "retr01_sim/bus.h"
#include "sn74hc00.h"
#include "sn74hc04.h"
#include "sn74hc08.h"
#include "sn74hc32.h"
#include "test_common.h"

static void drive_ab(R01sEntity *e, R01sLevel a, R01sLevel b) {
    r01s_entity_drive(e, "1A", a);
    r01s_entity_drive(e, "1B", b);
}

int main(void) {
    R01sSn74hc00 nand;
    R01sSn74hc08 andc;
    R01sSn74hc32 orc;
    R01sSn74hc04 inv;
    R01sEntity *e;

    /* NAND */
    r01s_sn74hc00_init(&nand, "U50");
    e = r01s_sn74hc00_entity(&nand);
    expect_true(e->pin_count == 14, "HC00 pins");
    drive_ab(e, R01S_LVL_H, R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "NAND HH->L");
    drive_ab(e, R01S_LVL_H, R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_H, "NAND HL->H");

    /* AND */
    r01s_sn74hc08_init(&andc, "U51");
    e = r01s_sn74hc08_entity(&andc);
    drive_ab(e, R01S_LVL_H, R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_H, "AND HH->H");
    drive_ab(e, R01S_LVL_H, R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "AND HL->L");

    /* OR */
    r01s_sn74hc32_init(&orc, "U52");
    e = r01s_sn74hc32_entity(&orc);
    drive_ab(e, R01S_LVL_L, R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "OR LL->L");
    drive_ab(e, R01S_LVL_L, R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_H, "OR LH->H");

    /* INV */
    r01s_sn74hc04_init(&inv, "U53");
    e = r01s_sn74hc04_entity(&inv);
    expect_true(e->pin_count == 14, "HC04 pins");
    r01s_entity_drive(e, "1A", R01S_LVL_H);
    r01s_entity_drive(e, "2A", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "1Y") == R01S_LVL_L, "INV H->L");
    expect_true(r01s_entity_sense(e, "2Y") == R01S_LVL_H, "INV L->H");

    return test_done("test_sn74hc_glue");
}
