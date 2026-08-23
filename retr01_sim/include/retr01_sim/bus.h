#ifndef RETR01_SIM_BUS_H
#define RETR01_SIM_BUS_H

#include "retr01_sim/entity.h"

#include <stdint.h>

R01sPin *r01s_entity_pin_named(R01sEntity *e, const char *name);
const R01sPin *r01s_entity_pin_named_const(const R01sEntity *e, const char *name);

void r01s_entity_drive(R01sEntity *e, const char *name, R01sLevel level);
R01sLevel r01s_entity_sense(const R01sEntity *e, const char *name);

/* Active-low enable: 1 if pin is L. */
int r01s_level_is_low(R01sLevel level);
int r01s_level_is_high(R01sLevel level);

/*
 * Drive/sense a numbered bus: prefix "A" + width 16 => A0..A15.
 * write: sets pin levels from value (bit0 = *0).
 * read:  H=>1, else 0 (Z/X count as 0).
 * hiz:   all bits to Z.
 */
void r01s_bus_write(R01sEntity *e, const char *prefix, int width, uint32_t value);
uint32_t r01s_bus_read(const R01sEntity *e, const char *prefix, int width);
void r01s_bus_hiz(R01sEntity *e, const char *prefix, int width);

#endif
