#ifndef R01A_LAB_PARTS_H
#define R01A_LAB_PARTS_H

#include "discrete_ic/entity.h"

#include <stdlib.h>
#include <string.h>

/* DAC ladder R1-R11, the RGBS header, and the LCD. Everything else is digital. */
static inline int r01a_part_on_analog_island(const NsEntity *e) {
    const char *ref;
    int n;
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_DISPLAY || e->visual == NS_ENTITY_VIS_PIN_HDR) {
        return 1;
    }
    ref = e->refdes;
    if (!ref || ref[0] != 'R') {
        return 0;
    }
    n = atoi(ref + 1);
    return n >= 1 && n <= 11;
}

#endif
