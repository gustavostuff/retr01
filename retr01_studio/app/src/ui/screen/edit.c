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
