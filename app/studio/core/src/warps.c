#include "retr01_studio/warps.h"
#include "retr01_studio/play.h"

#include <stdio.h>
#include <string.h>

void r01_world_warps_init(R01World *w) {
    if (!w) {
        return;
    }
    memset(w->warp_entrances, 0, sizeof(w->warp_entrances));
    memset(w->warp_exits, 0, sizeof(w->warp_exits));
    w->warp_entrance_count = 0;
    w->warp_exit_count = 0;
}

static int warp_entrance_slot(R01World *w) {
    int i;
    for (i = 0; i < R01_MAX_WARP_ENTRANCES; i++) {
        if (!w->warp_entrances[i].present) {
            return i;
        }
    }
    return -1;
}

int r01_world_warp_entrance_add(R01World *w, int screen_col, int screen_row, int tile_col,
                                int tile_row) {
    R01WarpEntrance *e;
    int idx;
    if (!w || screen_col < 0 || screen_col > 7 || screen_row < 0 || screen_row > 7 || tile_col < 0 ||
        tile_col >= R01_SCREEN_TILES_X || tile_row < 0 || tile_row >= R01_SCREEN_TILES_Y) {
        return -1;
    }
    idx = r01_world_warp_entrance_at(w, screen_col, screen_row, tile_col, tile_row);
    if (idx >= 0) {
        return idx;
    }
    idx = warp_entrance_slot(w);
    if (idx < 0) {
        return -1;
    }
    e = &w->warp_entrances[idx];
    memset(e, 0, sizeof(*e));
    e->present = 1;
    snprintf(e->id, sizeof(e->id), "w_%02d", idx);
    e->screen_col = screen_col;
    e->screen_row = screen_row;
    e->tile_col = tile_col;
    e->tile_row = tile_row;
    if (idx >= w->warp_entrance_count) {
        w->warp_entrance_count = idx + 1;
    }
    return idx;
}

int r01_world_warp_exit_set(R01World *w, int entrance_idx, int dest_screen_col, int dest_screen_row,
                            int dest_tile_col, int dest_tile_row, uint8_t flags) {
    R01WarpExit *x;
    int i;
    int slot = -1;
    if (!w || entrance_idx < 0 || entrance_idx >= R01_MAX_WARP_ENTRANCES ||
        !w->warp_entrances[entrance_idx].present || dest_screen_col < 0 || dest_screen_col > 7 ||
        dest_screen_row < 0 || dest_screen_row > 7 || dest_tile_col < 0 ||
        dest_tile_col >= R01_SCREEN_TILES_X || dest_tile_row < 0 ||
        dest_tile_row >= R01_SCREEN_TILES_Y) {
        return -1;
    }
    for (i = 0; i < R01_MAX_WARP_EXITS; i++) {
        if (w->warp_exits[i].present && w->warp_exits[i].entrance_idx == entrance_idx) {
            slot = i;
            break;
        }
        if (slot < 0 && !w->warp_exits[i].present) {
            slot = i;
        }
    }
    if (slot < 0) {
        return -1;
    }
    x = &w->warp_exits[slot];
    memset(x, 0, sizeof(*x));
    x->present = 1;
    x->entrance_idx = entrance_idx;
    x->dest_screen_col = dest_screen_col;
    x->dest_screen_row = dest_screen_row;
    x->dest_tile_col = dest_tile_col;
    x->dest_tile_row = dest_tile_row;
    x->flags = flags;
    if (slot >= w->warp_exit_count) {
        w->warp_exit_count = slot + 1;
    }
    return slot;
}

int r01_world_warp_entrance_at(const R01World *w, int screen_col, int screen_row, int tile_col,
                               int tile_row) {
    int i;
    if (!w) {
        return -1;
    }
    for (i = 0; i < w->warp_entrance_count; i++) {
        const R01WarpEntrance *e = &w->warp_entrances[i];
        if (!e->present) {
            continue;
        }
        if (e->screen_col == screen_col && e->screen_row == screen_row && e->tile_col == tile_col &&
            e->tile_row == tile_row) {
            return i;
        }
    }
    return -1;
}

int r01_world_warp_exit_for_entrance(const R01World *w, int entrance_idx) {
    int i;
    if (!w || entrance_idx < 0) {
        return -1;
    }
    for (i = 0; i < w->warp_exit_count; i++) {
        if (w->warp_exits[i].present && w->warp_exits[i].entrance_idx == entrance_idx) {
            return i;
        }
    }
    return -1;
}

void r01_world_warp_tile_world_pos(int screen_col, int screen_row, int tile_col, int tile_row,
                                   int *out_wx, int *out_wy) {
    if (out_wx) {
        *out_wx = screen_col * R01_SCREEN_PX_W + tile_col * 8;
    }
    if (out_wy) {
        *out_wy = screen_row * R01_SCREEN_PX_H + tile_row * 8;
    }
}

static int rects_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

int r01_world_warp_entrance_hit(const R01World *w, int player_x, int player_y, int player_w,
                                int player_h) {
    int i;
    if (!w) {
        return -1;
    }
    for (i = 0; i < w->warp_entrance_count; i++) {
        const R01WarpEntrance *e = &w->warp_entrances[i];
        int tx, ty;
        if (!e->present) {
            continue;
        }
        r01_world_warp_tile_world_pos(e->screen_col, e->screen_row, e->tile_col, e->tile_row, &tx,
                                      &ty);
        if (rects_overlap(player_x, player_y, player_w, player_h, tx, ty, 8, 8)) {
            return i;
        }
    }
    return -1;
}
