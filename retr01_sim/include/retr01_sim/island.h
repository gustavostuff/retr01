#ifndef RETR01_SIM_ISLAND_H
#define RETR01_SIM_ISLAND_H

#include "retr01_sim/entity.h"

#define R01S_ISLAND_MAX_ENTITIES 16

typedef struct R01sIsland R01sIsland;

typedef struct R01sIslandVTable {
    void (*init)(R01sIsland *island);
    void (*shutdown)(R01sIsland *island);
    void (*reset)(R01sIsland *island);
    void (*eval)(R01sIsland *island);
    void (*tick)(R01sIsland *island);
} R01sIslandVTable;

struct R01sIsland {
    const R01sIslandVTable *vt;
    const char *title;
    int board_x;
    int board_y;
    int board_w;
    int board_h;
    R01sEntity *entities[R01S_ISLAND_MAX_ENTITIES];
    int entity_count;
    void *impl;
};

void r01s_island_setup(R01sIsland *island, const R01sIslandVTable *vt, const char *title, int board_x,
                       int board_y, int board_w, int board_h, void *impl);

int r01s_island_add_entity(R01sIsland *island, R01sEntity *entity);

void r01s_island_init(R01sIsland *island);
void r01s_island_shutdown(R01sIsland *island);
void r01s_island_reset(R01sIsland *island);
void r01s_island_eval(R01sIsland *island);
void r01s_island_tick(R01sIsland *island);

#endif
