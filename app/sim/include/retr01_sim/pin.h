#ifndef retr01_SIM_PIN_H
#define retr01_SIM_PIN_H

#include "retr01_sim/types.h"

typedef struct R01sPin {
    const char *name; /* datasheet name, e.g. "PHI2", "A0" */
    int number;       /* 1-based package pin */
    R01sPinDir dir;
    R01sLevel level;  /* current driven/sensed level */
} R01sPin;

void r01s_pin_init(R01sPin *pin, int number, const char *name, R01sPinDir dir);
void r01s_pin_set(R01sPin *pin, R01sLevel level);
R01sLevel r01s_pin_get(const R01sPin *pin);

#endif
