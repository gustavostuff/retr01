#ifndef DISCRETE_IC_PIN_H
#define DISCRETE_IC_PIN_H

#include "discrete_ic/types.h"

typedef struct NsPin {
    const char *name; /* datasheet name, e.g. "PHI2", "A0" */
    int number;       /* 1-based package pin */
    NsPinDir dir;
    NsLevel level;  /* current driven/sensed level */
} NsPin;

void ns_pin_init(NsPin *pin, int number, const char *name, NsPinDir dir);
void ns_pin_set(NsPin *pin, NsLevel level);
NsLevel ns_pin_get(const NsPin *pin);

#endif
