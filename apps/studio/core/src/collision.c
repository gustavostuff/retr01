#include "retr01_studio/collision.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"
#include "r01_play_collision.h"

#include <string.h>

typedef struct {
    const R01World *w;
    const uint8_t *banks;
    const uint8_t *tiles;
    int count;
} R01WorldAabbCtx;

static int world_aabb_has_screen(void *v, int col, int row) {
    const R01WorldAabbCtx *c = (const R01WorldAabbCtx *)v;
    int idx;
    if (!c || !c->w || col < 0 || col >= R01_GRID_MAX || row < 0 || row >= R01_GRID_MAX) {
        return 0;
    }
    idx = r01_world_find_screen(c->w, col, row);
    if (idx < 0 || idx >= c->w->screen_count) {
        return 0;
    }
    return c->w->screens[idx].present;
}

static int world_aabb_solid_at(void *v, int wx, int wy) {
    const R01WorldAabbCtx *c = (const R01WorldAabbCtx *)v;
    return r01_world_solid_at_list(c->w, wx, wy, c->banks, c->tiles, c->count);
}

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

int r01_ctx_pattern_solid(const uint8_t *banks, const uint8_t *tiles, int count, int bank, int tile) {
    int i;
    if (!banks || !tiles || count < 1) {
        return 0;
    }
    if (bank < 0 || bank >= R01_BG_BANKS || tile < 0 || tile >= R01_TILES_PER_BANK) {
        return 0;
    }
    if (count > R01_SOLID_PAT_MAX) {
        count = R01_SOLID_PAT_MAX;
    }
    for (i = 0; i < count; i++) {
        if ((int)banks[i] == bank && (int)tiles[i] == tile) {
            return 1;
        }
    }
    return 0;
}

int r01_project_pattern_solid(const R01Project *p, int bank, int tile) {
    if (!p) {
        return 0;
    }
    return r01_ctx_pattern_solid(p->solid_pat_bank, p->solid_pat_tile, p->solid_pat_count, bank, tile);
}

void r01_project_copy_solid_pats(const R01Project *p, uint8_t *count, uint8_t *banks, uint8_t *tiles) {
    int n;
    if (!count || !banks || !tiles) {
        return;
    }
    n = p ? p->solid_pat_count : 0;
    if (n < 0) {
        n = 0;
    }
    if (n > R01_SOLID_PAT_MAX) {
        n = R01_SOLID_PAT_MAX;
    }
    *count = (uint8_t)n;
    if (n > 0) {
        memcpy(banks, p->solid_pat_bank, (size_t)n);
        memcpy(tiles, p->solid_pat_tile, (size_t)n);
    }
}

static void sync_screen_solids(R01Project *p, R01Screen *s) {
    int cell;
    if (!s) {
        return;
    }
    for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
        s->solids[cell] =
            r01_project_pattern_solid(p, r01_attr_solid_bank(s->attrs[cell]), (int)s->tiles[cell]) ? 1u : 0u;
    }
}

void r01_project_sync_solids(R01Project *p) {
    int wi, si;
    if (!p) {
        return;
    }
    for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
        R01World *w = &p->worlds[wi];
        for (si = 0; si < w->screen_count; si++) {
            if (w->screens[si].present) {
                sync_screen_solids(p, &w->screens[si]);
            }
        }
    }
}

int r01_project_set_pattern_solid(R01Project *p, int bank, int tile, int on) {
    int i;
    if (!p || bank < 0 || bank >= R01_BG_BANKS || tile < 0 || tile >= R01_TILES_PER_BANK) {
        return 0;
    }
    for (i = 0; i < p->solid_pat_count; i++) {
        if ((int)p->solid_pat_bank[i] == bank && (int)p->solid_pat_tile[i] == tile) {
            if (on) {
                return 1;
            }
            p->solid_pat_count--;
            p->solid_pat_bank[i] = p->solid_pat_bank[p->solid_pat_count];
            p->solid_pat_tile[i] = p->solid_pat_tile[p->solid_pat_count];
            r01_project_sync_solids(p);
            return 0;
        }
    }
    if (!on) {
        return 0;
    }
    if (p->solid_pat_count >= R01_SOLID_PAT_MAX) {
        return 0;
    }
    p->solid_pat_bank[p->solid_pat_count] = (uint8_t)bank;
    p->solid_pat_tile[p->solid_pat_count] = (uint8_t)tile;
    p->solid_pat_count++;
    r01_project_sync_solids(p);
    return 1;
}

int r01_project_toggle_pattern_solid(R01Project *p, int bank, int tile) {
    int on = !r01_project_pattern_solid(p, bank, tile);
    return r01_project_set_pattern_solid(p, bank, tile, on);
}

int r01_world_solid_at_list(const R01World *w, int wx, int wy, const uint8_t *banks, const uint8_t *tiles,
                            int count) {
    const R01Screen *s;
    int lx, ly, tx, ty, cell;
    if (!world_screen_at_pixel(w, wx, wy, &s, &lx, &ly)) {
        return 0;
    }
    tx = lx / 8;
    ty = ly / 8;
    cell = ty * R01_SCREEN_TILES_X + tx;
    return r01_ctx_pattern_solid(banks, tiles, count, r01_attr_solid_bank(s->attrs[cell]), (int)s->tiles[cell]);
}

int r01_world_solid_at(const R01Project *p, const R01World *w, int wx, int wy) {
    if (!p) {
        return 0;
    }
    return r01_world_solid_at_list(w, wx, wy, p->solid_pat_bank, p->solid_pat_tile, p->solid_pat_count);
}

int r01_world_aabb_ok_list(const R01World *w, int px, int py, int bw, int bh, const uint8_t *banks,
                           const uint8_t *tiles, int count) {
    R01WorldAabbCtx ctx;
    if (!w) {
        return 0;
    }
    ctx.w = w;
    ctx.banks = banks;
    ctx.tiles = tiles;
    ctx.count = count;
    return r01_play_aabb_ok(px, py, bw, bh, R01_SCREEN_PX_W, R01_SCREEN_PX_H, world_aabb_has_screen,
                            world_aabb_solid_at, &ctx);
}

int r01_world_aabb_ok(const R01Project *p, const R01World *w, int px, int py, int bw, int bh) {
    if (!p) {
        return 0;
    }
    return r01_world_aabb_ok_list(w, px, py, bw, bh, p->solid_pat_bank, p->solid_pat_tile, p->solid_pat_count);
}

int r01_world_player_aabb_ok(const R01Project *p, const R01World *w, int px, int py) {
    return r01_world_aabb_ok(p, w, px, py, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H);
}
