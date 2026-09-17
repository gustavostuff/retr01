#include "retr01_studio/palette.h"
#include "retr01_studio/chr_pack.h"
#include "r01_kit_palette.h"

#include <string.h>

uint8_t r01_quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t rr = (uint8_t)((r * 7 + 127) / 255);
    uint8_t gg = (uint8_t)((g * 7 + 127) / 255);
    uint8_t bb = (uint8_t)((b * 3 + 127) / 255);
    return (uint8_t)((rr << 5) | (gg << 2) | bb);
}

int r01_kit_nearest_master(uint8_t r, uint8_t g, uint8_t b) {
    int best = 0;
    int best_d = 0x7fffffff;
    int i;
    for (i = 0; i < R01_KIT_COLORS; i++) {
        int dr = (int)r - (int)R01_KIT_RGB[i][0];
        int dg = (int)g - (int)R01_KIT_RGB[i][1];
        int db = (int)b - (int)R01_KIT_RGB[i][2];
        int d = dr * dr + dg * dg + db * db;
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static void pal_phase1_bg(R01PalRow *pal, int column) {
    int col = column & 3;
    pal->idx[0] = 0;
    pal->idx[1] = (uint8_t)(16 + col);
    pal->idx[2] = (uint8_t)(32 + col);
    pal->idx[3] = (uint8_t)(48 + col);
}

static void pal_phase1_spr(R01PalRow *pal) {
    pal->idx[0] = 0;
    pal->idx[1] = (uint8_t)R01_KIT_RED_MASTER;
    pal->idx[2] = (uint8_t)R01_KIT_RED_MASTER;
    pal->idx[3] = (uint8_t)R01_KIT_RED_MASTER;
}

uint8_t r01_project_player_master(const R01Project *p) {
    if (!p) {
        return (uint8_t)R01_KIT_RED_MASTER;
    }
    return p->global_pal_spr[R01_PLAYER_SPR_ROW][R01_PLAYER_SPR_PAL].idx[R01_PLAYER_SPR_COLOR];
}

void r01_project_player_rgb(const R01Project *p, uint8_t *r, uint8_t *g, uint8_t *b) {
    r01_kit_rgb(r01_project_player_master(p), r, g, b);
}

void r01_project_init_phase1_pals(R01Project *p) {
    int row, pal;
    if (!p) {
        return;
    }
    for (row = 0; row < R01_PAL_ROWS; row++) {
        for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
            /* Kit column = pal within row; rows 4-7 repeat columns 0-3. */
            pal_phase1_bg(&p->global_pal_bg[row][pal], pal);
            pal_phase1_spr(&p->global_pal_spr[row][pal]);
        }
    }
}

void r01_project_set_bg_pals_from_png(R01Project *p, const uint8_t master_for_index[4]) {
    R01PalRow row;
    int r, pal;
    if (!p || !master_for_index) {
        return;
    }
    row.idx[0] = master_for_index[0];
    row.idx[1] = master_for_index[1];
    row.idx[2] = master_for_index[2];
    row.idx[3] = master_for_index[3];
    for (r = 0; r < R01_PAL_ROWS; r++) {
        for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
            p->global_pal_bg[r][pal] = row;
            p->global_pal_spr[r][pal].idx[0] = row.idx[0];
        }
    }
}

static int clamp_pal_row(int row) {
    if (row < 0) {
        return 0;
    }
    if (row >= R01_PAL_ROWS) {
        return R01_PAL_ROWS - 1;
    }
    return row;
}

void r01_project_backdrop_rgb(const R01Project *p, const R01World *w, uint8_t *r, uint8_t *g, uint8_t *b) {
    int prow;
    uint8_t master = 0;
    if (p) {
        prow = clamp_pal_row(w ? w->default_pal_row : 0);
        master = p->global_pal_bg[prow][0].idx[0];
    }
    r01_kit_rgb(master, r, g, b);
}

uint8_t r01_screen_pixel_color(const R01World *w, const R01Screen *s, int px, int py) {
    int tx, ty, sx, sy, cell, bank;
    uint8_t attr, tile_id;
    const uint8_t *tile;
    if (!s || px < 0 || py < 0 || px >= R01_SCREEN_PX_W || py >= R01_SCREEN_PX_H) {
        return 0;
    }
    tx = px / 8;
    ty = py / 8;
    sx = px % 8;
    sy = py % 8;
    cell = ty * R01_SCREEN_TILES_X + tx;
    attr = s->attrs[cell];
    tile_id = s->tiles[cell];
    bank = r01_attr_bank(attr);
    if (r01_attr_flip_h(attr)) {
        sx = 7 - sx;
    }
    if (r01_attr_flip_v(attr)) {
        sy = 7 - sy;
    }
    if (w && bank >= 0 && bank < R01_BG_BANKS && tile_id < (uint8_t)w->bg_banks[bank].tile_count) {
        tile = w->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
        return r01_tile_pixel_color(tile, sx, sy) & 3u;
    }
    return s->pixels[(ty * 8 + sy) * R01_SCREEN_PX_W + (tx * 8 + sx)] & 3u;
}

void r01_screen_pixel_rgb(const R01Project *p, const R01World *w, const R01Screen *s, int px, int py, uint8_t *r,
                          uint8_t *g, uint8_t *b) {
    int tx, ty, cell, prow;
    uint8_t attr, color, master;
    if (!p || !s || px < 0 || py < 0 || px >= R01_SCREEN_PX_W || py >= R01_SCREEN_PX_H) {
        if (r) {
            *r = 0;
        }
        if (g) {
            *g = 0;
        }
        if (b) {
            *b = 0;
        }
        return;
    }
    tx = px / 8;
    ty = py / 8;
    cell = ty * R01_SCREEN_TILES_X + tx;
    attr = s->attrs[cell];
    color = r01_screen_pixel_color(w, s, px, py);
    prow = clamp_pal_row(w ? w->default_pal_row : 0);
    master = p->global_pal_bg[prow][r01_attr_pal(attr)].idx[color];
    r01_kit_rgb(master, r, g, b);
}

void r01_compose_screen_pixel_rgb(const R01Project *p, const R01World *w, const R01Screen *bg1,
                                  const R01Screen *bg0, int px, int py, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t col1;
    if (!p) {
        if (r) {
            *r = 0;
        }
        if (g) {
            *g = 0;
        }
        if (b) {
            *b = 0;
        }
        return;
    }
    if (!bg1) {
        if (bg0) {
            r01_screen_pixel_rgb(p, w, bg0, px, py, r, g, b);
            if (r01_screen_pixel_color(w, bg0, px, py) == 0) {
                r01_project_backdrop_rgb(p, w, r, g, b);
            }
        } else {
            r01_project_backdrop_rgb(p, w, r, g, b);
        }
        return;
    }
    col1 = r01_screen_pixel_color(w, bg1, px, py);
    if (col1 != 0) {
        r01_screen_pixel_rgb(p, w, bg1, px, py, r, g, b);
        return;
    }
    /* BG1 color 0 show-through: BG0 pixel, else shared backdrop. */
    if (bg0) {
        if (r01_screen_pixel_color(w, bg0, px, py) != 0) {
            r01_screen_pixel_rgb(p, w, bg0, px, py, r, g, b);
            return;
        }
    }
    r01_project_backdrop_rgb(p, w, r, g, b);
}
