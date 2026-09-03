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

R01Screen *ui_edit_map_screen(const UiState *ui) {
    if (!ui || !ui->project) {
        return NULL;
    }
    if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
        return r01_project_active_bg0_screen(ui->project);
    }
    return r01_project_active_screen(ui->project);
}

static void world_sel_set(UiState *ui, int col, int row) {
    if (!ui) {
        return;
    }
    ui->world_sel_col = col;
    ui->world_sel_row = row;
}

int ui_screen_nav(UiState *ui, int dcol, int drow) {
    R01World *w;
    R01Screen *cur;
    int ncol, nrow, idx;
    if (!ui || ui->play.active || ui->pal_edit.open || ui->tile_edit.open || ui->menu.open) {
        return 0;
    }
    w = r01_project_active_world(ui->project);
    cur = r01_project_active_screen(ui->project);
    if (!w || !cur) {
        return 0;
    }
    ncol = cur->col + dcol;
    nrow = cur->row + drow;
    idx = r01_world_find_screen(w, ncol, nrow);
    if (idx < 0) {
        return 0;
    }
    ui->project->active_screen = idx;
    world_sel_set(ui, ncol, nrow);
    screen_sel_clear(ui);
    return 1;
}

void handle_world_click(UiState *ui, int col, int row, int ctrl, int dbl) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    if (!w) {
        return;
    }
    world_sel_set(ui, col, row);
    if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
        if (ctrl) {
            (void)r01_world_bg0_remove_screen(w, col, row);
            return;
        }
        if (dbl) {
            if (r01_world_bg0_create_screen(w, col, row) < 0) {
                ui_toast(ui, "8 BG0 screens max", 1);
            }
            return;
        }
        idx = r01_world_bg0_screen_index(w, col, row);
        if (idx >= 0 && idx < w->bg0_screen_count && w->bg0_screens[idx].present) {
            w->bg0_active_screen = idx;
        }
        return;
    }
    if (ctrl) {
        idx = r01_world_screen_index(w, col, row);
        if (r01_world_remove_screen(w, col, row) == 0 && ui->project->active_screen == idx) {
            r01_project_select_start_screen(ui->project);
        }
        return;
    }
    if (dbl) {
        idx = r01_world_create_screen(w, col, row);
        if (idx >= 0) {
            ui->project->active_screen = idx;
        } else if (r01_world_present_count(w) >= R01_MAX_PRESENT_SCREENS) {
            ui_toast(ui, "32 present screens max", 1);
        }
        return;
    }
    idx = r01_world_find_screen(w, col, row);
    if (idx >= 0) {
        ui->project->active_screen = idx;
    }
}

static const R01Screen *world_sel_src(const UiState *ui, const R01World *w) {
    int col, row, idx;
    if (!ui || !w || ui->world_sel_col < 0 || ui->world_sel_row < 0) {
        return NULL;
    }
    col = ui->world_sel_col;
    row = ui->world_sel_row;
    if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
        idx = r01_world_bg0_screen_index(w, col, row);
        if (idx < 0 || idx >= w->bg0_screen_count || !w->bg0_screens[idx].present) {
            return NULL;
        }
        return &w->bg0_screens[idx];
    }
    idx = r01_world_find_screen(w, col, row);
    if (idx < 0 || !w->screens[idx].present) {
        return NULL;
    }
    return &w->screens[idx];
}

static int world_edit_blocked(const UiState *ui) {
    return !ui || ui->play.active || ui->menu.open || ui->tile_edit.open || ui->sprite_edit.open ||
           ui->metasprite_edit.open || ui->entity_edit.open || ui->pal_edit.open;
}

int ui_world_screen_copy(UiState *ui) {
    R01World *w;
    const R01Screen *src;
    if (world_edit_blocked(ui)) {
        return 0;
    }
    w = r01_project_active_world(ui->project);
    src = world_sel_src(ui, w);
    if (!src) {
        ui_toast(ui, "select a present screen to copy", 1);
        return 1;
    }
    ui->screen_clip = *src;
    ui->screen_clip.present = 1;
    ui->screen_clip_valid = 1;
    ui_toast(ui, "screen copied", 0);
    return 1;
}

int ui_world_screen_paste(UiState *ui) {
    R01World *w;
    R01Screen *dst;
    int col, row, idx;
    if (world_edit_blocked(ui)) {
        return 0;
    }
    if (!ui->screen_clip_valid) {
        ui_toast(ui, "no screen on clipboard", 1);
        return 1;
    }
    w = r01_project_active_world(ui->project);
    if (!w || ui->world_sel_col < 0 || ui->world_sel_row < 0) {
        ui_toast(ui, "select a grid slot to paste", 1);
        return 1;
    }
    col = ui->world_sel_col;
    row = ui->world_sel_row;
    if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
        if (r01_world_bg0_create_screen(w, col, row) < 0) {
            ui_toast(ui, "BG0 paste failed", 1);
            return 1;
        }
        idx = r01_world_bg0_screen_index(w, col, row);
        if (idx < 0) {
            ui_toast(ui, "BG0 paste failed", 1);
            return 1;
        }
        dst = &w->bg0_screens[idx];
        w->bg0_active_screen = idx;
    } else {
        idx = r01_world_create_screen(w, col, row);
        if (idx < 0) {
            if (r01_world_present_count(w) >= R01_MAX_PRESENT_SCREENS) {
                ui_toast(ui, "32 present screens max", 1);
            } else {
                ui_toast(ui, "paste failed", 1);
            }
            return 1;
        }
        dst = &w->screens[idx];
        ui->project->active_screen = idx;
    }
    memcpy(dst->tiles, ui->screen_clip.tiles, sizeof(dst->tiles));
    memcpy(dst->attrs, ui->screen_clip.attrs, sizeof(dst->attrs));
    memcpy(dst->pixels, ui->screen_clip.pixels, sizeof(dst->pixels));
    dst->col = col;
    dst->row = row;
    dst->present = 1;
    r01_screen_fill_pixels_from_bank(w, dst);
    ui_toast(ui, "screen pasted", 0);
    return 1;
}

int ui_world_screen_remove(UiState *ui) {
    R01World *w;
    int col, row, idx;
    if (world_edit_blocked(ui)) {
        return 0;
    }
    w = r01_project_active_world(ui->project);
    if (!w || ui->world_sel_col < 0 || ui->world_sel_row < 0) {
        ui_toast(ui, "select a present screen to remove", 1);
        return 1;
    }
    col = ui->world_sel_col;
    row = ui->world_sel_row;
    if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
        if (r01_world_bg0_remove_screen(w, col, row) != 0) {
            ui_toast(ui, "select a present screen to remove", 1);
            return 1;
        }
        ui_toast(ui, "screen removed", 0);
        return 1;
    }
    idx = r01_world_screen_index(w, col, row);
    if (r01_world_remove_screen(w, col, row) != 0) {
        ui_toast(ui, "select a present screen to remove", 1);
        return 1;
    }
    if (ui->project->active_screen == idx) {
        r01_project_select_start_screen(ui->project);
    }
    ui_toast(ui, "screen removed", 0);
    return 1;
}
