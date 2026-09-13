#ifndef NETLIST_SIM_ISLAND_H
#define NETLIST_SIM_ISLAND_H

#include "netlist_sim/entity.h"

#define NS_ISLAND_MAX_ENTITIES 20

typedef struct NsIsland NsIsland;

typedef struct NsIslandVTable {
    void (*init)(NsIsland *island);
    void (*shutdown)(NsIsland *island);
    void (*reset)(NsIsland *island);
    void (*eval)(NsIsland *island);
    void (*tick)(NsIsland *island);
} NsIslandVTable;

struct NsIsland {
    const NsIslandVTable *vt;
    const char *title;
    int board_x;
    int board_y;
    int board_w;
    int board_h;
    NsEntity *entities[NS_ISLAND_MAX_ENTITIES];
    int entity_count;
    void *impl;
};

void ns_island_setup(NsIsland *island, const NsIslandVTable *vt, const char *title, int board_x,
                       int board_y, int board_w, int board_h, void *impl);

int ns_island_add_entity(NsIsland *island, NsEntity *entity);

void ns_island_init(NsIsland *island);
void ns_island_shutdown(NsIsland *island);
void ns_island_reset(NsIsland *island);
void ns_island_eval(NsIsland *island);
void ns_island_tick(NsIsland *island);

#endif
