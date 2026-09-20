#ifndef NETLIST_SIM_ISLAND_GROUP_H
#define NETLIST_SIM_ISLAND_GROUP_H

#include "netlist_sim/health.h"
#include "netlist_sim/island.h"

#include <stddef.h>

#define NS_MAX_ISLANDS 17

typedef struct NsIslandGroup NsIslandGroup;

typedef struct NsIslandGroupVTable {
    void (*shutdown)(NsIslandGroup *group);
    void (*reset)(NsIslandGroup *group);
    void (*wire)(NsIslandGroup *group);
    void (*step)(NsIslandGroup *group);
    void (*eval_idle)(NsIslandGroup *group);
    void (*status)(NsIslandGroup *group, char *buf, size_t buf_len);
    void (*update_probes)(NsIslandGroup *group, int *probe_vdd, int *probe_phi2, int *probe_resb_low);
    void (*fill_health)(NsIslandGroup *group, NsSystemHealth *out);
} NsIslandGroupVTable;

struct NsIslandGroup {
    const NsIslandGroupVTable *vt;
    NsIsland *islands[NS_MAX_ISLANDS];
    int island_count;
    int running;
    int powered;
    void *impl;
};

void ns_island_group_init(NsIslandGroup *group);

int ns_island_group_add(NsIslandGroup *group, NsIsland *island);

void ns_island_group_bind(NsIslandGroup *group, const NsIslandGroupVTable *vt, void *impl);

void ns_island_group_shutdown(NsIslandGroup *group);
void ns_island_group_reset(NsIslandGroup *group);
void ns_island_group_step(NsIslandGroup *group);
/* When paused: combinatorial settle only. When running: fixed step batch (non-UI / tests). */
void ns_island_group_frame(NsIslandGroup *group);
/* Paused-path settle used by the UI frame loop (running path steps with a wall budget). */
void ns_island_group_eval_idle(NsIslandGroup *group);
void ns_island_group_fill_status(NsIslandGroup *group, char *buf, size_t buf_len);
void ns_island_group_update_probes(NsIslandGroup *group, int *probe_vdd, int *probe_phi2,
                                     int *probe_resb_low);
void ns_island_group_fill_health(NsIslandGroup *group, NsSystemHealth *out);

const NsIsland *ns_island_group_at(const NsIslandGroup *group, int index);
NsIsland *ns_island_group_at_mut(NsIslandGroup *group, int index);
int ns_island_group_count(const NsIslandGroup *group);

#endif
