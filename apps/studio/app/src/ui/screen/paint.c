#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/undo/undo_cmds.h"
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
    ui->paint_stamp_w = 1;
    ui->paint_stamp_h = 1;
    ui->paint_stamp_tiles[0] = tile;
    ui->paint_stamp_attrs[0] = attr;
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
    s = ui_edit_map_screen(ui);
    if (!s) {
        return;
    }
    cell = ty * R01_SCREEN_TILES_X + tx;
    ui_paint_stamp_set(ui, s->tiles[cell], s->attrs[cell]);
}

void ui_paint_stamp_from_selection(UiState *ui) {
    R01Screen *s;
    int min_x, min_y, max_x, max_y;
    int w, h, y, x, i;
    if (!ui || !screen_sel_valid(ui)) {
        return;
    }
    s = ui_edit_map_screen(ui);
    if (!s) {
        return;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    w = max_x - min_x + 1;
    h = max_y - min_y + 1;
    if (w < 1 || h < 1 || w * h > R01_TILES_PER_SCREEN) {
        return;
    }
    i = 0;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int cell = (min_y + y) * R01_SCREEN_TILES_X + (min_x + x);
            ui->paint_stamp_tiles[i] = s->tiles[cell];
            ui->paint_stamp_attrs[i] = s->attrs[cell];
            i++;
        }
    }
    ui->paint_stamp_w = w;
    ui->paint_stamp_h = h;
    ui->paint_stamp_tile = ui->paint_stamp_tiles[0];
    ui->paint_stamp_attr = ui->paint_stamp_attrs[0];
    ui->paint_stamp_valid = 1;
}

int ui_paint_stamp_from_sel(const UiState *ui, uint8_t *out_tile, uint8_t *out_attr) {
    const R01Screen *s;
    int min_x, min_y, max_x, max_y;
    int cell;
    if (!screen_sel_valid(ui)) {
        return 0;
    }
    s = ui_edit_map_screen(ui);
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

static int ui_paint_stamp_ready(const UiState *ui) {
    if (ui && ui->paint_stamp_valid && ui->paint_stamp_w > 0 && ui->paint_stamp_h > 0) {
        return 1;
    }
    return 0;
}

void ui_paint_tile(UiState *ui, int tx, int ty) {
    R01World *w;
    R01Screen *s;
    int sw, sh, y, x;
    if (!ui || ui->play.active || tx < 0 || ty < 0) {
        return;
    }
    if (!ui_paint_stamp_ready(ui)) {
        if (screen_sel_valid(ui)) {
            ui_paint_stamp_from_selection(ui);
        }
        if (!ui_paint_stamp_ready(ui)) {
            ui_toast(ui, "Not tile was selected as brush", 1);
            return;
        }
    }
    if (tx == ui->last_paint_tx && ty == ui->last_paint_ty) {
        return;
    }
    w = r01_project_active_world(ui->project);
    s = ui_edit_map_screen(ui);
    if (!w || !s) {
        return;
    }
    sw = ui->paint_stamp_w;
    sh = ui->paint_stamp_h;
    for (y = 0; y < sh; y++) {
        for (x = 0; x < sw; x++) {
            int dx = tx + x;
            int dy = ty + y;
            int cell;
            int si;
            uint8_t tile_id, attr;
            uint8_t old_tile, old_attr;
            if (dx < 0 || dy < 0 || dx >= R01_SCREEN_TILES_X || dy >= R01_SCREEN_TILES_Y) {
                continue;
            }
            si = y * sw + x;
            tile_id = ui->paint_stamp_tiles[si];
            attr = ui->paint_stamp_attrs[si];
            cell = dy * R01_SCREEN_TILES_X + dx;
            old_tile = s->tiles[cell];
            old_attr = s->attrs[cell];
            if (old_tile == tile_id && old_attr == attr) {
                continue;
            }
            ui_undo_paint_record_cell(ui, dx, dy, old_tile, old_attr, tile_id, attr);
            r01_screen_paint_tile(w, s, dx, dy, tile_id, attr);
        }
    }
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
    if (!ui_paint_stamp_ready(ui)) {
        if (screen_sel_valid(ui)) {
            ui_paint_stamp_from_selection(ui);
        }
    }
    if (!ui_paint_stamp_ready(ui)) {
        ui_toast(ui, "select or Alt+click a tile to fill with", 1);
        return;
    }
    stamp_tile = ui->paint_stamp_tiles[0];
    stamp_attr = ui->paint_stamp_attrs[0];
    w = r01_project_active_world(ui->project);
    s = ui_edit_map_screen(ui);
    if (!w || !s) {
        return;
    }
    {
        int cell = ty * R01_SCREEN_TILES_X + tx;
        seed_tile = s->tiles[cell];
        seed_attr = s->attrs[cell];
    }
    if (seed_tile == stamp_tile && seed_attr == stamp_attr) {
        return;
    }
    memset(visited, 0, sizeof(visited));
    (void)ui_undo_paint_begin(ui);
    queue[qtail++] = ty * R01_SCREEN_TILES_X + tx;
    visited[ty * R01_SCREEN_TILES_X + tx] = 1;
    while (qhead < qtail) {
        int cell = queue[qhead++];
        int cx = cell % R01_SCREEN_TILES_X;
        int cy = cell / R01_SCREEN_TILES_X;
        int n;
        const int nx[4] = {cx - 1, cx + 1, cx, cx};
        const int ny[4] = {cy, cy, cy - 1, cy + 1};
        ui_undo_paint_record_cell(ui, cx, cy, s->tiles[cell], s->attrs[cell], stamp_tile, stamp_attr);
        r01_screen_paint_tile(w, s, cx, cy, stamp_tile, stamp_attr);
        for (n = 0; n < 4; n++) {
            int ncell;
            if (nx[n] < 0 || ny[n] < 0 || nx[n] >= R01_SCREEN_TILES_X || ny[n] >= R01_SCREEN_TILES_Y) {
                continue;
            }
            ncell = ny[n] * R01_SCREEN_TILES_X + nx[n];
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
    ui_undo_paint_end(ui);
}
