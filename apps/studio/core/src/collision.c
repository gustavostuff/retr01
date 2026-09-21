#include "retr01_studio/collision.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

static int world_screen_at_pixel(const R01World *w, int wx, int wy, const R01Screen **out_screen, int *out_lx,
                                 int *out_ly) {
    int col, row, idx;
    if (!w || wx < 0 || wy < 0) {
        return 0;
    }
    col = wx / R01_SCREEN_PX_W;
    row = wy / R01_SCREEN_PX_H;
    idx = r01_world_find_screen(w, col, row);
    if (idx < 0 || idx >= w->screen_count) {
        return 0;
    }
    if (!w->screens[idx].present) {
        return 0;
    }
    if (out_screen) {
        *out_screen = &w->screens[idx];
    }
    if (out_lx) {
        *out_lx = wx % R01_SCREEN_PX_W;
    }
    if (out_ly) {
        *out_ly = wy % R01_SCREEN_PX_H;
    }
    return 1;
}

int r01_world_attr_at(const R01World *w, int wx, int wy, uint8_t *out_attr) {
    const R01Screen *s;
    int lx, ly, tx, ty, cell;
    if (!world_screen_at_pixel(w, wx, wy, &s, &lx, &ly)) {
        return -1;
    }
    tx = lx / 8;
    ty = ly / 8;
    cell = ty * R01_SCREEN_TILES_X + tx;
    if (out_attr) {
        *out_attr = s->attrs[cell];
    }
    return 0;
}

int r01_world_solid_at(const R01World *w, int wx, int wy) {
    const R01Screen *s;
    int lx, ly, tx, ty, cell;
    if (!world_screen_at_pixel(w, wx, wy, &s, &lx, &ly)) {
        return 0;
    }
    tx = lx / 8;
    ty = ly / 8;
    cell = ty * R01_SCREEN_TILES_X + tx;
    return r01_screen_cell_is_solid(s, cell);
}

int r01_world_aabb_ok(const R01World *w, int px, int py, int bw, int bh) {
    int x1, y1, c0, c1, r0, r1, c, r;
    int tx0, ty0, tx1, ty1, tx, ty;
    const int tile = 8;
    if (!w || px < 0 || py < 0 || bw < 1 || bh < 1) {
        return 0;
    }
    x1 = px + bw - 1;
    y1 = py + bh - 1;
    c0 = px / R01_SCREEN_PX_W;
    c1 = x1 / R01_SCREEN_PX_W;
    r0 = py / R01_SCREEN_PX_H;
    r1 = y1 / R01_SCREEN_PX_H;
    for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
            int idx = r01_world_find_screen(w, c, r);
            if (idx < 0 || !w->screens[idx].present) {
                return 0;
            }
        }
    }
    /*
     * Probe every BG tile the AABB overlaps. Corner-only samples miss solids that
     * hit the middle of an edge (e.g. a single-tile jut on a vertical wall when
     * the hitbox is taller than one tile).
     */
    tx0 = px / tile;
    ty0 = py / tile;
    tx1 = x1 / tile;
    ty1 = y1 / tile;
    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            int wx = tx * tile;
            int wy = ty * tile;
            if (wx < px) {
                wx = px;
            }
            if (wy < py) {
                wy = py;
            }
            if (wx > x1) {
                wx = x1;
            }
            if (wy > y1) {
                wy = y1;
            }
            if (r01_world_solid_at(w, wx, wy)) {
                return 0;
            }
        }
    }
    return 1;
}

int r01_world_player_aabb_ok(const R01World *w, int px, int py) {
    return r01_world_aabb_ok(w, px, py, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H);
}

int r01_world_apply_solid_hw(R01World *w, uint8_t hw_key, int set_solid) {
    int si, cell, touched = 0;
    if (!w) {
        return 0;
    }
    hw_key &= R01_ATTR_HW_MASK;
    for (si = 0; si < w->screen_count; si++) {
        R01Screen *s = &w->screens[si];
        if (!s->present) {
            continue;
        }
        for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
            if (s->tiles[cell] == 0) {
                if (!set_solid && s->solids[cell]) {
                    s->solids[cell] = 0;
                    touched++;
                }
                continue;
            }
            if (r01_attr_hw(s->attrs[cell]) != hw_key) {
                continue;
            }
            s->solids[cell] = set_solid ? 1u : 0u;
            touched++;
        }
    }
    return touched;
}
