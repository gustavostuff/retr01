#ifndef DISCRETE_IC_OUTLINE_H
#define DISCRETE_IC_OUTLINE_H

#include "discrete_ic/health.h"

/* 1px outline RGB for IC / island frames. */
typedef struct NsOutlineRgb {
    unsigned char r, g, b;
} NsOutlineRgb;

static inline NsOutlineRgb ns_outline_rgb(NsHealth h) {
    NsOutlineRgb c;
    switch (h) {
    case NS_HEALTH_OK:
        c.r = 40;
        c.g = 200;
        c.b = 80;
        break;
    case NS_HEALTH_WARN:
        c.r = 220;
        c.g = 180;
        c.b = 40;
        break;
    case NS_HEALTH_FAIL:
        c.r = 220;
        c.g = 50;
        c.b = 50;
        break;
    default: /* BOOT */
        c.r = 120;
        c.g = 120;
        c.b = 130;
        break;
    }
    return c;
}

#endif
