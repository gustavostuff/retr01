#include "at27c256r.h"

#include "netlist_sim/bus.h"
#include "r01_kit_palette.h"
#include "test_common.h"

int main(void) {
    R01aAt27c256r prom;
    NsEntity *e;
    uint8_t packed;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint32_t q;

    r01a_at27c256r_init(&prom, "U24");
    e = r01a_at27c256r_entity(&prom);
    r01_kit_rgb(48, &r, &g, &b);
    packed = (uint8_t)((((r * 7 + 127) / 255) << 5) | (((g * 7 + 127) / 255) << 2) | ((b * 3 + 127) / 255));
    expect_true(r01a_at27c256r_peek(&prom, 48) == packed, "kit 48 packed byte");

    ns_entity_drive(e, "A0", NS_LVL_L);
    ns_entity_drive(e, "A1", NS_LVL_L);
    ns_entity_drive(e, "A2", NS_LVL_L);
    ns_entity_drive(e, "A3", NS_LVL_L);
    ns_entity_drive(e, "A4", NS_LVL_H); /* 16 */
    ns_entity_drive(e, "A5", NS_LVL_H); /* 48 */
    ns_entity_drive(e, "A6", NS_LVL_L);
    ns_entity_drive(e, "CE#", NS_LVL_L);
    ns_entity_drive(e, "OE#", NS_LVL_L);
    ns_entity_eval(e);
    q = ns_bus_read(e, "O", 8);
    expect_true((uint8_t)q == packed, "PROM data at A=48");

    ns_entity_drive(e, "OE#", NS_LVL_H);
    ns_entity_eval(e);
    expect_true(ns_entity_sense(e, "O0") == NS_LVL_Z, "OE# high -> hi-Z");

    return test_done("test_at27c256r");
}
