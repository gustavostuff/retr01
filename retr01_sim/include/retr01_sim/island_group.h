#ifndef RETR01_SIM_ISLAND_GROUP_H
#define RETR01_SIM_ISLAND_GROUP_H

#include "retr01_sim/health.h"
#include "retr01_sim/island.h"

#include <stddef.h>

#define R01S_MAX_ISLANDS 16

typedef struct R01sIslandGroup R01sIslandGroup;

typedef struct R01sIslandGroupVTable {
    void (*shutdown)(R01sIslandGroup *group);
    void (*reset)(R01sIslandGroup *group);
    void (*wire)(R01sIslandGroup *group);
    void (*step)(R01sIslandGroup *group);
    void (*eval_idle)(R01sIslandGroup *group);
    void (*status)(R01sIslandGroup *group, char *buf, size_t buf_len);
    void (*update_probes)(R01sIslandGroup *group, int *probe_vdd, int *probe_phi2, int *probe_resb_low);
    void (*fill_health)(R01sIslandGroup *group, R01sSystemHealth *out);
} R01sIslandGroupVTable;

struct R01sIslandGroup {
    const R01sIslandGroupVTable *vt;
    R01sIsland *islands[R01S_MAX_ISLANDS];
    int island_count;
    int running;
    int powered;
    void *impl;
};

void r01s_island_group_init(R01sIslandGroup *group);

int r01s_island_group_add(R01sIslandGroup *group, R01sIsland *island);

void r01s_island_group_bind(R01sIslandGroup *group, const R01sIslandGroupVTable *vt, void *impl);

void r01s_island_group_shutdown(R01sIslandGroup *group);
void r01s_island_group_reset(R01sIslandGroup *group);
void r01s_island_group_step(R01sIslandGroup *group);
void r01s_island_group_frame(R01sIslandGroup *group);
void r01s_island_group_fill_status(R01sIslandGroup *group, char *buf, size_t buf_len);
void r01s_island_group_update_probes(R01sIslandGroup *group, int *probe_vdd, int *probe_phi2,
                                     int *probe_resb_low);
void r01s_island_group_fill_health(R01sIslandGroup *group, R01sSystemHealth *out);

const R01sIsland *r01s_island_group_at(const R01sIslandGroup *group, int index);
R01sIsland *r01s_island_group_at_mut(R01sIslandGroup *group, int index);
int r01s_island_group_count(const R01sIslandGroup *group);

#endif
