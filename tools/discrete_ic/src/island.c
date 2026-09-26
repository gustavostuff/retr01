#include "discrete_ic/island.h"

#include <string.h>

void ns_island_setup(NsIsland *island, const NsIslandVTable *vt, const char *title, int board_x,
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

int ns_island_add_entity(NsIsland *island, NsEntity *entity) {
    if (!island || !entity || island->entity_count >= NS_ISLAND_MAX_ENTITIES) {
        return -1;
    }
    island->entities[island->entity_count++] = entity;
    return 0;
}

int ns_island_remove_entity(NsIsland *island, NsEntity *entity) {
    int i;
    if (!island || !entity) {
        return -1;
    }
    for (i = 0; i < island->entity_count; i++) {
        if (island->entities[i] != entity) {
            continue;
        }
        for (; i < island->entity_count - 1; i++) {
            island->entities[i] = island->entities[i + 1];
        }
        island->entity_count--;
        island->entities[island->entity_count] = NULL;
        return 0;
    }
    return -1;
}

void ns_island_init(NsIsland *island) {
    int i;
    if (!island) {
        return;
    }
    if (island->vt && island->vt->init) {
        island->vt->init(island);
    }
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_reset(island->entities[i]);
    }
}

void ns_island_shutdown(NsIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_destroy(island->entities[i]);
    }
    if (island->vt && island->vt->shutdown) {
        island->vt->shutdown(island);
    }
    island->entity_count = 0;
    island->impl = NULL;
}

void ns_island_reset(NsIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_reset(island->entities[i]);
    }
    if (island->vt && island->vt->reset) {
        island->vt->reset(island);
    }
}

void ns_island_eval(NsIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_eval(island->entities[i]);
    }
    if (island->vt && island->vt->eval) {
        island->vt->eval(island);
    }
}

void ns_island_tick(NsIsland *island) {
    int i;
    if (!island) {
        return;
    }
    if (island->vt && island->vt->tick) {
        island->vt->tick(island);
    }
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_tick(island->entities[i]);
    }
}
