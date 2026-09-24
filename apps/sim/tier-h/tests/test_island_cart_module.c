#include "retr01_sim/cart_module.h"
#include "retr01_sim/island_builder.h"
#include "test_common.h"

static const R01sIslandVTable CART_VT = {r01s_island_cart_module_init, NULL, NULL, NULL, NULL};

int main(void) {
    R01sIslandBuilder builder;
    R01sCartModule mod;
    R01sIslandCartModuleImpl impl;
    R01sIslandGroup *group;

    impl.module = &mod;
    r01s_island_builder_init(&builder);
    expect_true(r01s_island_builder_add(&builder, &CART_VT, "ISLAND CART MODULE", 0, 0, 1, 1, &impl) == 0,
                "add cart island");
    r01s_island_builder_mount_rel(&builder, r01s_sst39sf040_entity(&mod.flash), 0, 0, 0);
    r01s_island_builder_mount_rel(&builder, r01s_i2c_eeprom_entity(&mod.save), 0, 80, 0);
    expect_true(r01s_island_builder_finish(&builder) == 0, "finish");
    group = r01s_island_builder_group(&builder);
    expect_true(r01s_island_group_count(group) == 1, "one island");
    expect_true(r01s_island_group_at(group, 0)->entity_count == 2, "flash + eeprom");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_island_cart_module");
}
