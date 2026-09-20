#include "netlist_sim/island_group.h"

#include <stdio.h>
#include <string.h>

/*
 * Fixed board steps for non-UI callers (tests / tools). The interactive app
 * budgets wall time instead so the UI can hold ~60 FPS (see ns_app_frame).
 */
#define NS_GROUP_HALF_STEPS_PER_FRAME 32

void ns_island_group_init(NsIslandGroup *group) {
    if (!group) {
        return;
    }
    memset(group, 0, sizeof(*group));
    group->running = 1;
    group->powered = 1;
}

int ns_island_group_add(NsIslandGroup *group, NsIsland *island) {
    if (!group || !island || group->island_count >= NS_MAX_ISLANDS) {
        return -1;
    }
    group->islands[group->island_count++] = island;
    return 0;
}

void ns_island_group_bind(NsIslandGroup *group, const NsIslandGroupVTable *vt, void *impl) {
    if (!group) {
        return;
    }
    group->vt = vt;
    group->impl = impl;
}

void ns_island_group_shutdown(NsIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    if (group->vt && group->vt->shutdown) {
        group->vt->shutdown(group);
    } else {
        for (i = group->island_count - 1; i >= 0; i--) {
            ns_island_shutdown(group->islands[i]);
        }
    }
    group->island_count = 0;
    group->vt = NULL;
    group->impl = NULL;
}

void ns_island_group_reset(NsIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    if (group->vt && group->vt->reset) {
        group->vt->reset(group);
        return;
    }
    for (i = 0; i < group->island_count; i++) {
        ns_island_reset(group->islands[i]);
    }
}

void ns_island_group_step(NsIslandGroup *group) {
    if (!group || !group->powered) {
        return;
    }
    if (group->vt && group->vt->step) {
        group->vt->step(group);
    }
}

void ns_island_group_eval_idle(NsIslandGroup *group) {
    if (!group) {
        return;
    }
    if (group->vt && group->vt->eval_idle) {
        group->vt->eval_idle(group);
    }
}

void ns_island_group_frame(NsIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    if (group->running) {
        if (group->vt && group->vt->step) {
            for (i = 0; i < NS_GROUP_HALF_STEPS_PER_FRAME; i++) {
                ns_island_group_step(group);
            }
        }
    } else {
        ns_island_group_eval_idle(group);
    }
}

void ns_island_group_fill_status(NsIslandGroup *group, char *buf, size_t buf_len) {
    if (!group || !buf || buf_len == 0) {
        return;
    }
    if (group->vt && group->vt->status) {
        group->vt->status(group, buf, buf_len);
    } else {
        buf[0] = '\0';
    }
}

void ns_island_group_update_probes(NsIslandGroup *group, int *probe_vdd, int *probe_phi2,
                                     int *probe_resb_low) {
    if (!group) {
        return;
    }
    if (group->vt && group->vt->update_probes) {
        group->vt->update_probes(group, probe_vdd, probe_phi2, probe_resb_low);
    }
}

void ns_island_group_fill_health(NsIslandGroup *group, NsSystemHealth *out) {
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (group && group->vt && group->vt->fill_health) {
        group->vt->fill_health(group, out);
    } else if (group) {
        out->island_count = group->island_count;
        out->system = NS_HEALTH_WARN;
        snprintf(out->system_label, sizeof(out->system_label), "UNKNOWN");
        snprintf(out->system_detail, sizeof(out->system_detail), "No health hook for this group");
    }
}

const NsIsland *ns_island_group_at(const NsIslandGroup *group, int index) {
    if (!group || index < 0 || index >= group->island_count) {
        return NULL;
    }
    return group->islands[index];
}

NsIsland *ns_island_group_at_mut(NsIslandGroup *group, int index) {
    if (!group || index < 0 || index >= group->island_count) {
        return NULL;
    }
    return group->islands[index];
}

int ns_island_group_count(const NsIslandGroup *group) {
    return group ? group->island_count : 0;
}
