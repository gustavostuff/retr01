#include "ui/widgets/widgets.h"
#include "ui/internal.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <string.h>

int ui_compose_clamp_part(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > R01_ENTITY_COMPOSE_PX - 8) {
        return R01_ENTITY_COMPOSE_PX - 8;
    }
    return v;
}

int ui_compose_clamp_origin(int v) {
    if (v < 0) {
        return 0;
    }
    if (v > R01_ENTITY_COMPOSE_PX) {
        return R01_ENTITY_COMPOSE_PX;
    }
    return v;
}

void ui_compose_clamp_hitbox(int *x, int *y, int *w, int *h) {
    int hx, hy, hw, hh;
    if (!x || !y || !w || !h) {
        return;
    }
    hx = *x;
    hy = *y;
    hw = *w;
    hh = *h;
    if (hw < 1) {
        hw = 1;
    }
    if (hh < 1) {
        hh = 1;
    }
    if (hw > R01_ENTITY_COMPOSE_PX) {
        hw = R01_ENTITY_COMPOSE_PX;
    }
    if (hh > R01_ENTITY_COMPOSE_PX) {
        hh = R01_ENTITY_COMPOSE_PX;
    }
    if (hx < 0) {
        hx = 0;
    }
    if (hy < 0) {
        hy = 0;
    }
    if (hx > R01_ENTITY_COMPOSE_PX - hw) {
        hx = R01_ENTITY_COMPOSE_PX - hw;
    }
    if (hy > R01_ENTITY_COMPOSE_PX - hh) {
        hy = R01_ENTITY_COMPOSE_PX - hh;
    }
    *x = hx;
    *y = hy;
    *w = hw;
    *h = hh;
}

void ui_compose_draw_grid(SDL_Renderer *r, int ox, int oy, int size_px, int cell_px) {
    int cells;
    if (cell_px < 1) {
        cell_px = 8;
    }
    cells = size_px / cell_px;
    if (cells < 1) {
        cells = 1;
    }
    draw_chess_grid(r, ox, oy, cells, cells, cell_px);
}

void ui_compose_draw_part(SDL_Renderer *r, const R01Project *p, const R01World *w, const R01EntityPart *pt, int ox,
                          int oy, int scale, int selected, int outline, Uint8 alpha) {
    uint8_t oriented[R01_TILE_BYTES];
    const uint8_t *raw;
    int row = w ? w->default_pal_row : 0;
    int sy, sx;
    int bx, by, bw, bh;
    if (!p || !w || !pt) {
        return;
    }
    raw = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!raw) {
        return;
    }
    r01_tile_orient(raw, pt->flip_h, pt->flip_v, oriented);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(oriented, sx, sy);
            uint8_t cr, cg, cb;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(p->global_pal_spr[row][pt->pal & 3].idx[col & 3u], &cr, &cg, &cb);
            if (alpha >= 255) {
                fill_rect(r, ox + (pt->dx + sx) * scale, oy + (pt->dy + sy) * scale, scale, scale, cr, cg, cb);
            } else {
                fill_rect_alpha(r, ox + (pt->dx + sx) * scale, oy + (pt->dy + sy) * scale, scale, scale, cr, cg, cb,
                                alpha);
            }
        }
    }
    bx = ox + pt->dx * scale;
    by = oy + pt->dy * scale;
    bw = 8 * scale;
    bh = 8 * scale;
    if (selected) {
        draw_marching_ants(r, bx, by, bw, bh);
    } else if (outline) {
        draw_rect(r, bx, by, bw, bh, 140, 140, 150);
    }
}

void ui_compose_draw_frame(SDL_Renderer *r, const R01Project *p, const R01World *w, const R01EntityFrame *fr, int ox,
                           int oy, int scale, int sel_part, int show_outlines, Uint8 alpha) {
    int i;
    if (!fr) {
        return;
    }
    for (i = 0; i < fr->part_count; i++) {
        if (i == sel_part) {
            continue;
        }
        ui_compose_draw_part(r, p, w, &fr->parts[i], ox, oy, scale, 0, show_outlines, alpha);
    }
    if (sel_part >= 0 && sel_part < fr->part_count) {
        ui_compose_draw_part(r, p, w, &fr->parts[sel_part], ox, oy, scale, 1, 0, alpha);
    }
}

void ui_compose_draw_frame_icon(SDL_Renderer *r, const R01Project *p, const R01World *w, const R01EntityFrame *fr,
                                int dx, int dy, int icon_size) {
    int i;
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    int cx, cy, off_x, off_y;
    UiClipStack stack;
    fill_rect(r, dx, dy, icon_size, icon_size, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!fr || fr->part_count < 1 || !p || !w) {
        return;
    }
    min_x = fr->parts[0].dx;
    min_y = fr->parts[0].dy;
    max_x = fr->parts[0].dx + 8;
    max_y = fr->parts[0].dy + 8;
    for (i = 1; i < fr->part_count; i++) {
        const R01EntityPart *pt = &fr->parts[i];
        if (pt->dx < min_x) {
            min_x = pt->dx;
        }
        if (pt->dy < min_y) {
            min_y = pt->dy;
        }
        if (pt->dx + 8 > max_x) {
            max_x = pt->dx + 8;
        }
        if (pt->dy + 8 > max_y) {
            max_y = pt->dy + 8;
        }
    }
    cx = (min_x + max_x) / 2;
    cy = (min_y + max_y) / 2;
    off_x = icon_size / 2 - cx;
    off_y = icon_size / 2 - cy;
    ui_clip_push(r, dx, dy, icon_size, icon_size, &stack);
    for (i = 0; i < fr->part_count; i++) {
        R01EntityPart ghost = fr->parts[i];
        ghost.dx = fr->parts[i].dx + off_x;
        ghost.dy = fr->parts[i].dy + off_y;
        ui_compose_draw_part(r, p, w, &ghost, dx, dy, 1, 0, 0, 255);
    }
    ui_clip_pop(r, &stack);
}

int ui_compose_part_at(const R01EntityFrame *fr, int px, int py, int prefer_sel) {
    int i;
    if (!fr) {
        return -1;
    }
    if (prefer_sel >= 0 && prefer_sel < fr->part_count) {
        const R01EntityPart *pt = &fr->parts[prefer_sel];
        if (px >= pt->dx && px < pt->dx + 8 && py >= pt->dy && py < pt->dy + 8) {
            return prefer_sel;
        }
    }
    for (i = fr->part_count - 1; i >= 0; i--) {
        const R01EntityPart *pt = &fr->parts[i];
        if (px >= pt->dx && px < pt->dx + 8 && py >= pt->dy && py < pt->dy + 8) {
            return i;
        }
    }
    return -1;
}

int ui_compose_sample_part(R01World *w, const R01EntityPart *pt, int cx, int cy, int *out_color) {
    const uint8_t *src;
    uint8_t oriented[R01_TILE_BYTES];
    int lx, ly;
    if (!w || !pt) {
        return 0;
    }
    if (cx < pt->dx || cx >= pt->dx + 8 || cy < pt->dy || cy >= pt->dy + 8) {
        return 0;
    }
    src = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!src) {
        return 0;
    }
    r01_tile_orient(src, pt->flip_h, pt->flip_v, oriented);
    lx = cx - pt->dx;
    ly = cy - pt->dy;
    if (out_color) {
        *out_color = (int)(r01_tile_pixel_color(oriented, lx, ly) & 3u);
    }
    return 1;
}

int ui_compose_paint_part(R01Project *p, R01World *w, R01EntityPart *pt, int cx, int cy, int paint_color) {
    const uint8_t *src;
    uint8_t tile[R01_TILE_BYTES];
    int lx, ly;
    uint8_t old_col;
    (void)p;
    if (!w || !pt) {
        return 0;
    }
    if (cx < pt->dx || cx >= pt->dx + 8 || cy < pt->dy || cy >= pt->dy + 8) {
        return 0;
    }
    src = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!src) {
        return 0;
    }
    memcpy(tile, src, R01_TILE_BYTES);
    lx = cx - pt->dx;
    ly = cy - pt->dy;
    if (pt->flip_h) {
        lx = 7 - lx;
    }
    if (pt->flip_v) {
        ly = 7 - ly;
    }
    old_col = r01_tile_pixel_color(tile, lx, ly) & 3u;
    if (old_col == (uint8_t)(paint_color & 3)) {
        return 0;
    }
    r01_tile_set_pixel(tile, lx, ly, (uint8_t)(paint_color & 3));
    (void)r01_chr_write_spr_tile(w, pt->bank, pt->tile_id, tile);
    return 1;
}

/* Brush stamps (world-pixel masks), sizes 1..4 from Studio brush reference. */
static const uint8_t k_brush1[] = {1};
static const uint8_t k_brush2[] = {1, 1, 1, 1};
static const uint8_t k_brush3[] = {
    0, 1, 1, 0,
    1, 1, 1, 1,
    1, 1, 1, 1,
    0, 1, 1, 0,
};
static const uint8_t k_brush4[] = {
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
};

void ui_compose_brush_stamp(int brush_size, int *out_w, int *out_h, const uint8_t **out_bits) {
    int w = 1;
    int h = 1;
    const uint8_t *bits = k_brush1;
    if (brush_size < UI_BRUSH_SIZE_MIN) {
        brush_size = UI_BRUSH_SIZE_MIN;
    }
    if (brush_size > UI_BRUSH_SIZE_MAX) {
        brush_size = UI_BRUSH_SIZE_MAX;
    }
    if (brush_size == 2) {
        w = 2;
        h = 2;
        bits = k_brush2;
    } else if (brush_size == 3) {
        w = 4;
        h = 4;
        bits = k_brush3;
    } else if (brush_size == 4) {
        w = 5;
        h = 5;
        bits = k_brush4;
    }
    if (out_w) {
        *out_w = w;
    }
    if (out_h) {
        *out_h = h;
    }
    if (out_bits) {
        *out_bits = bits;
    }
}

int ui_compose_paint_brush(R01Project *p, R01World *w, R01EntityPart *pt, int cx, int cy, int paint_color,
                           int brush_size) {
    int bw, bh, ox, oy, x, y;
    const uint8_t *bits;
    int wrote = 0;
    ui_compose_brush_stamp(brush_size, &bw, &bh, &bits);
    if (!bits) {
        return 0;
    }
    ox = (bw - 1) / 2;
    oy = (bh - 1) / 2;
    for (y = 0; y < bh; y++) {
        for (x = 0; x < bw; x++) {
            if (!bits[y * bw + x]) {
                continue;
            }
            if (ui_compose_paint_part(p, w, pt, cx - ox + x, cy - oy + y, paint_color)) {
                wrote = 1;
            }
        }
    }
    return wrote;
}
