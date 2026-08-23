#include "retr01_sim/island_group.h"

#include <string.h>

#define R01S_GROUP_HALF_STEPS_PER_FRAME 8

void r01s_island_group_init(R01sIslandGroup *group) {
    if (!group) {
        return;
    }
    memset(group, 0, sizeof(*group));
    group->running = 1;
    group->powered = 1;
}

int r01s_island_group_add(R01sIslandGroup *group, R01sIsland *island) {
    if (!group || !island || group->island_count >= R01S_MAX_ISLANDS) {
        return -1;
    }
    group->islands[group->island_count++] = island;
    return 0;
}

void r01s_island_group_bind(R01sIslandGroup *group, const R01sIslandGroupVTable *vt, void *impl) {
    if (!group) {
        return;
    }
    group->vt = vt;
    group->impl = impl;
}

void r01s_island_group_shutdown(R01sIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    if (group->vt && group->vt->shutdown) {
        group->vt->shutdown(group);
    } else {
        for (i = group->island_count - 1; i >= 0; i--) {
            r01s_island_shutdown(group->islands[i]);
        }
    }
    group->island_count = 0;
    group->vt = NULL;
    group->impl = NULL;
}

void r01s_island_group_reset(R01sIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    if (group->vt && group->vt->reset) {
        group->vt->reset(group);
        return;
    }
    for (i = 0; i < group->island_count; i++) {
        r01s_island_reset(group->islands[i]);
    }
}

void r01s_island_group_step(R01sIslandGroup *group) {
    if (!group || !group->powered) {
        return;
    }
    if (group->vt && group->vt->step) {
        group->vt->step(group);
    }
}

void r01s_island_group_frame(R01sIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    if (group->running) {
        if (group->vt && group->vt->step) {
            for (i = 0; i < R01S_GROUP_HALF_STEPS_PER_FRAME; i++) {
                r01s_island_group_step(group);
            }
        }
    } else if (group->vt && group->vt->eval_idle) {
        group->vt->eval_idle(group);
    }
}

void r01s_island_group_fill_status(R01sIslandGroup *group, char *buf, size_t buf_len) {
    if (!group || !buf || buf_len == 0) {
        return;
    }
    if (group->vt && group->vt->status) {
        group->vt->status(group, buf, buf_len);
    } else {
        buf[0] = '\0';
    }
}

void r01s_island_group_update_probes(R01sIslandGroup *group, int *probe_vdd, int *probe_phi2,
                                     int *probe_resb_low) {
    if (!group) {
        return;
    }
    if (group->vt && group->vt->update_probes) {
        group->vt->update_probes(group, probe_vdd, probe_phi2, probe_resb_low);
    }
}

const R01sIsland *r01s_island_group_at(const R01sIslandGroup *group, int index) {
    if (!group || index < 0 || index >= group->island_count) {
        return NULL;
    }
    return group->islands[index];
}

int r01s_island_group_count(const R01sIslandGroup *group) {
    return group ? group->island_count : 0;
}
