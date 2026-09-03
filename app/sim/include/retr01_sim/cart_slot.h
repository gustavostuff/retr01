#ifndef retr01_SIM_CART_SLOT_H
#define retr01_SIM_CART_SLOT_H

#include "retr01_sim/cart_module.h"

#include <stdio.h>

typedef enum R01sCartSlotSite {
    R01S_CART_SLOT_NONE = 0,
    R01S_CART_SLOT_MOBO,
    R01S_CART_SLOT_FLASHER,
} R01sCartSlotSite;

/* One physical cart module may occupy at most one socket site. */
typedef struct R01sCartSlotMgr {
    R01sCartModule *module;
    R01sCartSlotSite site;
} R01sCartSlotMgr;

void r01s_cart_slot_reset(R01sCartSlotMgr *mgr);
int r01s_cart_slot_insert(R01sCartSlotMgr *mgr, R01sCartModule *mod, R01sCartSlotSite site);
int r01s_cart_slot_remove(R01sCartSlotMgr *mgr, R01sCartSlotSite site);
int r01s_cart_slot_present(const R01sCartSlotMgr *mgr, R01sCartSlotSite site);

R01sSst39sf040 *r01s_cart_slot_flash(R01sCartSlotMgr *mgr, R01sCartSlotSite site);
R01sI2cEeprom *r01s_cart_slot_eeprom(R01sCartSlotMgr *mgr, R01sCartSlotSite site);

int r01s_cart_slot_check_read(const R01sCartSlotMgr *mgr, R01sCartSlotSite site, const char *context);

void r01s_cart_slot_log_warn(const char *msg);

#endif
