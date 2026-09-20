#include "netlist_sim/bus.h"
#include "netlist_sim/entity.h"

#include <stdio.h>

static int g_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail = 1;
    }
}

int main(void) {
    static const NsEntityVTable EMPTY_VT = {NULL, NULL, NULL, NULL};
    NsEntity a, b, dst;

    ns_entity_init(&a, &EMPTY_VT, "A", "TA");
    ns_entity_init(&b, &EMPTY_VT, "B", "TB");
    ns_entity_init(&dst, &EMPTY_VT, "D", "TD");
    ns_entity_add_pin(&a, 1, "DQ0", NS_PIN_IO);
    ns_entity_add_pin(&a, 2, "DQ1", NS_PIN_IO);
    ns_entity_add_pin(&b, 1, "DQ0", NS_PIN_IO);
    ns_entity_add_pin(&b, 2, "DQ1", NS_PIN_IO);
    ns_entity_add_pin(&dst, 1, "DQ0", NS_PIN_IO);
    ns_entity_add_pin(&dst, 2, "DQ1", NS_PIN_IO);

    ns_bus_hiz(&a, "DQ", 2);
    expect_true(ns_bus_read(&a, "DQ", 2) == 0x3, "Z pulls high");

    ns_bus_set_fatal_conflicts(0);
    ns_bus_clear_conflicts();
    ns_entity_drive(&a, "DQ0", NS_LVL_H);
    ns_entity_drive(&b, "DQ0", NS_LVL_L);
    ns_entity_drive(&a, "DQ1", NS_LVL_H);
    ns_entity_drive(&b, "DQ1", NS_LVL_H);
    ns_bus_resolve(&dst, "DQ", &a, "DQ", &b, "DQ", 2);
    expect_true(ns_entity_sense(&dst, "DQ0") == NS_LVL_X, "H+L fight");
    expect_true(ns_entity_sense(&dst, "DQ1") == NS_LVL_H, "H+H ok");
    expect_true(ns_bus_conflict_count() >= 1, "conflict counted");

    ns_bus_clear_conflicts();
    expect_true(ns_level_merge(NS_LVL_Z, NS_LVL_L) == NS_LVL_L, "Z merge L");
    expect_true(ns_level_pulled(NS_LVL_Z) == NS_LVL_H, "pull-up");

    ns_bus_set_fatal_conflicts(1);
    expect_true(ns_bus_fatal_conflicts() == 1, "fatal restored");

    if (g_fail) {
        fprintf(stderr, "test_ns_bus: FAILED\n");
        return 1;
    }
    printf("test_ns_bus: ok\n");
    return 0;
}
