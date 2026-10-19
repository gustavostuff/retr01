#include "retr01_sim/timing.h"
#include "test_common.h"

int main(void) {
    uint32_t tpd;
    uint32_t path_typ;
    uint32_t path_max;
    R01sDelayU8 d;

    r01s_timing_reset();
    r01s_timing_set_prop_override(0, R01S_TPD_TYP);
    expect_true(!r01s_timing_prop_enabled(), "override off");

    r01s_timing_set_prop_override(1, R01S_TPD_TYP);
    expect_true(r01s_timing_prop_enabled(), "override on");
    expect_true(r01s_timing_pin_tpd_ns(R01S_TPD_PART_ATF22) == 0, "pin tpd always 0");
    tpd = r01s_timing_tpd_ns(R01S_TPD_PART_ATF22);
    expect_true(tpd == R01S_TPD_ATF22_TYP_NS, "typ ATF22 budget tpd");

    /* Delay helper models datasheet tpd (unit / future micro-settle). */
    r01s_timing_reset();
    r01s_timing_set_prop_override(1, R01S_TPD_TYP);
    r01s_timing_set_now_ns(0);
    r01s_delay_u8_reset(&d, 0x00);
    expect_true(r01s_delay_u8_update(&d, 0x5A, tpd) == 0x00, "helper holds before tpd");
    r01s_timing_advance_ns(tpd);
    expect_true(r01s_delay_u8_update(&d, 0x5A, tpd) == 0x5A, "helper after tpd");

    /* Corner max bumps path stack. */
    r01s_timing_set_prop_override(1, R01S_TPD_MAX);
    path_max = r01s_timing_path_decode_bus_reg_ns();
    r01s_timing_set_prop_override(1, R01S_TPD_TYP);
    path_typ = r01s_timing_path_decode_bus_reg_ns();
    expect_true(path_max > path_typ, "max path > typ path");
    expect_true(path_typ == R01S_TPD_ATF22_TYP_NS + R01S_TPD_HC245_TYP_NS + R01S_TPD_ATF22_TYP_NS,
                "typ path sum (decode+245+pld_reg)");
    expect_true(path_max == R01S_TPD_ATF22_MAX_NS + R01S_TPD_HC245_MAX_NS + R01S_TPD_ATF22_MAX_NS,
                "max path sum (decode+245+pld_reg)");
    expect_true(path_max <= R01S_PHI2_HALF_NS, "max decode+245+pld_reg fits PHI2 half");

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
