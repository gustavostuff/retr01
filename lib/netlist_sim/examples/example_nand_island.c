#include "netlist_sim/chip.h"
#include "netlist_sim/ns_island.h"

#include <stdio.h>

extern void example_nand_register(void);

NsIsland *example_nand_island_build(void) {
    NsIsland *island;
    NsEntity *u1;
    NsEntity *u2;

    example_nand_register();
    island = ns_island_create("nand_demo");
    if (!island) {
        return NULL;
    }
    u1 = ns_chip_create("SN74HC00", "U1");
    u2 = ns_chip_create("SN74HC00", "U2");
    if (!u1 || !u2) {
        ns_island_destroy(island);
        return NULL;
    }
    ns_island_add(island, u1);
    ns_island_add(island, u2);
    ns_island_wire(island, "U1", "1Y", "U2", "1A");
    u1->health = NS_HEALTH_OK;
    u2->health = NS_HEALTH_OK;
    ns_island_set_health(island, NS_HEALTH_OK);
    return island;
}
