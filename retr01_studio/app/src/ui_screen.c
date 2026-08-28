#include "ui.h"
#include "ui_internal.h"
#include "font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void screen_sel_set(UiState *ui, int x0, int y0, int x1, int y1) {
    if (!ui) {
        return;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= R01_SCREEN_TILES_X) {
        x1 = R01_SCREEN_TILES_X - 1;
    }
    if (y1 >= R01_SCREEN_TILES_Y) {
        y1 = R01_SCREEN_TILES_Y - 1;
    }
    ui->sel_x0 = x0;
    ui->sel_y0 = y0;
    ui->sel_x1 = x1;
    ui->sel_y1 = y1;
}

void screen_sel_clear(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->sel_x0 = -1;
    ui->sel_y0 = -1;
    ui->sel_x1 = -1;
    ui->sel_y1 = -1;
    ui->sel_drag = 0;
}

int screen_sel_valid(const UiState *ui) {
    return ui && ui->sel_x0 >= 0 && ui->sel_y0 >= 0;
}

void screen_sel_bounds(const UiState *ui, int *min_x, int *min_y, int *max_x, int *max_y) {
    if (!screen_sel_valid(ui)) {
        *min_x = *min_y = *max_x = *max_y = -1;
        return;
    }
    *min_x = ui->sel_x0 < ui->sel_x1 ? ui->sel_x0 : ui->sel_x1;
    *max_x = ui->sel_x0 > ui->sel_x1 ? ui->sel_x0 : ui->sel_x1;
    *min_y = ui->sel_y0 < ui->sel_y1 ? ui->sel_y0 : ui->sel_y1;
    *max_y = ui->sel_y0 > ui->sel_y1 ? ui->sel_y0 : ui->sel_y1;
}

int screen_sel_is_multi(const UiState *ui) {
    int min_x, min_y, max_x, max_y;
    if (!screen_sel_valid(ui)) {
        return 0;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    return (max_x - min_x + 1) * (max_y - min_y + 1) > 1;
}

void ui_paint_stamp_set(UiState *ui, uint8_t tile, uint8_t attr) {
    if (!ui) {
        return;
    }
    ui->paint_stamp_tile = tile;
    ui->paint_stamp_attr = attr;
    ui->paint_stamp_valid = 1;
}

void ui_paint_stamp_from_cell(UiState *ui, int tx, int ty) {
    R01Screen *s;
    int cell;
    if (!ui || tx < 0 || ty < 0) {
        return;
    }
    s = r01_project_active_screen(ui->project);
    if (!s) {
        return;
    }
    cell = ty * R01_SCREEN_TILES_X + tx;
    ui_paint_stamp_set(ui, s->tiles[cell], s->attrs[cell]);
}

int ui_paint_stamp_from_sel(const UiState *ui, uint8_t *out_tile, uint8_t *out_attr) {
    const R01Screen *s;
    int min_x, min_y, max_x, max_y;
    int cell;
    if (!screen_sel_valid(ui)) {
        return 0;
    }
    s = r01_project_active_screen(ui->project);
    if (!s) {
        return 0;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    cell = min_y * R01_SCREEN_TILES_X + min_x;
    if (out_tile) {
        *out_tile = s->tiles[cell];
    }
    if (out_attr) {
        *out_attr = s->attrs[cell];
    }
    return 1;
}

static int ui_paint_stamp_get(const UiState *ui, uint8_t *out_tile, uint8_t *out_attr) {
    if (ui && ui->paint_stamp_valid) {
        if (out_tile) {
            *out_tile = ui->paint_stamp_tile;
        }
        if (out_attr) {
            *out_attr = ui->paint_stamp_attr;
        }
        return 1;
    }
    return ui_paint_stamp_from_sel(ui, out_tile, out_attr);
}

void ui_paint_tile(UiState *ui, int tx, int ty) {
    R01World *w;
    R01Screen *s;
    uint8_t tile_id, attr;
    if (!ui || ui->play.active || tx < 0 || ty < 0) {
        return;
    }
    if (!ui_paint_stamp_get(ui, &tile_id, &attr)) {
        ui_toast(ui, "select or Alt+click a tile to paint with", 1);
        return;
    }
    if (tx == ui->last_paint_tx && ty == ui->last_paint_ty) {
        return;
    }
    w = r01_project_active_world(ui->project);
    s = r01_project_active_screen(ui->project);
    if (!w || !s) {
        return;
    }
    r01_screen_paint_tile(w, s, tx, ty, tile_id, attr);
    ui->last_paint_tx = tx;
    ui->last_paint_ty = ty;
}

void ui_flood_fill(UiState *ui, int tx, int ty) {
    R01World *w;
    R01Screen *s;
    uint8_t seed_tile, seed_attr;
    uint8_t stamp_tile, stamp_attr;
    uint8_t visited[R01_TILES_PER_SCREEN];
    int queue[R01_TILES_PER_SCREEN];
    int qhead = 0;
    int qtail = 0;
    if (!ui || ui->play.active || tx < 0 || ty < 0) {
        return;
    }
    if (!ui_paint_stamp_get(ui, &stamp_tile, &stamp_attr)) {
        ui_toast(ui, "select or Alt+click a tile to fill with", 1);
        return;
    }
    w = r01_project_active_world(ui->project);
    s = r01_project_active_screen(ui->project);
    if (!w || !s) {
        return;
    }
    seed_tile = s->tiles[ty * R01_SCREEN_TILES_X + tx];
    seed_attr = s->attrs[ty * R01_SCREEN_TILES_X + tx];
    if (seed_tile == stamp_tile && seed_attr == stamp_attr) {
        return;
    }
    memset(visited, 0, sizeof(visited));
    queue[qtail++] = ty * R01_SCREEN_TILES_X + tx;
    visited[ty * R01_SCREEN_TILES_X + tx] = 1;
    while (qhead < qtail) {
        static const int dx[4] = {1, -1, 0, 0};
        static const int dy[4] = {0, 0, 1, -1};
        int cell = queue[qhead++];
        int cx = cell % R01_SCREEN_TILES_X;
        int cy = cell / R01_SCREEN_TILES_X;
        int d;
        r01_screen_paint_tile(w, s, cx, cy, stamp_tile, stamp_attr);
        for (d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            int ncell;
            if (nx < 0 || ny < 0 || nx >= R01_SCREEN_TILES_X || ny >= R01_SCREEN_TILES_Y) {
                continue;
            }
            ncell = ny * R01_SCREEN_TILES_X + nx;
            if (visited[ncell]) {
                continue;
            }
            if (s->tiles[ncell] != seed_tile || s->attrs[ncell] != seed_attr) {
                continue;
            }
            visited[ncell] = 1;
            queue[qtail++] = ncell;
        }
    }
}

void ui_toggle_play(UiState *ui) {
    if (ui->play.active) {
        r01_play_stop(&ui->play);
    } else {
        r01_project_begin_play(ui->project);
        if (!r01_play_start(&ui->play, ui->project)) {
            ui_toast(ui, "no screens - create one first", 1);
        } else {
            ui->play_last_tick = SDL_GetTicks();
        }
    }
}

static void screen_refresh_tile(R01World *w, R01Screen *s) {
    if (w && s && s->present) {
        r01_screen_fill_pixels_from_bank(w, s);
    }
}

void screen_refresh_sel(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    screen_refresh_tile(w, s);
}

static void screen_set_sel_attr(UiState *ui, int bank, int pal, int flip_h, int flip_v) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!w || !s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t old = s->attrs[cell];
            s->attrs[cell] = r01_attr_merge(old, bank, pal, flip_h, flip_v);
        }
    }
    screen_refresh_sel(ui);
}

void screen_set_sel_bank(UiState *ui, int bank) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!w || !s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t old = s->attrs[cell];
            s->attrs[cell] =
                r01_attr_merge(old, bank, r01_attr_pal(old), r01_attr_flip_h(old), r01_attr_flip_v(old));
        }
    }
    screen_refresh_sel(ui);
}

void screen_set_sel_pal(UiState *ui, int pal) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!w || !s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            uint8_t old = s->attrs[cell];
            s->attrs[cell] =
                r01_attr_merge(old, r01_attr_bank(old), pal, r01_attr_flip_h(old), r01_attr_flip_v(old));
        }
    }
    screen_refresh_sel(ui);
}

void screen_toggle_sel_flag(UiState *ui, uint8_t flag) {
    R01Screen *s = r01_project_active_screen(ui->project);
    int min_x, min_y, max_x, max_y, ty, tx;
    if (!s || !screen_sel_valid(ui)) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    for (ty = min_y; ty <= max_y; ty++) {
        for (tx = min_x; tx <= max_x; tx++) {
            int cell = ty * R01_SCREEN_TILES_X + tx;
            s->attrs[cell] ^= flag;
        }
    }
}

void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int ox, oy, y, x;
    R01World *w = r01_project_active_world(ui->project);
    screen_origin(ui, &ox, &oy);
    fill_rect(r, ox, oy, UI_SCREEN_W, UI_SCREEN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!s || !w) {
        font_draw_centered(r, ox, oy, UI_SCREEN_W, UI_SCREEN_H, "No screen", 160, 160, 170);
        return;
    }
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t cr, cg, cb;
            SDL_Rect px;
            r01_screen_pixel_rgb(ui->project, w, s, x, y, &cr, &cg, &cb);
            px.x = ox + x * UI_SCREEN_SCALE;
            px.y = oy + y * UI_SCREEN_SCALE;
            px.w = UI_SCREEN_SCALE;
            px.h = UI_SCREEN_SCALE;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    if (screen_sel_valid(ui) && ui->screen_mode == UI_SCREEN_MODE_SEL) {
        int min_x, min_y, max_x, max_y;
        int sx, sy, sw, sh;
        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
        sx = ox + min_x * 8 * UI_SCREEN_SCALE;
        sy = oy + min_y * 8 * UI_SCREEN_SCALE;
        sw = (max_x - min_x + 1) * 8 * UI_SCREEN_SCALE;
        sh = (max_y - min_y + 1) * 8 * UI_SCREEN_SCALE;
        draw_rect(r, sx, sy, sw, sh, 255, 255, 255);
    }
}

void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int ox, oy, vy, vx;
    screen_origin(ui, &ox, &oy);
    for (vy = 0; vy < R01_SCREEN_PX_H; vy++) {
        for (vx = 0; vx < R01_SCREEN_PX_W; vx++) {
            uint8_t cr = 0, cg = 0, cb = 0;
            SDL_Rect px;
            r01_play_sample_bg(ui->project, &ui->play, vx, vy, &cr, &cg, &cb);
            px.x = ox + vx * UI_SCREEN_SCALE;
            px.y = oy + vy * UI_SCREEN_SCALE;
            px.w = UI_SCREEN_SCALE;
            px.h = UI_SCREEN_SCALE;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    {
        int pcx, pcy;
        uint8_t pr, pg, pb;
        r01_project_player_rgb(ui->project, &pr, &pg, &pb);
        for (pcy = 0; pcy < R01_PLAY_PLAYER_H; pcy++) {
            for (pcx = 0; pcx < R01_PLAY_PLAYER_W; pcx++) {
                int wx = ui->play.player_x + pcx;
                int wy = ui->play.player_y + pcy;
                int vx2 = wx - ui->play.cam_x;
                int vy2 = wy - ui->play.cam_y;
                SDL_Rect px;
                if (vx2 < 0 || vy2 < 0 || vx2 >= R01_SCREEN_PX_W || vy2 >= R01_SCREEN_PX_H) {
                    continue;
                }
                px.x = ox + vx2 * UI_SCREEN_SCALE;
                px.y = oy + vy2 * UI_SCREEN_SCALE;
                px.w = UI_SCREEN_SCALE;
                px.h = UI_SCREEN_SCALE;
                SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

