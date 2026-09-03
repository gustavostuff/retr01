#include "retr01_sim/cart_module.h"

#include <string.h>

void r01s_cart_module_init(R01sCartModule *mod) {
    if (!mod) {
        return;
    }
    memset(mod, 0, sizeof(*mod));
    r01s_sst39sf040_init(&mod->flash, "U40");
    r01s_i2c_eeprom_init(&mod->save, "U50");
}

R01sSst39sf040 *r01s_cart_module_flash(R01sCartModule *mod) {
    return mod ? &mod->flash : NULL;
}

R01sI2cEeprom *r01s_cart_module_eeprom(R01sCartModule *mod) {
    return mod ? &mod->save : NULL;
}

void r01s_island_cart_module_init(R01sIsland *island) {
    R01sIslandCartModuleImpl *impl;
    R01sCartModule *mod;

    if (!island || !island->impl) {
        return;
    }
    impl = (R01sIslandCartModuleImpl *)island->impl;
    mod = impl->module;
    if (!mod) {
        return;
    }
    r01s_cart_module_init(mod);
    r01s_island_add_entity(island, r01s_sst39sf040_entity(&mod->flash));
    r01s_island_add_entity(island, r01s_i2c_eeprom_entity(&mod->save));
}
