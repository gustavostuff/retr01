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

static void menu_set_default_screen(UiState *ui);
static void menu_set_default_world(UiState *ui);

void menu_close(UiState *ui) {
    ui->menu.open = 0;
    ui->menu.kind = 0;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    ui->menu.item_count = 0;
    ui->menu.sub_count = 0;
    ui->menu.world_screen_idx = -1;
    memset(ui->menu.item_disabled, 0, sizeof(ui->menu.item_disabled));
}

static int menu_panel_w(char items[UI_MENU_MAX][32], int count, uint8_t *item_sub) {
    int i, w = UI_UNIT * 10;
    for (i = 0; i < count; i++) {
        int tw = label_width(items[i]);
        if (item_sub && item_sub[i]) {
            tw += UI_UNIT;
        }
        if (tw > w) {
            w = tw;
        }
    }
    return w;
}

static void menu_clamp_xy(int *x, int *y, int w, int h) {
    if (*x + w > UI_LOGIC_W) {
        *x = UI_LOGIC_W - w;
    }
    if (*y + h > UI_LOGIC_H) {
        *y = UI_LOGIC_H - h;
    }
    if (*x < 0) {
        *x = 0;
    }
    if (*y < 0) {
        *y = 0;
    }
}

static void menu_build_sub(UiState *ui, int sub_kind) {
    int i;
    ui->menu.sub_count = 0;
    if (sub_kind == UI_MENU_SUB_BANK || sub_kind == UI_MENU_SUB_PAL) {
        for (i = 0; i < 4; i++) {
            snprintf(ui->menu.sub_items[ui->menu.sub_count++], 24, "%d", i + 1);
        }
    }
    ui->menu.sub_w = UI_UNIT * 4;
    for (i = 0; i < ui->menu.sub_count; i++) {
        int tw = label_width(ui->menu.sub_items[i]);
        if (tw > ui->menu.sub_w) {
            ui->menu.sub_w = tw;
        }
    }
}

static void menu_place_submenu(UiState *ui, int root_item) {
    int sub_h;
    if (root_item < 0 || root_item >= ui->menu.item_count || !ui->menu.item_sub[root_item]) {
        ui->menu.submenu = UI_MENU_SUB_NONE;
        return;
    }
    menu_build_sub(ui, ui->menu.item_sub[root_item]);
    ui->menu.submenu = ui->menu.item_sub[root_item];
    sub_h = ui->menu.sub_count * UI_BTN_H;
    ui->menu.sub_x = ui->menu.root_x + ui->menu.root_w;
    ui->menu.sub_y = ui->menu.root_y + root_item * UI_BTN_H;
    if (ui->menu.sub_x + ui->menu.sub_w > UI_LOGIC_W) {
        ui->menu.sub_x = ui->menu.root_x - ui->menu.sub_w;
    }
    if (ui->menu.sub_y + sub_h > UI_LOGIC_H) {
        ui->menu.sub_y = UI_LOGIC_H - sub_h;
    }
    if (ui->menu.sub_y < 0) {
        ui->menu.sub_y = 0;
    }
    if (ui->menu.sub_x < 0) {
        ui->menu.sub_x = 0;
    }
}

void menu_open_tile(UiState *ui, int x, int y, int tx, int ty) {
    ui->menu.open = 1;
    ui->menu.kind = UI_MENU_KIND_TILE;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    ui->menu.screen_tx = tx;
    ui->menu.screen_ty = ty;
    ui->menu.world_screen_idx = -1;
    ui->menu.item_count = 0;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Move to tile bank");
    ui->menu.item_sub[ui->menu.item_count++] = UI_MENU_SUB_BANK;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Add new tile here");
    ui->menu.item_sub[ui->menu.item_count++] = 0;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Edit tile");
    ui->menu.item_sub[ui->menu.item_count++] = 0;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Set tile palette");
    ui->menu.item_sub[ui->menu.item_count++] = UI_MENU_SUB_PAL;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Set Anim mode");
    ui->menu.item_sub[ui->menu.item_count++] = 0;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Set Solid");
    ui->menu.item_sub[ui->menu.item_count++] = 0;
    memset(ui->menu.item_disabled, 0, sizeof(ui->menu.item_disabled));
    if (screen_sel_is_multi(ui)) {
        ui->menu.item_disabled[1] = 1;
        ui->menu.item_disabled[2] = 1;
    }
    ui->menu.root_w = menu_panel_w(ui->menu.items, ui->menu.item_count, ui->menu.item_sub);
    ui->menu.root_x = x;
    ui->menu.root_y = y;
    menu_clamp_xy(&ui->menu.root_x, &ui->menu.root_y, ui->menu.root_w, ui->menu.item_count * UI_BTN_H);
}

void menu_open_world_cell(UiState *ui, int x, int y, int screen_idx) {
    ui->menu.open = 1;
    ui->menu.kind = UI_MENU_KIND_WORLD;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    ui->menu.screen_tx = -1;
    ui->menu.screen_ty = -1;
    ui->menu.world_screen_idx = screen_idx;
    ui->menu.item_count = 0;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Set default screen");
    ui->menu.item_sub[ui->menu.item_count++] = 0;
    snprintf(ui->menu.items[ui->menu.item_count], 32, "Make default world");
    ui->menu.item_sub[ui->menu.item_count++] = 0;
    ui->menu.root_w = menu_panel_w(ui->menu.items, ui->menu.item_count, ui->menu.item_sub);
    ui->menu.root_x = x;
    ui->menu.root_y = y;
    menu_clamp_xy(&ui->menu.root_x, &ui->menu.root_y, ui->menu.root_w, ui->menu.item_count * UI_BTN_H);
}

static int menu_root_hit(const UiState *ui, int lx, int ly, int *out_item) {
    int h;
    if (!ui->menu.open) {
        return 0;
    }
    h = ui->menu.item_count * UI_BTN_H;
    if (lx < ui->menu.root_x || ly < ui->menu.root_y || lx >= ui->menu.root_x + ui->menu.root_w ||
        ly >= ui->menu.root_y + h) {
        return 0;
    }
    if (out_item) {
        *out_item = (ly - ui->menu.root_y) / UI_BTN_H;
    }
    return 1;
}

static int menu_sub_hit(const UiState *ui, int lx, int ly, int *out_item) {
    int h;
    if (!ui->menu.open || ui->menu.submenu == UI_MENU_SUB_NONE) {
        return 0;
    }
    h = ui->menu.sub_count * UI_BTN_H;
    if (lx < ui->menu.sub_x || ly < ui->menu.sub_y || lx >= ui->menu.sub_x + ui->menu.sub_w ||
        ly >= ui->menu.sub_y + h) {
        return 0;
    }
    if (out_item) {
        *out_item = (ly - ui->menu.sub_y) / UI_BTN_H;
    }
    return 1;
}

int menu_hit(const UiState *ui, int lx, int ly, int *out_item, int *out_sub) {
    if (menu_sub_hit(ui, lx, ly, out_item)) {
        if (out_sub) {
            *out_sub = 1;
        }
        return 1;
    }
    if (menu_root_hit(ui, lx, ly, out_item)) {
        if (out_sub) {
            *out_sub = 0;
        }
        return 1;
    }
    return 0;
}

void menu_update_hover(UiState *ui, int lx, int ly) {
    int item;
    int root_h;
    if (!ui->menu.open) {
        return;
    }
    root_h = ui->menu.item_count * UI_BTN_H;
    if (menu_sub_hit(ui, lx, ly, NULL)) {
        return;
    }
    if (menu_root_hit(ui, lx, ly, &item)) {
        if (ui->menu.item_disabled[item]) {
            ui->menu.submenu = UI_MENU_SUB_NONE;
            return;
        }
        if (ui->menu.item_sub[item]) {
            menu_place_submenu(ui, item);
        } else {
            ui->menu.submenu = UI_MENU_SUB_NONE;
        }
        return;
    }
    if (ui->menu.submenu != UI_MENU_SUB_NONE) {
        int sub_h = ui->menu.sub_count * UI_BTN_H;
        int min_x = ui->menu.root_x < ui->menu.sub_x ? ui->menu.root_x : ui->menu.sub_x;
        int max_x = (ui->menu.root_x + ui->menu.root_w) > (ui->menu.sub_x + ui->menu.sub_w)
                        ? ui->menu.root_x + ui->menu.root_w
                        : ui->menu.sub_x + ui->menu.sub_w;
        int min_y = ui->menu.root_y < ui->menu.sub_y ? ui->menu.root_y : ui->menu.sub_y;
        int max_y = (ui->menu.root_y + root_h) > (ui->menu.sub_y + sub_h) ? ui->menu.root_y + root_h
                                                                           : ui->menu.sub_y + sub_h;
        if (lx >= min_x && lx < max_x && ly >= min_y && ly < max_y) {
            return;
        }
    }
    ui->menu.submenu = UI_MENU_SUB_NONE;
}

static void menu_ensure_tile_sel(UiState *ui) {
    if (ui->menu.kind != UI_MENU_KIND_TILE) {
        return;
    }
    if (!screen_sel_valid(ui) && ui->menu.screen_tx >= 0 && ui->menu.screen_ty >= 0) {
        screen_sel_set(ui, ui->menu.screen_tx, ui->menu.screen_ty, ui->menu.screen_tx, ui->menu.screen_ty);
    }
}

void handle_menu_pick(UiState *ui, int item, int is_sub) {
    if (is_sub) {
        if (item < 0 || item >= ui->menu.sub_count) {
            menu_close(ui);
            return;
        }
        menu_ensure_tile_sel(ui);
        if (ui->menu.submenu == UI_MENU_SUB_BANK) {
            screen_set_sel_bank(ui, item);
        } else if (ui->menu.submenu == UI_MENU_SUB_PAL) {
            screen_set_sel_pal(ui, item);
        }
        menu_close(ui);
        return;
    }
    if (item < 0 || item >= ui->menu.item_count) {
        menu_close(ui);
        return;
    }
    if (ui->menu.item_disabled[item]) {
        return;
    }
    if (ui->menu.item_sub[item]) {
        menu_ensure_tile_sel(ui);
        menu_place_submenu(ui, item);
        return;
    }
    if (ui->menu.kind == UI_MENU_KIND_WORLD) {
        if (item == 0) {
            menu_set_default_screen(ui);
        } else if (item == 1) {
            menu_set_default_world(ui);
        }
        menu_close(ui);
        return;
    }
    switch (item) {
    case 1:
        tile_edit_open_new(ui, ui->menu.screen_tx, ui->menu.screen_ty);
        break;
    case 2:
        tile_edit_open(ui, ui->menu.screen_tx, ui->menu.screen_ty);
        break;
    case 4:
        menu_ensure_tile_sel(ui);
        screen_toggle_sel_flag(ui, R01_ATTR_ANIM);
        break;
    case 5:
        menu_ensure_tile_sel(ui);
        screen_toggle_sel_flag(ui, R01_ATTR_SOLID);
        break;
    default:
        break;
    }
    menu_close(ui);
}

static void menu_set_default_screen(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    if (!w || ui->menu.world_screen_idx < 0 || ui->menu.world_screen_idx >= w->screen_count ||
        !w->screens[ui->menu.world_screen_idx].present) {
        return;
    }
    w->default_screen = ui->menu.world_screen_idx;
    ui_toast(ui, "default screen set", 0);
}

static void menu_set_default_world(UiState *ui) {
    if (!ui || !ui->project) {
        return;
    }
    ui->project->default_world = ui->project->active_world;
    ui_toast(ui, "default world set", 0);
}

