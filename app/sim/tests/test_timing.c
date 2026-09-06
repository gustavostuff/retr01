#include "retr01_sim/bus.h"
#include "retr01_sim/timing.h"
#include "sn74hc573.h"
#include "test_common.h"

#include <stdio.h>

static void drive_d(R01sEntity *e, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%dD", i + 1);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static uint8_t sense_q(R01sEntity *e) {
    int i;
    uint8_t v = 0;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%dQ", i + 1);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            v |= (uint8_t)(1u << i);
        }
    }
    return v;
}

int main(void) {
    R01sSn74hc573 latch;
    R01sEntity *e;
    uint32_t tpd;
    uint32_t path_typ;
    uint32_t path_max;
    R01sDelayU8 d;

    r01s_timing_reset();
    r01s_timing_set_prop_override(0, R01S_TPD_TYP);
    expect_true(!r01s_timing_prop_enabled(), "override off");

    /* Chip pin model stays combinatorial even when DELAY corner is on. */
    r01s_timing_set_prop_override(1, R01S_TPD_TYP);
    expect_true(r01s_timing_prop_enabled(), "override on");
    expect_true(r01s_timing_pin_tpd_ns(R01S_TPD_PART_HC573) == 0, "pin tpd always 0");
    tpd = r01s_timing_tpd_ns(R01S_TPD_PART_HC573);
    expect_true(tpd == R01S_TPD_HC573_TYP_NS, "typ HC573 budget tpd");

    r01s_sn74hc573_init(&latch, "U_td");
    e = r01s_sn74hc573_entity(&latch);
    r01s_entity_drive(e, "OE", R01S_LVL_L);
    r01s_entity_drive(e, "LE", R01S_LVL_H);
    drive_d(e, 0xA5);
    r01s_entity_eval(e);
    expect_true(sense_q(e) == 0xA5, "chip Q combinatorial with DELAY on");

    drive_d(e, 0x5A);
    r01s_entity_eval(e);
    expect_true(sense_q(e) == 0x5A, "chip Q same-settle");
    expect_true(r01s_sn74hc573_peek_q(&latch) == 0x5A, "internal latch");

    /* Delay helper still models datasheet tpd (unit / future micro-settle). */
    r01s_timing_reset();
    r01s_timing_set_prop_override(1, R01S_TPD_TYP);
    r01s_timing_set_now_ns(0);
    r01s_delay_u8_reset(&d, 0x00);
    expect_true(r01s_delay_u8_update(&d, 0x5A, tpd) == 0x00, "helper holds before tpd");
    r01s_timing_advance_ns(tpd);
    expect_true(r01s_delay_u8_update(&d, 0x5A, tpd) == 0x5A, "helper after tpd");

    /* Corner max bumps path stack. */
    r01s_timing_set_prop_override(1, R01S_TPD_MAX);
    path_max = r01s_timing_path_decode_bus_latch_ns();
    r01s_timing_set_prop_override(1, R01S_TPD_TYP);
    path_typ = r01s_timing_path_decode_bus_latch_ns();
    expect_true(path_max > path_typ, "max path > typ path");
    expect_true(path_typ == R01S_TPD_ATF22_TYP_NS + R01S_TPD_HC245_TYP_NS + R01S_TPD_HC573_TYP_NS,
                "typ path sum (historical decode+245+573 budget)");
    expect_true(path_max > R01S_PHI2_HALF_NS, "max decode+245+573 exceeds PHI2 half (budget flag)");

    /* SRAM tAA helper. */
    r01s_timing_reset();
    r01s_timing_set_prop_override(1, R01S_TPD_MAX);
    r01s_timing_set_now_ns(0);
    r01s_delay_u8_reset(&d, 0x11);
    expect_true(r01s_delay_u8_update(&d, 0x22, R01S_TAA_SRAM_MAX_NS) == 0x11, "helper holds before tAA");
    r01s_timing_advance_ns(R01S_TAA_SRAM_MAX_NS);
    expect_true(r01s_delay_u8_update(&d, 0x22, R01S_TAA_SRAM_MAX_NS) == 0x22, "helper after tAA");

    r01s_timing_set_prop_override(-1, R01S_TPD_TYP);
    return test_done("test_timing");
}
