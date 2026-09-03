#include "retr01_sim/cart_slot.h"

void r01s_cart_slot_log_warn(const char *msg) {
    if (msg) {
        fprintf(stderr, "WARN: %s\n", msg);
    }
}

void r01s_cart_slot_reset(R01sCartSlotMgr *mgr) {
    if (!mgr) {
        return;
    }
    mgr->module = NULL;
    mgr->site = R01S_CART_SLOT_NONE;
}

int r01s_cart_slot_insert(R01sCartSlotMgr *mgr, R01sCartModule *mod, R01sCartSlotSite site) {
    if (!mgr || !mod || site == R01S_CART_SLOT_NONE) {
        return -1;
    }
    if (mgr->module != NULL && mgr->site != R01S_CART_SLOT_NONE) {
        r01s_cart_slot_log_warn("cart already inserted elsewhere");
        return -1;
    }
    mgr->module = mod;
    mgr->site = site;
    return 0;
}

int r01s_cart_slot_remove(R01sCartSlotMgr *mgr, R01sCartSlotSite site) {
    if (!mgr) {
        return -1;
    }
    if (mgr->site != site || mgr->module == NULL) {
        if (site == R01S_CART_SLOT_FLASHER) {
            r01s_cart_slot_log_warn("no cart in flasher socket");
        } else if (site == R01S_CART_SLOT_MOBO) {
            r01s_cart_slot_log_warn("no cart in console socket");
        }
        return -1;
    }
    mgr->module = NULL;
    mgr->site = R01S_CART_SLOT_NONE;
    return 0;
}

int r01s_cart_slot_present(const R01sCartSlotMgr *mgr, R01sCartSlotSite site) {
    return mgr && mgr->module && mgr->site == site;
}

R01sSst39sf040 *r01s_cart_slot_flash(R01sCartSlotMgr *mgr, R01sCartSlotSite site) {
    if (!r01s_cart_slot_present(mgr, site)) {
        return NULL;
    }
    return r01s_cart_module_flash(mgr->module);
}

R01sI2cEeprom *r01s_cart_slot_eeprom(R01sCartSlotMgr *mgr, R01sCartSlotSite site) {
    if (!r01s_cart_slot_present(mgr, site)) {
        return NULL;
    }
    return r01s_cart_module_eeprom(mgr->module);
}

int r01s_cart_slot_check_read(const R01sCartSlotMgr *mgr, R01sCartSlotSite site, const char *context) {
    if (r01s_cart_slot_present(mgr, site)) {
        return 0;
    }
    if (context) {
        fprintf(stderr, "WARN: cart read with no cart in %s socket (%s)\n",
                site == R01S_CART_SLOT_MOBO ? "console" : "flasher", context);
    } else {
        r01s_cart_slot_log_warn("cart read with empty socket");
    }
    return -1;
}
