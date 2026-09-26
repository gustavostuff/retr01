#include "discrete_ic/bus.h"
#include "discrete_ic/chip.h"
#include "discrete_ic/ns_island.h"

#include <stdio.h>

static int g_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail = 1;
    }
}

static void nand_eval(NsEntity *e) {
    NsLevel a = ns_entity_sense(e, "1A");
    NsLevel b = ns_entity_sense(e, "1B");
    int ah = (a == NS_LVL_H || a == NS_LVL_Z);
    int bh = (b == NS_LVL_H || b == NS_LVL_Z);
    ns_entity_drive(e, "1Y", (ah && bh) ? NS_LVL_L : NS_LVL_H);
}

static const NsChipPinDef k_pins[] = {
    {1, "1A", NS_PIN_IN},
    {2, "1B", NS_PIN_IN},
    {3, "1Y", NS_PIN_OUT},
    {14, "VCC", NS_PIN_PWR},
    {7, "GND", NS_PIN_PWR},
};

int main(void) {
    NsChipClass cls = {0};
    NsEntity *u1;
    NsIsland *island;

    cls.part = "SN74HC00";
    cls.dip_pins = 14;
    cls.pins = k_pins;
    cls.pin_count = (int)(sizeof(k_pins) / sizeof(k_pins[0]));
    cls.eval = nand_eval;
    expect_true(ns_chip_register(&cls) == 0, "register");
    expect_true(ns_chip_lookup("SN74HC00") != NULL, "lookup");

    u1 = ns_chip_create("SN74HC00", "U1");
    expect_true(u1 != NULL, "create");
    expect_true(u1->pin_count == 5, "pins");
    expect_true(u1->dip_pins == 14, "dip");

    island = ns_island_create("nand_demo");
    expect_true(island != NULL, "island");
    expect_true(ns_island_add(island, u1) == 0, "add");
    expect_true(ns_island_find(island, "U1") == u1, "find");

    ns_entity_drive(u1, "1A", NS_LVL_H);
    ns_entity_drive(u1, "1B", NS_LVL_H);
    ns_island_eval(island);
    expect_true(ns_entity_sense(u1, "1Y") == NS_LVL_L, "nand H H -> L");

    ns_island_destroy(island);

    if (g_fail) {
        fprintf(stderr, "test_ns_chip_registry: FAILED\n");
        return 1;
    }
    printf("test_ns_chip_registry: ok\n");
    return 0;
}
