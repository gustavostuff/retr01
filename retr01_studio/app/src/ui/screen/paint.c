#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
