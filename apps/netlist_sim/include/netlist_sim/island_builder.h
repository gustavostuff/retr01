#ifndef NETLIST_SIM_ISLAND_BUILDER_H
#define NETLIST_SIM_ISLAND_BUILDER_H

#include "netlist_sim/entity.h"
#include "netlist_sim/island.h"
#include "netlist_sim/island_group.h"

#define NS_BUILDER_MAX_MOUNT 64

struct NsUi;

/*
 * Assembles N islands into a group, records chip placements, and mounts the UI.
 * Board recipes (which islands, wiring) live at the call site: typically main.c.
 */
typedef struct NsIslandBuilder {
    NsIslandGroup group;
    NsIsland islands[NS_MAX_ISLANDS];
    int island_count;
    struct NsIslandBuilderMount {
        NsEntity *entity;
        int island_index;
        int board_x;
        int board_y;
    } mounts[NS_BUILDER_MAX_MOUNT];
    int mount_count;
} NsIslandBuilder;

void ns_island_builder_init(NsIslandBuilder *builder);

void ns_island_builder_bind(NsIslandBuilder *builder, const NsIslandGroupVTable *vt, void *impl);

/* Returns island index, or -1 on failure. Calls ns_island_init before add. */
int ns_island_builder_add(NsIslandBuilder *builder, const NsIslandVTable *vt, const char *title,
                            int board_x, int board_y, int board_w, int board_h, void *impl);

void ns_island_builder_mount(NsIslandBuilder *builder, NsEntity *entity, int island_index, int board_x,
                               int board_y);

/* Place chip relative to island content area (inside pad + pin stub margin). */
void ns_island_builder_mount_rel(NsIslandBuilder *builder, NsEntity *entity, int island_index, int rel_x,
                                 int rel_y);

/* Shrink island frame(s) to tightly wrap mounted chips. */
void ns_island_builder_fit_island(NsIslandBuilder *builder, int island_index);
void ns_island_builder_fit_all(NsIslandBuilder *builder);

/*
 * Repack island frames (and their chips) starting at (start_x, start_y).
 * horizontal: 1 = left-to-right row, 0 = top-to-bottom column.
 */
void ns_island_builder_arrange(NsIslandBuilder *builder, int start_x, int start_y, int gap, int horizontal);

/*
 * Pack left-to-right, wrapping to the next row when the next island would exceed
 * start_x + max_row_w. Prefer this for the default board so islands stay visible
 * with vertical pan instead of one long horizontal strip.
 */
void ns_island_builder_arrange_rows(NsIslandBuilder *builder, int start_x, int start_y, int gap_x,
                                      int gap_y, int max_row_w);

/* Adds all islands to the group and runs group reset. Returns 0 ok, -1 if empty. */
int ns_island_builder_finish(NsIslandBuilder *builder);

void ns_island_builder_shutdown(NsIslandBuilder *builder);

NsIslandGroup *ns_island_builder_group(NsIslandBuilder *builder);

/* Count mounted entities with a given canvas visual (e.g. NS_ENTITY_VIS_IC for BOM DIP count). */
int ns_island_builder_count_visual(const NsIslandBuilder *builder, NsEntityVisual visual);

/* Count VIS_IC mounts (generic BOM-style IC count). */
int ns_island_builder_count_bom_ic(const NsIslandBuilder *builder);

#endif
