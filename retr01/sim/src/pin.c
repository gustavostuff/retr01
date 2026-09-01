#include "retr01_sim/pin.h"

void r01s_pin_init(R01sPin *pin, int number, const char *name, R01sPinDir dir) {
    if (!pin) {
        return;
    }
    pin->name = name;
    pin->number = number;
    pin->dir = dir;
    pin->level = R01S_LVL_Z;
}

void r01s_pin_set(R01sPin *pin, R01sLevel level) {
    if (pin) {
        pin->level = level;
    }
}

R01sLevel r01s_pin_get(const R01sPin *pin) {
    return pin ? pin->level : R01S_LVL_Z;
}
