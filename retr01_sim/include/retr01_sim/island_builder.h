#ifndef RETR01_SIM_ISLAND_BUILDER_H
#define RETR01_SIM_ISLAND_BUILDER_H

#include "retr01_sim/entity.h"
#include "retr01_sim/island.h"
#include "retr01_sim/island_group.h"

#define R01S_BUILDER_MAX_MOUNT 64

struct R01sUi;

/*
 * Assembles N islands into a group, records chip placements, and mounts the UI.
 * Board recipes (which islands, wiring) live at the call site: typically main.c.
 */
typedef struct R01sIslandBuilder {
    R01sIslandGroup group;
    R01sIsland islands[R01S_MAX_ISLANDS];
    int island_count;
    struct R01sIslandBuilderMount {
        R01sEntity *entity;
        int island_index;
        int board_x;
        int board_y;
    } mounts[R01S_BUILDER_MAX_MOUNT];
    int mount_count;
} R01sIslandBuilder;

void r01s_island_builder_init(R01sIslandBuilder *builder);

void r01s_island_builder_bind(R01sIslandBuilder *builder, const R01sIslandGroupVTable *vt, void *impl);

/* Returns island index, or -1 on failure. Calls r01s_island_init before add. */
int r01s_island_builder_add(R01sIslandBuilder *builder, const R01sIslandVTable *vt, const char *title,
                            int board_x, int board_y, int board_w, int board_h, void *impl);

void r01s_island_builder_mount(R01sIslandBuilder *builder, R01sEntity *entity, int island_index, int board_x,
                               int board_y);

/* Place chip relative to island content area (inside pad + pin stub margin). */
void r01s_island_builder_mount_rel(R01sIslandBuilder *builder, R01sEntity *entity, int island_index, int rel_x,
                                 int rel_y);

/* Shrink island frame(s) to tightly wrap mounted chips. */
void r01s_island_builder_fit_island(R01sIslandBuilder *builder, int island_index);
void r01s_island_builder_fit_all(R01sIslandBuilder *builder);

/*
 * Repack island frames (and their chips) starting at (start_x, start_y).
 * horizontal: 1 = left-to-right row, 0 = top-to-bottom column.
 */
void r01s_island_builder_arrange(R01sIslandBuilder *builder, int start_x, int start_y, int gap, int horizontal);

/*
 * Pack left-to-right, wrapping to the next row when the next island would exceed
 * start_x + max_row_w. Prefer this for the default board so islands stay visible
 * with vertical pan instead of one long horizontal strip.
 */
void r01s_island_builder_arrange_rows(R01sIslandBuilder *builder, int start_x, int start_y, int gap_x,
                                      int gap_y, int max_row_w);

/* Adds all islands to the group and runs group reset. Returns 0 ok, -1 if empty. */
int r01s_island_builder_finish(R01sIslandBuilder *builder);

void r01s_island_builder_shutdown(R01sIslandBuilder *builder);

R01sIslandGroup *r01s_island_builder_group(R01sIslandBuilder *builder);

/* Count mounted entities with a given canvas visual (e.g. R01S_ENTITY_VIS_IC for BOM DIP count). */
int r01s_island_builder_count_visual(const R01sIslandBuilder *builder, R01sEntityVisual visual);

#endif
