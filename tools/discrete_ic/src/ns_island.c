#include "discrete_ic/ns_island.h"

#include "discrete_ic/bus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct NsIslandExtra {
    NsHealth health;
    /* pending wire list applied on eval */
    struct {
        char ref_a[16];
        char pin_a[16];
        char ref_b[16];
        char pin_b[16];
        int used;
    } wires[64];
    int wire_count;
} NsIslandExtra;

static void island_eval_wires(NsIsland *island) {
    NsIslandExtra *x;
    int i;
    if (!island || !island->impl) {
        return;
    }
    x = (NsIslandExtra *)island->impl;
    for (i = 0; i < x->wire_count; i++) {
        NsEntity *a = ns_island_find(island, x->wires[i].ref_a);
        NsEntity *b = ns_island_find(island, x->wires[i].ref_b);
        NsLevel la;
        if (!a || !b) {
            continue;
        }
        la = ns_entity_sense(a, x->wires[i].pin_a);
        ns_entity_drive(b, x->wires[i].pin_b, la);
    }
}

static void island_eval(NsIsland *island) {
    int i;
    island_eval_wires(island);
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_eval(island->entities[i]);
    }
    island_eval_wires(island);
}

static void island_reset(NsIsland *island) {
    int i;
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_reset(island->entities[i]);
    }
}

static void island_tick(NsIsland *island) {
    int i;
    for (i = 0; i < island->entity_count; i++) {
        ns_entity_tick(island->entities[i]);
    }
}

static const NsIslandVTable k_vt = {NULL, NULL, island_reset, island_eval, island_tick};

NsIsland *ns_island_create(const char *name) {
    NsIsland *island = (NsIsland *)calloc(1, sizeof(NsIsland));
    NsIslandExtra *x;
    if (!island) {
        return NULL;
    }
    x = (NsIslandExtra *)calloc(1, sizeof(NsIslandExtra));
    if (!x) {
        free(island);
        return NULL;
    }
    x->health = NS_HEALTH_OK;
    ns_island_setup(island, &k_vt, name ? name : "island", 0, 0, 200, 120, x);
    return island;
}

void ns_island_destroy(NsIsland *island) {
    int i;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        ns_chip_destroy(island->entities[i]);
        island->entities[i] = NULL;
    }
    free(island->impl);
    free(island);
}

int ns_island_add(NsIsland *island, NsEntity *entity) {
    return ns_island_add_entity(island, entity);
}

NsEntity *ns_island_find(NsIsland *island, const char *refdes) {
    int i;
    if (!island || !refdes) {
        return NULL;
    }
    for (i = 0; i < island->entity_count; i++) {
        NsEntity *e = island->entities[i];
        if (e && e->refdes && strcmp(e->refdes, refdes) == 0) {
            return e;
        }
    }
    return NULL;
}

int ns_island_wire(NsIsland *island, const char *ref_a, const char *pin_a, const char *ref_b,
                   const char *pin_b) {
    NsIslandExtra *x;
    int i;
    if (!island || !island->impl || !ref_a || !pin_a || !ref_b || !pin_b) {
        return -1;
    }
    x = (NsIslandExtra *)island->impl;
    if (x->wire_count >= 64) {
        return -1;
    }
    i = x->wire_count++;
    snprintf(x->wires[i].ref_a, sizeof(x->wires[i].ref_a), "%s", ref_a);
    snprintf(x->wires[i].pin_a, sizeof(x->wires[i].pin_a), "%s", pin_a);
    snprintf(x->wires[i].ref_b, sizeof(x->wires[i].ref_b), "%s", ref_b);
    snprintf(x->wires[i].pin_b, sizeof(x->wires[i].pin_b), "%s", pin_b);
    x->wires[i].used = 1;
    return 0;
}

void ns_island_set_health(NsIsland *island, NsHealth health) {
    NsIslandExtra *x;
    if (!island || !island->impl) {
        return;
    }
    x = (NsIslandExtra *)island->impl;
    x->health = health;
}

NsHealth ns_island_health(const NsIsland *island) {
    const NsIslandExtra *x;
    if (!island || !island->impl) {
        return NS_HEALTH_BOOT;
    }
    x = (const NsIslandExtra *)island->impl;
    return x->health;
}
