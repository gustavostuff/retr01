#include "discrete_ic/board_layout.h"
#include "discrete_ic/island_builder.h"

#include <string.h>

void ns_island_builder_init(NsIslandBuilder *builder) {
    if (!builder) {
        return;
    }
    memset(builder, 0, sizeof(*builder));
    ns_island_group_init(&builder->group);
}

void ns_island_builder_bind(NsIslandBuilder *builder, const NsIslandGroupVTable *vt, void *impl) {
    if (!builder) {
        return;
    }
    ns_island_group_bind(&builder->group, vt, impl);
}

int ns_island_builder_add(NsIslandBuilder *builder, const NsIslandVTable *vt, const char *title,
                            int board_x, int board_y, int board_w, int board_h, void *impl) {
    NsIsland *island;
    if (!builder || builder->island_count >= NS_MAX_ISLANDS) {
        return -1;
    }
    island = &builder->islands[builder->island_count];
    ns_island_setup(island, vt, title, board_x, board_y, board_w, board_h, impl);
    ns_island_init(island);
    return builder->island_count++;
}

static int island_content_x(const NsIsland *island) {
    return island->board_x + NS_ISLAND_PAD_X + NS_CHIP_PIN_OUT;
}

static int island_content_y(const NsIsland *island) {
    return island->board_y + NS_ISLAND_PAD_TOP;
}

void ns_island_builder_mount(NsIslandBuilder *builder, NsEntity *entity, int island_index, int board_x,
                               int board_y) {
    if (!builder || !entity || island_index < 0 || island_index >= builder->island_count ||
        builder->mount_count >= NS_BUILDER_MAX_MOUNT) {
        return;
    }
    board_x = ns_grid_snap(board_x);
    board_y = ns_grid_snap(board_y);
    ns_entity_place(entity, board_x, board_y);
    builder->mounts[builder->mount_count].entity = entity;
    builder->mounts[builder->mount_count].island_index = island_index;
    builder->mounts[builder->mount_count].board_x = board_x;
    builder->mounts[builder->mount_count].board_y = board_y;
    builder->mount_count++;
}

void ns_island_builder_mount_rel(NsIslandBuilder *builder, NsEntity *entity, int island_index, int rel_x,
                                   int rel_y) {
    NsIsland *island;
    if (!builder || island_index < 0 || island_index >= builder->island_count) {
        return;
    }
    island = &builder->islands[island_index];
    ns_island_builder_mount(builder, entity, island_index, island_content_x(island) + rel_x,
                              island_content_y(island) + rel_y);
}

void ns_island_builder_fit_island(NsIslandBuilder *builder, int island_index) {
    NsIsland *island;
    int i;
    int max_rx = 0;
    int max_ry = 0;
    int content_x;
    int content_y;

    if (!builder || island_index < 0 || island_index >= builder->island_count) {
        return;
    }
    island = &builder->islands[island_index];
    content_x = island_content_x(island);
    content_y = island_content_y(island);

    for (i = 0; i < builder->mount_count; i++) {
        const struct NsIslandBuilderMount *m = &builder->mounts[i];
        int rx;
        int ry;
        if (m->island_index != island_index || !m->entity) {
            continue;
        }
        rx = m->board_x - content_x;
        ry = m->board_y - content_y;
        if (rx + m->entity->body_w > max_rx) {
            max_rx = rx + m->entity->body_w;
        }
        if (ry + m->entity->body_h > max_ry) {
            max_ry = ry + m->entity->body_h;
        }
    }

    if (max_rx == 0 && max_ry == 0) {
        max_rx = 30;
        max_ry = 30;
    }
    island->board_w =
        ns_grid_snap_up(NS_ISLAND_PAD_X + NS_CHIP_PIN_OUT + max_rx + NS_CHIP_PIN_OUT + NS_ISLAND_PAD_X);
    island->board_h = ns_grid_snap_up(NS_ISLAND_PAD_TOP + max_ry + NS_ISLAND_PAD_BOTTOM);
}

void ns_island_builder_fit_all(NsIslandBuilder *builder) {
    int i;
    if (!builder) {
        return;
    }
    for (i = 0; i < builder->island_count; i++) {
        ns_island_builder_fit_island(builder, i);
    }
}

void ns_island_builder_arrange(NsIslandBuilder *builder, int start_x, int start_y, int gap, int horizontal) {
    int i;
    int x = start_x;
    int y = start_y;

    if (!builder) {
        return;
    }
    for (i = 0; i < builder->island_count; i++) {
        NsIsland *island = &builder->islands[i];
        int dx = x - island->board_x;
        int dy = y - island->board_y;
        int j;

        if (dx != 0 || dy != 0) {
            island->board_x = x;
            island->board_y = y;
            for (j = 0; j < builder->mount_count; j++) {
                struct NsIslandBuilderMount *m = &builder->mounts[j];
                if (m->island_index != i || !m->entity) {
                    continue;
                }
                m->board_x += dx;
                m->board_y += dy;
                ns_entity_place(m->entity, m->board_x, m->board_y);
            }
        }

        if (horizontal) {
            x += island->board_w + gap;
        } else {
            y += island->board_h + gap;
        }
    }
}

void ns_island_builder_arrange_rows(NsIslandBuilder *builder, int start_x, int start_y, int gap_x,
                                      int gap_y, int max_row_w) {
    int i;
    int x = ns_grid_snap(start_x);
    int y = ns_grid_snap(start_y);
    int row_h = 0;
    int limit;

    if (!builder) {
        return;
    }
    if (max_row_w < 64) {
        max_row_w = 64;
    }
    start_x = x;
    start_y = y;
    limit = start_x + max_row_w;

    for (i = 0; i < builder->island_count; i++) {
        NsIsland *island = &builder->islands[i];
        int dx;
        int dy;
        int j;

        island->board_w = ns_grid_snap_up(island->board_w);
        island->board_h = ns_grid_snap_up(island->board_h);

        if (i > 0 && x > start_x && x + island->board_w > limit) {
            x = start_x;
            y += row_h + gap_y;
            row_h = 0;
        }
        x = ns_grid_snap(x);
        y = ns_grid_snap(y);

        dx = x - island->board_x;
        dy = y - island->board_y;
        if (dx != 0 || dy != 0) {
            island->board_x = x;
            island->board_y = y;
            for (j = 0; j < builder->mount_count; j++) {
                struct NsIslandBuilderMount *m = &builder->mounts[j];
                if (m->island_index != i || !m->entity) {
                    continue;
                }
                m->board_x = ns_grid_snap(m->board_x + dx);
                m->board_y = ns_grid_snap(m->board_y + dy);
                ns_entity_place(m->entity, m->board_x, m->board_y);
            }
        } else {
            island->board_x = x;
            island->board_y = y;
        }

        if (island->board_h > row_h) {
            row_h = island->board_h;
        }
        x += island->board_w + gap_x;
    }
}

int ns_island_builder_finish(NsIslandBuilder *builder) {
    int i;
    if (!builder || builder->island_count == 0) {
        return -1;
    }
    for (i = 0; i < builder->island_count; i++) {
        if (ns_island_group_add(&builder->group, &builder->islands[i]) != 0) {
            return -1;
        }
    }
    ns_island_group_reset(&builder->group);
    return 0;
}

void ns_island_builder_shutdown(NsIslandBuilder *builder) {
    if (!builder) {
        return;
    }
    ns_island_group_shutdown(&builder->group);
    memset(builder, 0, sizeof(*builder));
}

NsIslandGroup *ns_island_builder_group(NsIslandBuilder *builder) {
    return builder ? &builder->group : NULL;
}

int ns_island_builder_count_visual(const NsIslandBuilder *builder, NsEntityVisual visual) {
    int i;
    int n = 0;
    if (!builder) {
        return 0;
    }
    for (i = 0; i < builder->mount_count; i++) {
        NsEntity *e = builder->mounts[i].entity;
        if (e && e->visual == visual) {
            n++;
        }
    }
    return n;
}

int ns_island_builder_count_bom_ic(const NsIslandBuilder *builder) {
    /* Generic: all VIS_IC mounts. Consumers filter support parts themselves. */
    return ns_island_builder_count_visual(builder, NS_ENTITY_VIS_IC);
}
