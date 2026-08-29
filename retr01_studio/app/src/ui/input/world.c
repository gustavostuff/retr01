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
    screen_sel_clear(ui);
    return 1;
}

void handle_world_click(UiState *ui, int col, int row, int ctrl, int dbl) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    if (!w) {
        return;
    }
    if (ctrl) {
        r01_world_remove_screen(w, col, row);
        if (ui->project->active_screen == r01_world_screen_index(w, col, row)) {
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
