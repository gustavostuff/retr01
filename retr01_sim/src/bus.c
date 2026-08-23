#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

R01sPin *r01s_entity_pin_named(R01sEntity *e, const char *name) {
    int i;
    if (!e || !name) {
        return NULL;
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].name && strcmp(e->pins[i].name, name) == 0) {
            return &e->pins[i];
        }
    }
    return NULL;
}

const R01sPin *r01s_entity_pin_named_const(const R01sEntity *e, const char *name) {
    return r01s_entity_pin_named((R01sEntity *)e, name);
}

void r01s_entity_drive(R01sEntity *e, const char *name, R01sLevel level) {
    R01sPin *p = r01s_entity_pin_named(e, name);
    if (p) {
        p->level = level;
    }
}

R01sLevel r01s_entity_sense(const R01sEntity *e, const char *name) {
    const R01sPin *p = r01s_entity_pin_named_const(e, name);
    return p ? p->level : R01S_LVL_Z;
}

int r01s_level_is_low(R01sLevel level) {
    return level == R01S_LVL_L;
}

int r01s_level_is_high(R01sLevel level) {
    return level == R01S_LVL_H;
}

void r01s_bus_write(R01sEntity *e, const char *prefix, int width, uint32_t value) {
    int i;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, (value & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

uint32_t r01s_bus_read(const R01sEntity *e, const char *prefix, int width) {
    int i;
    uint32_t v = 0;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return 0;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            v |= (1u << i);
        }
    }
    return v;
}

void r01s_bus_hiz(R01sEntity *e, const char *prefix, int width) {
    int i;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, R01S_LVL_Z);
    }
}
