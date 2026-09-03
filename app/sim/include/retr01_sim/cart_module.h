#ifndef retr01_SIM_CART_MODULE_H
#define retr01_SIM_CART_MODULE_H

#include "i2c_eeprom.h"
#include "retr01_sim/entity.h"
#include "retr01_sim/island.h"
#include "sst39sf040.h"

/*
 * Detachable cartridge PCB: SST39SF040 + 24C64 (docs/cart.md).
 * Lives on its own island; inserts into mobo or flasher socket.
 */
typedef struct R01sCartModule {
    R01sSst39sf040 flash;
    R01sI2cEeprom save;
} R01sCartModule;

typedef struct R01sIslandCartModuleImpl {
    R01sCartModule *module;
} R01sIslandCartModuleImpl;

void r01s_cart_module_init(R01sCartModule *mod);
R01sSst39sf040 *r01s_cart_module_flash(R01sCartModule *mod);
R01sI2cEeprom *r01s_cart_module_eeprom(R01sCartModule *mod);

void r01s_island_cart_module_init(R01sIsland *island);

#endif
