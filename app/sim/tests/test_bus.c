#include "retr01_sim/bus.h"
#include "retr01_sim/entity.h"
#include "test_common.h"

static const R01sEntityVTable EMPTY_VT = {NULL, NULL, NULL, NULL};

int main(void) {
    R01sEntity a, b, dst;

    r01s_entity_init(&a, &EMPTY_VT, "A", "TA");
    r01s_entity_init(&b, &EMPTY_VT, "B", "TB");
    r01s_entity_init(&dst, &EMPTY_VT, "D", "TD");
    r01s_entity_add_pin(&a, 1, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&a, 2, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&b, 1, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&b, 2, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&dst, 1, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&dst, 2, "DQ1", R01S_PIN_IO);

    /* Pull-up: undriven Z reads as high. */
    r01s_bus_hiz(&a, "DQ", 2);
    expect_true(r01s_bus_read(&a, "DQ", 2) == 0x3, "Z pulls high");

    /* Intentional fight -- fatal off so we can assert X without exiting. */
    r01s_bus_set_fatal_conflicts(0);
    r01s_bus_clear_conflicts();
    r01s_entity_drive(&a, "DQ0", R01S_LVL_H);
    r01s_entity_drive(&b, "DQ0", R01S_LVL_L);
    r01s_entity_drive(&a, "DQ1", R01S_LVL_H);
    r01s_entity_drive(&b, "DQ1", R01S_LVL_H);
    r01s_bus_resolve(&dst, "DQ", &a, "DQ", &b, "DQ", 2);
    expect_true(r01s_entity_sense(&dst, "DQ0") == R01S_LVL_X, "H+L fight");
    expect_true(r01s_entity_sense(&dst, "DQ1") == R01S_LVL_H, "H+H ok");
    expect_true(r01s_bus_conflict_count() >= 1, "conflict counted");

    r01s_bus_clear_conflicts();
    expect_true(r01s_level_merge(R01S_LVL_Z, R01S_LVL_L) == R01S_LVL_L, "Z merge L");
    expect_true(r01s_level_pulled(R01S_LVL_Z) == R01S_LVL_H, "pull-up");

    r01s_bus_set_fatal_conflicts(1);
    expect_true(r01s_bus_fatal_conflicts() == 1, "fatal restored");

    return test_done("test_bus");
}
