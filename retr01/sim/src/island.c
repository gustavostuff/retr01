#include "retr01_sim/island.h"

#include <string.h>

void r01s_island_setup(R01sIsland *island, const R01sIslandVTable *vt, const char *title, int board_x,
                       int board_y, int board_w, int board_h, void *impl) {
    if (!island) {
        return;
    }
    memset(island, 0, sizeof(*island));
    island->vt = vt;
    island->title = title;
    island->board_x = board_x;
    island->board_y = board_y;
    island->board_w = board_w;
    island->board_h = board_h;
    island->impl = impl;
}

int r01s_island_add_entity(R01sIsland *island, R01sEntity *entity) {
    if (!island || !entity || island->entity_count >= R01S_ISLAND_MAX_ENTITIES) {
        return -1;
    }
    island->entities[island->entity_count++] = entity;
    return 0;
}

void r01s_island_init(R01sIsland *island) {
    int i;
    if (!island) {
        return;
    }
    if (island->vt && island->vt->init) {
        island->vt->init(island);
    }
    for (i = 0; i < island->entity_count; i++) {
        r01s_entity_reset(island->entities[i]);
    }
}

void r01s_island_shutdown(R01sIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        r01s_entity_destroy(island->entities[i]);
    }
    if (island->vt && island->vt->shutdown) {
        island->vt->shutdown(island);
    }
    island->entity_count = 0;
    island->impl = NULL;
}

void r01s_island_reset(R01sIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        r01s_entity_reset(island->entities[i]);
    }
    if (island->vt && island->vt->reset) {
        island->vt->reset(island);
    }
}

void r01s_island_eval(R01sIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        r01s_entity_eval(island->entities[i]);
    }
    if (island->vt && island->vt->eval) {
        island->vt->eval(island);
    }
}

void r01s_island_tick(R01sIsland *island) {
    int i;
    if (!island) {
        return;
    }
    if (island->vt && island->vt->tick) {
        island->vt->tick(island);
    }
    for (i = 0; i < island->entity_count; i++) {
        r01s_entity_tick(island->entities[i]);
    }
}
