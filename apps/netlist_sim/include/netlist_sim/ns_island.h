#ifndef NETLIST_SIM_NS_ISLAND_H
#define NETLIST_SIM_NS_ISLAND_H

#include "netlist_sim/chip.h"
#include "netlist_sim/island.h"

/*
 * Lightweight island helpers on top of NsIsland.
 * Island name is a string. Chips are looked up by refdes string when wiring.
 */

NsIsland *ns_island_create(const char *name);
void ns_island_destroy(NsIsland *island);

int ns_island_add(NsIsland *island, NsEntity *entity);

/* Wire by refdes + pin name. Levels: drive a onto b via merge into b (simple link). */
int ns_island_wire(NsIsland *island, const char *ref_a, const char *pin_a, const char *ref_b,
                   const char *pin_b);

NsEntity *ns_island_find(NsIsland *island, const char *refdes);

void ns_island_set_health(NsIsland *island, NsHealth health);
NsHealth ns_island_health(const NsIsland *island);

#endif
