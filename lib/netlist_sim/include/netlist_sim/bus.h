#ifndef NETLIST_SIM_BUS_H
#define NETLIST_SIM_BUS_H

#include "netlist_sim/entity.h"

#include <stdint.h>

NsPin *ns_entity_pin_named(NsEntity *e, const char *name);
const NsPin *ns_entity_pin_named_const(const NsEntity *e, const char *name);

void ns_entity_drive(NsEntity *e, const char *name, NsLevel level);
NsLevel ns_entity_sense(const NsEntity *e, const char *name);

/* Active-low enable: 1 if pin is L. */
int ns_level_is_low(NsLevel level);
int ns_level_is_high(NsLevel level);

const char *ns_level_name(NsLevel level);

/*
 * Merge two drivers onto one net.
 * Z is transparent; H+L (or any hard clash) => fatal bus fight (default) or X if
 * fatal conflicts are disabled (tests only).
 */
NsLevel ns_level_merge(NsLevel a, NsLevel b);
NsLevel ns_level_merge_at(NsLevel a, NsLevel b, const char *net,
                              const char *driver_a, const char *driver_b);

/* Pull-up model: undriven (Z) reads as H. X stays X. */
NsLevel ns_level_pulled(NsLevel level);

/* Fatal on fight is ON by default. Disable only in harnesses that assert on X. */
void ns_bus_set_fatal_conflicts(int enable);
int ns_bus_fatal_conflicts(void);

unsigned ns_bus_conflict_count(void);
void ns_bus_clear_conflicts(void);

/*
 * Drive/sense a numbered bus: prefix "A" + width 16 => A0..A15.
 * write: sets pin levels from value (bit0 = *0).
 * read:  H=>1; Z pulled high (=>1); L=>0; X is a bus fight (fatal by default).
 * hiz:   all bits to Z.
 */
void ns_bus_write(NsEntity *e, const char *prefix, int width, uint32_t value);
uint32_t ns_bus_read(const NsEntity *e, const char *prefix, int width);
void ns_bus_hiz(NsEntity *e, const char *prefix, int width);

/*
 * Resolve two chips' bus bits onto dst. On H+L (or X), aborts with a report of
 * which chips/pins fought (unless fatal conflicts are disabled).
 */
void ns_bus_resolve(NsEntity *dst, const char *dst_prefix, const NsEntity *a, const char *a_prefix,
                      const NsEntity *b, const char *b_prefix, int width);

#endif
