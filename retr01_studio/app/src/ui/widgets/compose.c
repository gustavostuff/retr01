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

void ui_compose_draw_grid(SDL_Renderer *r, int ox, int oy, int size_px) {
    int i;
    int cells = size_px / 8;
    fill_rect(r, ox, oy, size_px, size_px, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    for (i = 1; i < cells; i++) {
        int g = i * 8;
        SDL_SetRenderDrawColor(r, 50, 50, 58, 255);
        {
            SDL_Rect hr = {ox, oy + g, size_px, 1};
            SDL_Rect vr = {ox + g, oy, 1, size_px};
            SDL_RenderFillRect(r, &hr);
            SDL_RenderFillRect(r, &vr);
        }
    }
}

void ui_compose_draw_part(SDL_Renderer *r, const R01Project *p, const R01World *w, const R01EntityPart *pt, int ox,
                          int oy, int scale, int selected) {
    uint8_t oriented[R01_TILE_BYTES];
    const uint8_t *raw;
    int row = w ? w->default_pal_row : 0;
    int sy, sx;
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
            fill_rect(r, ox + (pt->dx + sx) * scale, oy + (pt->dy + sy) * scale, scale, scale, cr, cg, cb);
        }
    }
    if (selected) {
        draw_rect(r, ox + pt->dx * scale, oy + pt->dy * scale, 8 * scale, 8 * scale, 240, 240, 240);
    }
}

void ui_compose_draw_frame(SDL_Renderer *r, const R01Project *p, const R01World *w, const R01EntityFrame *fr, int ox,
                           int oy, int scale, int sel_part) {
    int i;
    if (!fr) {
        return;
    }
    for (i = 0; i < fr->part_count; i++) {
        if (i == sel_part) {
            continue;
        }
        ui_compose_draw_part(r, p, w, &fr->parts[i], ox, oy, scale, 0);
    }
    if (sel_part >= 0 && sel_part < fr->part_count) {
        ui_compose_draw_part(r, p, w, &fr->parts[sel_part], ox, oy, scale, 1);
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
        ui_compose_draw_part(r, p, w, &ghost, dx, dy, 1, 0);
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

void ui_compose_paint_part(R01Project *p, R01World *w, R01EntityPart *pt, int cx, int cy, int paint_color) {
    const uint8_t *src;
    uint8_t tile[R01_TILE_BYTES];
    int lx, ly;
    (void)p;
    if (!w || !pt) {
        return;
    }
    if (cx < pt->dx || cx >= pt->dx + 8 || cy < pt->dy || cy >= pt->dy + 8) {
        return;
    }
    src = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!src) {
        return;
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
    r01_tile_set_pixel(tile, lx, ly, (uint8_t)(paint_color & 3));
    (void)r01_chr_write_spr_tile(w, pt->bank, pt->tile_id, tile);
}
