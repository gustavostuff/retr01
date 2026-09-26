#include "discrete_ic/bus.h"
#include "discrete_ic/ns_island.h"
#include "discrete_ic/outline.h"

#include <stdio.h>

extern NsIsland *example_nand_island_build(void);

int main(void) {
    NsIsland *island = example_nand_island_build();
    NsEntity *u1;
    NsEntity *u2;
    NsOutlineRgb rgb;

    if (!island) {
        fprintf(stderr, "example_nand: build failed\n");
        return 1;
    }
    u1 = ns_island_find(island, "U1");
    u2 = ns_island_find(island, "U2");
    ns_entity_drive(u1, "1A", NS_LVL_H);
    ns_entity_drive(u1, "1B", NS_LVL_H);
    ns_entity_drive(u2, "1B", NS_LVL_H);
    ns_island_eval(island);

    rgb = ns_outline_rgb(ns_island_health(island));
    printf("U1.1Y=%s U2.1Y=%s island_health=%s outline_rgb=(%u,%u,%u)\n",
           ns_level_name(ns_entity_sense(u1, "1Y")), ns_level_name(ns_entity_sense(u2, "1Y")),
           ns_health_tag(ns_island_health(island)), (unsigned)rgb.r, (unsigned)rgb.g, (unsigned)rgb.b);

    ns_island_destroy(island);
    return 0;
}
