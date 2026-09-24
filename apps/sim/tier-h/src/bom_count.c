#include "retr01_sim/ns_compat.h"

#include <string.h>

/* Retr01 motherboard BOM: VIS_IC mounts excluding support ATtiny pad MCUs. */
int r01s_island_builder_count_bom_ic(const R01sIslandBuilder *builder) {
    int i;
    int n = 0;
    if (!builder) {
        return 0;
    }
    for (i = 0; i < builder->mount_count; i++) {
        R01sEntity *e = builder->mounts[i].entity;
        if (!e || e->visual != R01S_ENTITY_VIS_IC) {
            continue;
        }
        if (e->part && (strncmp(e->part, "ATtiny", 6) == 0 || strncmp(e->part, "ATTINY", 6) == 0)) {
            continue;
        }
        n++;
    }
    return n;
}
