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

const char *r01s_level_name(R01sLevel level);

/*
 * Merge two drivers onto one net.
 * Z is transparent; H+L (or any hard clash) => fatal bus fight (default) or X if
 * fatal conflicts are disabled (tests only).
 */
R01sLevel r01s_level_merge(R01sLevel a, R01sLevel b);
R01sLevel r01s_level_merge_at(R01sLevel a, R01sLevel b, const char *net,
                              const char *driver_a, const char *driver_b);

/* Pull-up model: undriven (Z) reads as H. X stays X. */
R01sLevel r01s_level_pulled(R01sLevel level);

/* Fatal on fight is ON by default. Disable only in harnesses that assert on X. */
void r01s_bus_set_fatal_conflicts(int enable);
int r01s_bus_fatal_conflicts(void);

unsigned r01s_bus_conflict_count(void);
void r01s_bus_clear_conflicts(void);

/*
 * Drive/sense a numbered bus: prefix "A" + width 16 => A0..A15.
 * write: sets pin levels from value (bit0 = *0).
 * read:  H=>1; Z pulled high (=>1); L=>0; X is a bus fight (fatal by default).
 * hiz:   all bits to Z.
 */
void r01s_bus_write(R01sEntity *e, const char *prefix, int width, uint32_t value);
uint32_t r01s_bus_read(const R01sEntity *e, const char *prefix, int width);
void r01s_bus_hiz(R01sEntity *e, const char *prefix, int width);

/*
 * Resolve two chips' bus bits onto dst. On H+L (or X), aborts with a report of
 * which chips/pins fought (unless fatal conflicts are disabled).
 */
void r01s_bus_resolve(R01sEntity *dst, const char *dst_prefix, const R01sEntity *a, const char *a_prefix,
                      const R01sEntity *b, const char *b_prefix, int width);

#endif
