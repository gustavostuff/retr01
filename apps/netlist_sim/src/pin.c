#include "netlist_sim/pin.h"

void ns_pin_init(NsPin *pin, int number, const char *name, NsPinDir dir) {
    if (!pin) {
        return;
    }
    pin->name = name;
    pin->number = number;
    pin->dir = dir;
    pin->level = NS_LVL_Z;
}

void ns_pin_set(NsPin *pin, NsLevel level) {
    if (pin) {
        pin->level = level;
    }
}

NsLevel ns_pin_get(const NsPin *pin) {
    return pin ? pin->level : NS_LVL_Z;
}
