#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ui_mode_panel_w(void) {
    int label_x = UI_TOGGLE_W;
    int w = label_width("Work on:");
    int cand;
    cand = label_x + label_width("BG layer");
    if (cand > w) {
        w = cand;
    }
    cand = label_x + label_width("Sprite layer");
    if (cand > w) {
        w = cand;
    }
    cand = label_x + label_width("Both");
    if (cand > w) {
        w = cand;
    }
    cand = label_width("Hide:");
    if (cand > w) {
        w = cand;
    }
    return w;
}

int ui_layer_panel_w(void) {
    return ui_mode_panel_w();
}

void ui_preview_size(const UiState *ui, int *out_w, int *out_h) {
    if (out_w) {
        *out_w = ui_screen_w(ui);
    }
    if (out_h) {
        *out_h = ui_screen_h(ui);
    }
}

void ui_editor_layout(const UiState *ui, int *screen_x, int *screen_y, int *layer_x, int *mode_x, int *mode_y0) {
    int sx = ui_preview_x(ui);
    int sy;
    int chrome = UI_APP_CHROME_H;
    int content_h;
    int ctrl_inner = ui_ctrl_x(ui) + UI_UNIT;
    int radios_y = chrome + UI_BTN_H + UI_UNIT;

    content_h = ui_logic_h(ui) - chrome;
    sy = chrome + (content_h - ui_screen_h(ui)) / 2;
    if (sy < chrome + UI_UNIT) {
        sy = chrome + UI_UNIT;
    }
    if (screen_x) {
        *screen_x = sx;
    }
    if (screen_y) {
        *screen_y = sy;
    }
    if (layer_x) {
        *layer_x = ctrl_inner;
    }
    if (mode_x) {
        *mode_x = ctrl_inner;
    }
    if (mode_y0) {
        /* First row is the "Work on:" label. */
        *mode_y0 = radios_y;
    }
}

int ui_mode_label_x(int mode_x) {
    return ui_toggle_label_x(mode_x);
}

int ui_work_allows_bg(const UiState *ui) {
    return ui && !ui->hide_bg_layer &&
           (ui->screen_layer == UI_SCREEN_LAYER_BG || ui->screen_layer == UI_SCREEN_LAYER_BOTH);
}

int ui_work_allows_spr(const UiState *ui) {
    return ui && !ui->hide_spr_layer &&
           (ui->screen_layer == UI_SCREEN_LAYER_SPR || ui->screen_layer == UI_SCREEN_LAYER_BOTH);
}

static void layer_chrome_ys(const UiState *ui, int *layer_x, int *work_label_y, int *work_radio_y0,
                            int *hide_label_y, int *hide_check_y0) {
    int sx, sy, mx, my0;
    ui_editor_layout(ui, &sx, &sy, layer_x, &mx, &my0);
    if (work_label_y) {
        *work_label_y = my0;
    }
    if (work_radio_y0) {
        *work_radio_y0 = my0 + UI_MODE_ROW_H;
    }
    if (hide_label_y) {
        *hide_label_y = my0 + UI_MODE_ROW_H + 3 * UI_MODE_ROW_H + UI_UNIT;
    }
    if (hide_check_y0) {
        *hide_check_y0 = my0 + UI_MODE_ROW_H + 3 * UI_MODE_ROW_H + UI_UNIT + UI_MODE_ROW_H;
    }
}

int screen_mode_row_hit(const UiState *ui, int lx, int ly, int row) {
    (void)ui;
    (void)lx;
    (void)ly;
    (void)row;
    return 0;
}

int screen_mode_hit(const UiState *ui, int lx, int ly, int *out_row) {
    (void)ui;
    (void)lx;
    (void)ly;
    if (out_row) {
        *out_row = 0;
    }
    return 0;
}

int screen_layer_row_hit(const UiState *ui, int lx, int ly, int row) {
    int layer_x, work_radio_y0;
    int y;
    if (!ui || ui->play.active || row < 0 || row > 2) {
        return 0;
    }
    layer_chrome_ys(ui, &layer_x, NULL, &work_radio_y0, NULL, NULL);
    y = work_radio_y0 + row * UI_MODE_ROW_H;
    return point_in_rect(lx, ly, layer_x, y, ui_layer_panel_w(), UI_MODE_ROW_H);
}

int screen_layer_hit(const UiState *ui, int lx, int ly, int *out_layer) {
    static const int k_layers[3] = {UI_SCREEN_LAYER_BG, UI_SCREEN_LAYER_SPR, UI_SCREEN_LAYER_BOTH};
    int row;
    for (row = 0; row < 3; row++) {
        if (screen_layer_row_hit(ui, lx, ly, row)) {
            if (out_layer) {
                *out_layer = k_layers[row];
            }
            return 1;
        }
    }
    return 0;
}

int screen_hide_row_hit(const UiState *ui, int lx, int ly, int row) {
    int layer_x, hide_check_y0;
    int y;
    if (!ui || ui->play.active || row < 0 || row > 1) {
        return 0;
    }
    layer_chrome_ys(ui, &layer_x, NULL, NULL, NULL, &hide_check_y0);
    y = hide_check_y0 + row * UI_MODE_ROW_H;
    return point_in_rect(lx, ly, layer_x, y, ui_layer_panel_w(), UI_MODE_ROW_H);
}

int screen_hide_hit(const UiState *ui, int lx, int ly, int *out_hide_bg) {
    if (screen_hide_row_hit(ui, lx, ly, 0)) {
        if (out_hide_bg) {
            *out_hide_bg = 1;
        }
        return 1;
    }
    if (screen_hide_row_hit(ui, lx, ly, 1)) {
        if (out_hide_bg) {
            *out_hide_bg = 0;
        }
        return 1;
    }
    return 0;
}

int play_btn_w(const UiState *ui) {
    return label_width(ui->play.active ? "Stop" : "Play");
}

int play_btn_x(const UiState *ui) {
    int w = play_btn_w(ui);
    (void)ui;
    return ui_ctrl_x(ui) + (UI_CTRL_SIDEBAR_W - w) / 2;
}

int play_btn_y(const UiState *ui) {
    (void)ui;
    return UI_APP_CHROME_H;
}

int play_button_hit(const UiState *ui, int lx, int ly) {
    int x = play_btn_x(ui);
    int y = play_btn_y(ui);
    int w = play_btn_w(ui);
    return lx >= x && lx < x + w && ly >= y && ly < y + UI_BTN_H;
}

void screen_origin(const UiState *ui, int *ox, int *oy) {
    int sx, sy, layer_x, mx, my0;
    ui_editor_layout(ui, &sx, &sy, &layer_x, &mx, &my0);
    *ox = sx;
    *oy = sy;
}

int screen_hit(const UiState *ui, int lx, int ly, int *out_tx, int *out_ty) {
    int ox, oy;
    int lx0, ly0;
    screen_origin(ui, &ox, &oy);
    if (lx < ox || ly < oy || lx >= ox + ui_screen_w(ui) || ly >= oy + ui_screen_h(ui)) {
        return 0;
    }
    lx0 = (lx - ox) / ui_screen_scale(ui);
    ly0 = (ly - oy) / ui_screen_scale(ui);
    if (lx0 >= R01_SCREEN_PX_W || ly0 >= R01_SCREEN_PX_H) {
        return 0;
    }
    if (out_tx) {
        *out_tx = lx0 / 8;
    }
    if (out_ty) {
        *out_ty = ly0 / 8;
    }
    return 1;
}

int screen_pixel_hit(const UiState *ui, int lx, int ly, int *out_px, int *out_py) {
    int ox, oy;
    int px, py;
    screen_origin(ui, &ox, &oy);
    if (lx < ox || ly < oy || lx >= ox + ui_screen_w(ui) || ly >= oy + ui_screen_h(ui)) {
        return 0;
    }
    px = (lx - ox) / ui_screen_scale(ui);
    py = (ly - oy) / ui_screen_scale(ui);
    if (px < 0 || py < 0 || px >= R01_SCREEN_PX_W || py >= R01_SCREEN_PX_H) {
        return 0;
    }
    if (out_px) {
        *out_px = px;
    }
    if (out_py) {
        *out_py = py;
    }
    return 1;
}

void accordion_layout(const UiState *ui, AccordionLayout *lo) {
    int y = UI_APP_CHROME_H;
    int always = UI_ACCORDION_ALWAYS_EXPANDED;
    int worlds_h;
    int pals_h;
    int sprites_h;
    int global_banks_h;
    int metatiles_h;
    int metasprites_h;
    int entities_h;

    if (always) {
        worlds_h = UI_WORLDS_BODY_H;
        pals_h = UI_PAL_BODY_H;
        sprites_h = UI_SPRITES_BODY_H;
        global_banks_h = UI_GLOBAL_BANKS_BODY_H;
        metatiles_h = UI_SHOW_METATILES ? UI_METATILES_BODY_H : 0;
        metasprites_h = UI_SHOW_METASPRITES ? UI_METASPRITES_BODY_H : 0;
        entities_h = UI_ENTITIES_BODY_H;
    } else if (ui) {
        worlds_h = ui->accordion_body_h[UI_ACC_WORLDS];
        pals_h = ui->accordion_body_h[UI_ACC_PALS];
        sprites_h = ui->accordion_body_h[UI_ACC_BANKS];
        global_banks_h = ui->accordion_body_h[UI_ACC_GLOBAL_BANKS];
        metatiles_h = UI_SHOW_METATILES ? ui->accordion_body_h[UI_ACC_METATILES] : 0;
        metasprites_h = UI_SHOW_METASPRITES ? ui->accordion_body_h[UI_ACC_METASPRITES] : 0;
        entities_h = ui->accordion_body_h[UI_ACC_ENTITIES];
    } else {
        worlds_h = 0;
        pals_h = 0;
        sprites_h = 0;
        global_banks_h = 0;
        metatiles_h = 0;
        metasprites_h = 0;
        entities_h = 0;
    }

    lo->worlds_hdr_y = y;
    y += UI_BTN_H;
    lo->worlds_open = always || (ui && ui->accordion_open == UI_ACC_WORLDS);
    lo->worlds_body_h = worlds_h;
    if (worlds_h > 0) {
        lo->worlds_btns_y = y;
        lo->worlds_grid_y = y + UI_WORLDS_TAB_STACK_H;
        y += worlds_h;
    } else {
        lo->worlds_btns_y = -1;
        lo->worlds_grid_y = -1;
    }

    lo->sprites_hdr_y = y;
    y += UI_BTN_H;
    lo->sprites_open = always || (ui && ui->accordion_open == UI_ACC_BANKS);
    lo->sprites_body_h = sprites_h;
    if (sprites_h > 0) {
        lo->sprites_body_y = y;
        y += sprites_h;
    } else {
        lo->sprites_body_y = -1;
    }

    lo->global_banks_hdr_y = y;
    y += UI_BTN_H;
    lo->global_banks_open = always || (ui && ui->accordion_open == UI_ACC_GLOBAL_BANKS);
    lo->global_banks_body_h = global_banks_h;
    if (global_banks_h > 0) {
        lo->global_banks_body_y = y;
        y += global_banks_h;
    } else {
        lo->global_banks_body_y = -1;
    }

    if (UI_SHOW_METATILES) {
        lo->metatiles_hdr_y = y;
        y += UI_BTN_H;
        lo->metatiles_open = always || (ui && ui->accordion_open == UI_ACC_METATILES);
        lo->metatiles_body_h = metatiles_h;
        if (metatiles_h > 0) {
            lo->metatiles_body_y = y;
            y += metatiles_h;
        } else {
            lo->metatiles_body_y = -1;
        }
    } else {
        lo->metatiles_hdr_y = -1;
        lo->metatiles_open = 0;
        lo->metatiles_body_h = 0;
        lo->metatiles_body_y = -1;
        metatiles_h = 0;
    }
    if (UI_SHOW_METASPRITES) {
        lo->metasprites_hdr_y = y;
        y += UI_BTN_H;
        lo->metasprites_open = always || (ui && ui->accordion_open == UI_ACC_METASPRITES);
        lo->metasprites_body_h = metasprites_h;
        if (metasprites_h > 0) {
            lo->metasprites_body_y = y;
            y += metasprites_h;
        } else {
            lo->metasprites_body_y = -1;
        }
    } else {
        lo->metasprites_hdr_y = -1;
        lo->metasprites_open = 0;
        lo->metasprites_body_h = 0;
        lo->metasprites_body_y = -1;
        metasprites_h = 0;
    }

    lo->entities_hdr_y = y;
    y += UI_BTN_H;
    lo->entities_open = always || (ui && ui->accordion_open == UI_ACC_ENTITIES);
    lo->entities_body_h = entities_h;
    if (entities_h > 0) {
        lo->entities_body_y = y;
        y += entities_h;
    } else {
        lo->entities_body_y = -1;
    }

    lo->pals_hdr_y = y;
    y += UI_BTN_H;
    lo->pals_open = always || (ui && ui->accordion_open == UI_ACC_PALS);
    lo->pals_body_h = pals_h;
    if (pals_h > 0) {
        lo->pals_body_y = y;
        y += pals_h;
    } else {
        lo->pals_body_y = -1;
    }
}

static int accordion_section_full_h(int section) {
    switch (section) {
    case UI_ACC_WORLDS:
        return UI_WORLDS_BODY_H;
    case UI_ACC_PALS:
        return UI_PAL_BODY_H;
    case UI_ACC_BANKS:
        return UI_BANKS_BODY_H;
    case UI_ACC_GLOBAL_BANKS:
        return UI_GLOBAL_BANKS_BODY_H;
    case UI_ACC_METATILES:
        return UI_SHOW_METATILES ? UI_METATILES_BODY_H : 0;
    case UI_ACC_METASPRITES:
        return UI_SHOW_METASPRITES ? UI_METASPRITES_BODY_H : 0;
    case UI_ACC_ENTITIES:
        return UI_ENTITIES_BODY_H;
    default:
        return 0;
    }
}

void accordion_init_heights(UiState *ui) {
    int i;
    if (!ui) {
        return;
    }
    for (i = 0; i < UI_ACC_SECTIONS; i++) {
        int full = accordion_section_full_h(i);
        if (UI_ACCORDION_ALWAYS_EXPANDED || ui->accordion_open == i) {
            ui->accordion_body_h[i] = full;
        } else {
            ui->accordion_body_h[i] = 0;
        }
    }
    ui->accordion_anim_last_ms = SDL_GetTicks();
}

void accordion_anim_tick(UiState *ui) {
    Uint32 now;
    Uint32 dt;
    int i;

    if (!ui) {
        return;
    }
    if ((!UI_SHOW_METATILES && ui->accordion_open == UI_ACC_METATILES) ||
        (!UI_SHOW_METASPRITES && ui->accordion_open == UI_ACC_METASPRITES)) {
        ui->accordion_open = UI_ACC_NONE;
    }
    now = SDL_GetTicks();
    dt = now - ui->accordion_anim_last_ms;
    ui->accordion_anim_last_ms = now;
    if (dt > 100u) {
        dt = 100u;
    }
    if (UI_ACCORDION_ALWAYS_EXPANDED) {
        for (i = 0; i < UI_ACC_SECTIONS; i++) {
            ui->accordion_body_h[i] = accordion_section_full_h(i);
        }
        return;
    }
    for (i = 0; i < UI_ACC_SECTIONS; i++) {
        int full = accordion_section_full_h(i);
        int target = (ui->accordion_open == i) ? full : 0;
        int cur = ui->accordion_body_h[i];
        int step;

        if (cur == target || full <= 0) {
            continue;
        }
        step = (int)((long long)full * (long long)dt / UI_ACCORDION_ANIM_MS);
        if (step < 1) {
            step = 1;
        }
        if (cur < target) {
            cur += step;
            if (cur > target) {
                cur = target;
            }
        } else {
            cur -= step;
            if (cur < target) {
                cur = target;
            }
        }
        ui->accordion_body_h[i] = cur;
    }
}
int world_cell_hit(const UiState *ui, int lx, int ly, int *out_col, int *out_row) {
    AccordionLayout lo;
    int x0 = UI_WORLDS_X;
    int y0;
    int col, row;
    accordion_layout(ui, &lo);
    if (lo.worlds_body_h < 1) {
        return 0;
    }
    y0 = lo.worlds_grid_y;
    if (lo.worlds_body_h <= UI_WORLDS_TAB_STACK_H || y0 < 0) {
        return 0;
    }
    if (lx < x0 || ly < y0 || lx >= x0 + R01_GRID_MAX * UI_WORLD_CELL ||
        ly >= y0 + R01_GRID_MAX * UI_WORLD_CELL) {
        return 0;
    }
    if (ly >= lo.worlds_btns_y + lo.worlds_body_h) {
        return 0;
    }
    col = (lx - x0) / UI_WORLD_CELL;
    row = (ly - y0) / UI_WORLD_CELL;
    if (col < 0 || row < 0 || col >= R01_GRID_MAX || row >= R01_GRID_MAX) {
        return 0;
    }
    if (out_col) {
        *out_col = col;
    }
    if (out_row) {
        *out_row = row;
    }
    return 1;
}

int world_btn_hit(const UiState *ui, int lx, int ly, int *out_wi) {
    AccordionLayout lo;
    UiTabsLayout tabs;
    int sel;
    accordion_layout(ui, &lo);
    if (lo.worlds_body_h < 1 || lo.worlds_btns_y < 0) {
        return 0;
    }
    worlds_tabs_prepare(ui, &tabs);
    sel = (ui && ui->project) ? ui->project->active_world : 0;
    return ui_tabs_hit(&tabs, sel, lx, ly, out_wi);
}

void worlds_tabs_prepare(const UiState *ui, UiTabsLayout *out) {
    AccordionLayout lo;
    static const char *const world_labs[R01_MAX_WORLDS] = {"", "", "", "", "", "", ""};
    int view;
    if (!out) {
        return;
    }
    accordion_layout(ui, &lo);
    ui_tabs_layout(world_labs, R01_MAX_WORLDS, UI_WORLDS_X, lo.worlds_btns_y, UI_WORLD_BTN, out);
    ui_tabs_set_dot(out, 1);
    view = (ui && ui->worlds_plane == UI_WORLDS_PLANE_BG0) ? 0 : 1;
    /* view 0 shows BG0 asset (far plane selected), view 1 shows BG1 asset */
    ui_tabs_set_dual(out, 1, view, g_bg0_btn_rgba, g_bg0_btn_w, g_bg0_btn_h, g_bg1_btn_rgba, g_bg1_btn_w,
                     g_bg1_btn_h);
}

void banks_tabs_prepare(const UiState *ui, UiTabsLayout *out) {
    AccordionLayout lo;
    static const char *const bank_labs[UI_BANKS_N] = {"", "", "", ""};
    int view;
    if (!out) {
        return;
    }
    accordion_layout(ui, &lo);
    ui_tabs_layout(bank_labs, UI_BANKS_N, UI_WORLDS_X, lo.sprites_body_y, UI_WORLD_BTN, out);
    ui_tabs_set_dot(out, 1);
    view = (ui && ui->banks_plane == UI_BANKS_PLANE_SPR) ? 1 : 0;
    ui_tabs_set_dual(out, 1, view, g_bg_bank_btn_rgba, g_bg_bank_btn_w, g_bg_bank_btn_h, g_spr_bank_btn_rgba,
                     g_spr_bank_btn_w, g_spr_bank_btn_h);
}

void global_banks_tabs_prepare(const UiState *ui, UiTabsLayout *out) {
    AccordionLayout lo;
    static const char *const bank_labs[UI_BANKS_N] = {"", "", "", ""};
    int view;
    if (!out) {
        return;
    }
    accordion_layout(ui, &lo);
    ui_tabs_layout(bank_labs, UI_BANKS_N, UI_WORLDS_X, lo.global_banks_body_y, UI_WORLD_BTN, out);
    ui_tabs_set_dot(out, 1);
    view = (ui && ui->global_banks_plane == UI_BANKS_PLANE_GLOBAL_SPR) ? 1 : 0;
    ui_tabs_set_dual(out, 1, view, g_bg_bank_btn_rgba, g_bg_bank_btn_w, g_bg_bank_btn_h, g_spr_bank_btn_rgba,
                     g_spr_bank_btn_w, g_spr_bank_btn_h);
}

int banks_tab_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    UiTabsLayout tabs;
    AccordionLayout lo;
    int sel;
    if (!ui) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.sprites_body_h < 1) {
        return 0;
    }
    banks_tabs_prepare(ui, &tabs);
    sel = ui->banks_idx;
    if (sel < 0) {
        sel = 0;
    }
    if (sel >= UI_BANKS_N) {
        sel = UI_BANKS_N - 1;
    }
    return ui_tabs_hit(&tabs, sel, lx, ly, out_idx);
}

int global_banks_tab_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    UiTabsLayout tabs;
    AccordionLayout lo;
    int sel;
    if (!ui) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.global_banks_body_h < 1) {
        return 0;
    }
    global_banks_tabs_prepare(ui, &tabs);
    sel = ui->global_banks_idx;
    if (sel < 0) {
        sel = 0;
    }
    if (sel >= UI_BANKS_N) {
        sel = UI_BANKS_N - 1;
    }
    return ui_tabs_hit(&tabs, sel, lx, ly, out_idx);
}

int banks_sub_hit(const UiState *ui, int lx, int ly) {
    UiTabsLayout tabs;
    int sel;
    if (!ui) {
        return 0;
    }
    banks_tabs_prepare(ui, &tabs);
    sel = ui->banks_idx;
    if (sel < 0) {
        sel = 0;
    }
    if (sel >= UI_BANKS_N) {
        sel = UI_BANKS_N - 1;
    }
    return ui_tabs_sub_hit(&tabs, sel, lx, ly);
}

int global_banks_sub_hit(const UiState *ui, int lx, int ly) {
    UiTabsLayout tabs;
    int sel;
    if (!ui) {
        return 0;
    }
    global_banks_tabs_prepare(ui, &tabs);
    sel = ui->global_banks_idx;
    if (sel < 0) {
        sel = 0;
    }
    if (sel >= UI_BANKS_N) {
        sel = UI_BANKS_N - 1;
    }
    return ui_tabs_sub_hit(&tabs, sel, lx, ly);
}

int banks_cell_hit(const UiState *ui, int lx, int ly, int *out_tile_id) {
    AccordionLayout lo;
    int grid_y;
    int tx, ty;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.sprites_body_h < UI_BANKS_BODY_H) {
        return 0;
    }
    grid_y = lo.sprites_body_y + UI_WORLDS_TAB_STACK_H;
    if (lx < UI_WORLDS_X || lx >= UI_WORLDS_X + UI_BANKS_GRID || ly < grid_y || ly >= grid_y + UI_BANKS_GRID) {
        return 0;
    }
    tx = (lx - UI_WORLDS_X) / 8;
    ty = (ly - grid_y) / 8;
    if (tx < 0 || tx >= 16 || ty < 0 || ty >= 16) {
        return 0;
    }
    if (out_tile_id) {
        *out_tile_id = ty * 16 + tx;
    }
    return 1;
}

int global_banks_cell_hit(const UiState *ui, int lx, int ly, int *out_tile_id) {
    AccordionLayout lo;
    int grid_y;
    int tx, ty;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.global_banks_body_h < UI_GLOBAL_BANKS_BODY_H) {
        return 0;
    }
    grid_y = lo.global_banks_body_y + UI_WORLDS_TAB_STACK_H;
    if (lx < UI_WORLDS_X || lx >= UI_WORLDS_X + UI_BANKS_GRID || ly < grid_y || ly >= grid_y + UI_BANKS_GRID) {
        return 0;
    }
    tx = (lx - UI_WORLDS_X) / 8;
    ty = (ly - grid_y) / 8;
    if (tx < 0 || tx >= 16 || ty < 0 || ty >= 16) {
        return 0;
    }
    if (out_tile_id) {
        *out_tile_id = ty * 16 + tx;
    }
    return 1;
}

static void bank_sel_bit_set(uint32_t *mask, int tile_id) {
    if (!mask || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return;
    }
    mask[tile_id >> 5] |= 1u << (tile_id & 31);
}

static void bank_sel_bit_clear(uint32_t *mask, int tile_id) {
    if (!mask || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return;
    }
    mask[tile_id >> 5] &= ~(1u << (tile_id & 31));
}

static int bank_sel_bit_get(const uint32_t *mask, int tile_id) {
    if (!mask || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return 0;
    }
    return (int)((mask[tile_id >> 5] >> (tile_id & 31)) & 1u);
}

static int bank_sel_tile_count(const UiState *ui, int plane, int bank) {
    const R01World *w;
    if (!ui || !ui->project || bank < 0 || bank >= UI_BANKS_N) {
        return 0;
    }
    w = r01_project_active_world_const(ui->project);
    if (plane == UI_BANKS_PLANE_GLOBAL_SPR) {
        return ui->project->other_spr_banks[bank].tile_count;
    }
    if (plane == UI_BANKS_PLANE_GLOBAL_BG) {
        return ui->project->other_bg_banks[bank].tile_count;
    }
    if (!w) {
        return 0;
    }
    if (plane == UI_BANKS_PLANE_SPR) {
        return w->spr_banks[bank].tile_count;
    }
    if (plane == UI_BANKS_PLANE_BG) {
        return w->bg_banks[bank].tile_count;
    }
    return 0;
}

static void bank_sel_densify(UiState *ui, int plane, int bank) {
    R01World *w;
    if (!ui || !ui->project || bank < 0 || bank >= UI_BANKS_N) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (plane == UI_BANKS_PLANE_GLOBAL_SPR) {
        r01_project_densify_other_spr_bank(ui->project, bank);
    } else if (plane == UI_BANKS_PLANE_GLOBAL_BG) {
        r01_project_densify_other_bg_bank(ui->project, bank);
    } else if (w && plane == UI_BANKS_PLANE_SPR) {
        r01_chr_densify_spr_bank(w, bank);
    } else if (w && plane == UI_BANKS_PLANE_BG) {
        r01_chr_densify_bg_bank(w, bank);
    }
}

static void bank_sel_refresh_primary(UiState *ui) {
    int i;
    if (!ui) {
        return;
    }
    ui->bank_sel_tile = -1;
    for (i = R01_TILES_PER_BANK - 1; i >= 0; i--) {
        if (bank_sel_bit_get(ui->bank_sel_mask, i)) {
            ui->bank_sel_tile = i;
            ui_paint_stamp_from_bank(ui, ui->bank_sel_plane, ui->bank_sel_bank, i);
            return;
        }
    }
}

static void bank_sel_begin_plane(UiState *ui, int plane, int bank) {
    if (!ui) {
        return;
    }
    ui->bank_sel_plane = plane;
    ui->bank_sel_bank = bank;
    ui->sel_instance = -1;
    ui->inst_drag = 0;
    screen_sel_clear(ui);
}

void bank_sel_clear(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->bank_sel_tile = -1;
    ui->bank_sel_bank = 0;
    ui->bank_sel_plane = UI_BANKS_PLANE_SPR;
    memset(ui->bank_sel_mask, 0, sizeof(ui->bank_sel_mask));
    memset(ui->bank_sel_mask_before, 0, sizeof(ui->bank_sel_mask_before));
    ui->bank_sel_drag = 0;
    ui->bank_sel_drag_moved = 0;
    ui->bank_sel_anchor = 0;
    ui->bank_sel_drag_tile = 0;
    ui->bank_sel_add = 0;
}

void bank_sel_set(UiState *ui, int plane, int bank, int tile_id) {
    int count;
    if (!ui || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        bank_sel_clear(ui);
        return;
    }
    if (bank < 0 || bank >= UI_BANKS_N) {
        bank_sel_clear(ui);
        return;
    }
    /* Heal mid/trailing blank holes when interacting with Banks. */
    bank_sel_densify(ui, plane, bank);
    count = bank_sel_tile_count(ui, plane, bank);
    if (tile_id >= count) {
        bank_sel_clear(ui);
        return;
    }
    memset(ui->bank_sel_mask, 0, sizeof(ui->bank_sel_mask));
    bank_sel_bit_set(ui->bank_sel_mask, tile_id);
    bank_sel_begin_plane(ui, plane, bank);
    ui->bank_sel_tile = tile_id;
    /* BG bank pick becomes the Ctrl+click map brush. */
    ui_paint_stamp_from_bank(ui, plane, bank, tile_id);
}

void bank_sel_toggle(UiState *ui, int plane, int bank, int tile_id) {
    int count;
    if (!ui || tile_id < 0 || tile_id >= R01_TILES_PER_BANK || bank < 0 || bank >= UI_BANKS_N) {
        return;
    }
    bank_sel_densify(ui, plane, bank);
    count = bank_sel_tile_count(ui, plane, bank);
    if (tile_id >= count) {
        return;
    }
    if (!bank_sel_valid(ui) || ui->bank_sel_plane != plane || ui->bank_sel_bank != bank) {
        bank_sel_set(ui, plane, bank, tile_id);
        return;
    }
    if (bank_sel_bit_get(ui->bank_sel_mask, tile_id)) {
        bank_sel_bit_clear(ui->bank_sel_mask, tile_id);
        if (ui->bank_sel_tile == tile_id) {
            bank_sel_refresh_primary(ui);
        }
        if (!bank_sel_valid(ui)) {
            bank_sel_clear(ui);
        }
        return;
    }
    bank_sel_bit_set(ui->bank_sel_mask, tile_id);
    ui->bank_sel_tile = tile_id;
    ui_paint_stamp_from_bank(ui, plane, bank, tile_id);
}

void bank_sel_select_all(UiState *ui) {
    int plane, bank, count, i;
    int region;
    if (!ui || ui->play.active) {
        return;
    }
    region = ui_region_get(ui);
    if (region == UI_REGION_GLOBAL_BANKS) {
        plane = ui->global_banks_plane;
        bank = ui->global_banks_idx;
    } else if (region == UI_REGION_BANKS) {
        plane = ui->banks_plane;
        bank = ui->banks_idx;
    } else {
        return;
    }
    if (bank < 0 || bank >= UI_BANKS_N) {
        return;
    }
    bank_sel_densify(ui, plane, bank);
    count = bank_sel_tile_count(ui, plane, bank);
    memset(ui->bank_sel_mask, 0, sizeof(ui->bank_sel_mask));
    if (count < 1) {
        bank_sel_clear(ui);
        return;
    }
    for (i = 0; i < count; i++) {
        bank_sel_bit_set(ui->bank_sel_mask, i);
    }
    bank_sel_begin_plane(ui, plane, bank);
    bank_sel_refresh_primary(ui);
}

void bank_sel_select_rect(UiState *ui, int tile_a, int tile_b, int add) {
    int x0, y0, x1, y1, x, y, count;
    int plane, bank;
    if (!ui) {
        return;
    }
    plane = ui->bank_sel_plane;
    bank = ui->bank_sel_bank;
    if (bank < 0 || bank >= UI_BANKS_N) {
        return;
    }
    if (tile_a < 0) {
        tile_a = 0;
    }
    if (tile_b < 0) {
        tile_b = 0;
    }
    if (tile_a >= R01_TILES_PER_BANK) {
        tile_a = R01_TILES_PER_BANK - 1;
    }
    if (tile_b >= R01_TILES_PER_BANK) {
        tile_b = R01_TILES_PER_BANK - 1;
    }
    x0 = tile_a % 16;
    y0 = tile_a / 16;
    x1 = tile_b % 16;
    y1 = tile_b / 16;
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
    bank_sel_densify(ui, plane, bank);
    count = bank_sel_tile_count(ui, plane, bank);
    if (add) {
        memcpy(ui->bank_sel_mask, ui->bank_sel_mask_before, sizeof(ui->bank_sel_mask));
    } else {
        memset(ui->bank_sel_mask, 0, sizeof(ui->bank_sel_mask));
    }
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            int id = y * 16 + x;
            if (id < count) {
                bank_sel_bit_set(ui->bank_sel_mask, id);
            }
        }
    }
    bank_sel_begin_plane(ui, plane, bank);
    bank_sel_refresh_primary(ui);
    if (!bank_sel_valid(ui)) {
        ui->bank_sel_plane = plane;
        ui->bank_sel_bank = bank;
    }
}

void bank_sel_drop_tile(UiState *ui, int plane, int bank, int tile_id) {
    if (!ui || !bank_sel_valid(ui)) {
        return;
    }
    if (ui->bank_sel_plane != plane || ui->bank_sel_bank != bank) {
        return;
    }
    bank_sel_bit_clear(ui->bank_sel_mask, tile_id);
    if (ui->bank_sel_tile == tile_id) {
        bank_sel_refresh_primary(ui);
    }
    if (!bank_sel_valid(ui)) {
        bank_sel_clear(ui);
    }
}

int bank_sel_valid(const UiState *ui) {
    int i;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < UI_BANK_SEL_WORDS; i++) {
        if (ui->bank_sel_mask[i]) {
            return 1;
        }
    }
    return ui->bank_sel_tile >= 0 && ui->bank_sel_tile < R01_TILES_PER_BANK;
}

int bank_sel_has(const UiState *ui, int tile_id) {
    return ui && bank_sel_bit_get(ui->bank_sel_mask, tile_id);
}

int bank_sel_count(const UiState *ui) {
    int i, n = 0;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < R01_TILES_PER_BANK; i++) {
        if (bank_sel_bit_get(ui->bank_sel_mask, i)) {
            n++;
        }
    }
    return n;
}

int bank_sel_is_multi(const UiState *ui) {
    return bank_sel_count(ui) > 1;
}

int bank_sel_cell_clamped(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    int grid_y, tx, ty;
    int global;
    if (!ui) {
        return 0;
    }
    accordion_layout(ui, &lo);
    global = (ui->bank_sel_plane == UI_BANKS_PLANE_GLOBAL_BG || ui->bank_sel_plane == UI_BANKS_PLANE_GLOBAL_SPR);
    grid_y = (global ? lo.global_banks_body_y : lo.sprites_body_y) + UI_WORLDS_TAB_STACK_H;
    tx = (lx - UI_WORLDS_X) / 8;
    ty = (ly - grid_y) / 8;
    if (tx < 0) {
        tx = 0;
    }
    if (tx > 15) {
        tx = 15;
    }
    if (ty < 0) {
        ty = 0;
    }
    if (ty > 15) {
        ty = 15;
    }
    return ty * 16 + tx;
}

int world_sub_hit(const UiState *ui, int lx, int ly) {
    UiTabsLayout tabs;
    int sel;
    if (!ui || !ui->project) {
        return 0;
    }
    worlds_tabs_prepare(ui, &tabs);
    sel = ui->project->active_world;
    return ui_tabs_sub_hit(&tabs, sel, lx, ly);
}

int accordion_header_hit(const UiState *ui, int lx, int ly, int *out_section) {
    AccordionLayout lo;
    if (UI_ACCORDION_ALWAYS_EXPANDED) {
        return 0;
    }
    if (lx < 0 || lx >= UI_SIDEBAR_W) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (ly >= lo.worlds_hdr_y && ly < lo.worlds_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_WORLDS;
        }
        return 1;
    }
    if (ly >= lo.pals_hdr_y && ly < lo.pals_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_PALS;
        }
        return 1;
    }
    if (ly >= lo.sprites_hdr_y && ly < lo.sprites_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_BANKS;
        }
        return 1;
    }
    if (ly >= lo.global_banks_hdr_y && ly < lo.global_banks_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_GLOBAL_BANKS;
        }
        return 1;
    }
    if (UI_SHOW_METATILES && ly >= lo.metatiles_hdr_y && ly < lo.metatiles_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_METATILES;
        }
        return 1;
    }
    if (UI_SHOW_METASPRITES && ly >= lo.metasprites_hdr_y && ly < lo.metasprites_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_METASPRITES;
        }
        return 1;
    }
    if (ly >= lo.entities_hdr_y && ly < lo.entities_hdr_y + UI_BTN_H) {
        if (out_section) {
            *out_section = UI_ACC_ENTITIES;
        }
        return 1;
    }
    return 0;
}

static int region_section_hit(int lx, int ly, int hdr_y, int body_y, int body_h) {
    if (lx < 0 || lx >= UI_SIDEBAR_W || hdr_y < 0) {
        return 0;
    }
    if (ly >= hdr_y && ly < hdr_y + UI_BTN_H) {
        return 1;
    }
    if (body_h > 0 && body_y >= 0 && ly >= body_y && ly < body_y + body_h) {
        return 1;
    }
    return 0;
}

int ui_region_at(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    if (!ui) {
        return UI_REGION_NONE;
    }
    if (ly < UI_APP_CHROME_H) {
        return UI_REGION_NONE;
    }
    if (ui->app_mode == UI_APP_SOUNDS) {
        return UI_REGION_SOUNDS;
    }
    if (ui->app_mode != UI_APP_GRAPHICS) {
        return UI_REGION_NONE;
    }
    if (lx >= ui_ctrl_x(ui)) {
        return UI_REGION_CTRL;
    }
    if (lx >= UI_SIDEBAR_W) {
        return UI_REGION_PREVIEW;
    }
    accordion_layout(ui, &lo);
    if (region_section_hit(lx, ly, lo.worlds_hdr_y, lo.worlds_btns_y, lo.worlds_body_h)) {
        return UI_REGION_WORLDS;
    }
    if (region_section_hit(lx, ly, lo.sprites_hdr_y, lo.sprites_body_y, lo.sprites_body_h)) {
        return UI_REGION_BANKS;
    }
    if (region_section_hit(lx, ly, lo.global_banks_hdr_y, lo.global_banks_body_y, lo.global_banks_body_h)) {
        return UI_REGION_GLOBAL_BANKS;
    }
    if (UI_SHOW_METATILES &&
        region_section_hit(lx, ly, lo.metatiles_hdr_y, lo.metatiles_body_y, lo.metatiles_body_h)) {
        return UI_REGION_METATILES;
    }
    if (UI_SHOW_METASPRITES &&
        region_section_hit(lx, ly, lo.metasprites_hdr_y, lo.metasprites_body_y, lo.metasprites_body_h)) {
        return UI_REGION_METASPRITES;
    }
    if (region_section_hit(lx, ly, lo.entities_hdr_y, lo.entities_body_y, lo.entities_body_h)) {
        return UI_REGION_ENTITIES;
    }
    if (region_section_hit(lx, ly, lo.pals_hdr_y, lo.pals_body_y, lo.pals_body_h)) {
        return UI_REGION_PALS;
    }
    return UI_REGION_NONE;
}

void ui_region_focus_at(UiState *ui, int lx, int ly) {
    int region = ui_region_at(ui, lx, ly);
    if (region != UI_REGION_NONE) {
        ui_region_set(ui, region);
    }
}

int ui_region_rect(const UiState *ui, int region, int *x, int *y, int *w, int *h) {
    AccordionLayout lo;
    int sx, sy;
    if (!ui || !x || !y || !w || !h) {
        return 0;
    }
    if (region == UI_REGION_PREVIEW) {
        ui_editor_layout(ui, &sx, &sy, NULL, NULL, NULL);
        *x = sx;
        *y = sy;
        *w = ui_screen_w(ui);
        *h = ui_screen_h(ui);
        return 1;
    }
    if (region == UI_REGION_CTRL) {
        *x = ui_ctrl_x(ui);
        *y = UI_APP_CHROME_H;
        *w = UI_CTRL_SIDEBAR_W;
        *h = ui_logic_h(ui) - UI_APP_CHROME_H;
        return 1;
    }
    if (region == UI_REGION_SOUNDS) {
        *x = 0;
        *y = UI_APP_CHROME_H;
        *w = ui_logic_w(ui);
        *h = ui_logic_h(ui) - UI_APP_CHROME_H;
        return 1;
    }
    accordion_layout(ui, &lo);
    *x = 0;
    *w = UI_SIDEBAR_W;
    switch (region) {
    case UI_REGION_WORLDS:
        *y = lo.worlds_hdr_y;
        *h = UI_BTN_H + (lo.worlds_body_h > 0 ? lo.worlds_body_h : 0);
        return 1;
    case UI_REGION_BANKS:
        *y = lo.sprites_hdr_y;
        *h = UI_BTN_H + (lo.sprites_body_h > 0 ? lo.sprites_body_h : 0);
        return 1;
    case UI_REGION_GLOBAL_BANKS:
        *y = lo.global_banks_hdr_y;
        *h = UI_BTN_H + (lo.global_banks_body_h > 0 ? lo.global_banks_body_h : 0);
        return 1;
    case UI_REGION_METATILES:
        if (!UI_SHOW_METATILES) {
            return 0;
        }
        *y = lo.metatiles_hdr_y;
        *h = UI_BTN_H + (lo.metatiles_body_h > 0 ? lo.metatiles_body_h : 0);
        return 1;
    case UI_REGION_METASPRITES:
        if (!UI_SHOW_METASPRITES) {
            return 0;
        }
        *y = lo.metasprites_hdr_y;
        *h = UI_BTN_H + (lo.metasprites_body_h > 0 ? lo.metasprites_body_h : 0);
        return 1;
    case UI_REGION_ENTITIES:
        *y = lo.entities_hdr_y;
        *h = UI_BTN_H + (lo.entities_body_h > 0 ? lo.entities_body_h : 0);
        return 1;
    case UI_REGION_PALS:
        *y = lo.pals_hdr_y;
        *h = UI_BTN_H + (lo.pals_body_h > 0 ? lo.pals_body_h : 0);
        return 1;
    default:
        return 0;
    }
}

void draw_region_focus(UiState *ui, SDL_Renderer *r) {
    int x, y, w, h;
    if (!ui || !r || ui->play.active || ui->menu.open || ui->tile_edit.open || ui->sprite_edit.open ||
        ui->metasprite_edit.open || ui->entity_edit.open || ui->pal_edit.open) {
        return;
    }
    if (ui->app_mode != UI_APP_GRAPHICS) {
        return;
    }
    if (!ui_region_rect(ui, ui->region_focus, &x, &y, &w, &h) || w < 1 || h < 1) {
        return;
    }
    draw_rect(r, x, y, w, h, UI_COL_PRESENT_R, UI_COL_PRESENT_G, UI_COL_PRESENT_B);
}

void accordion_toggle(UiState *ui, int section) {
    if (UI_ACCORDION_ALWAYS_EXPANDED) {
        return;
    }
    if ((!UI_SHOW_METATILES && section == UI_ACC_METATILES) ||
        (!UI_SHOW_METASPRITES && section == UI_ACC_METASPRITES)) {
        return;
    }
    if (ui->accordion_open == section) {
        ui->accordion_open = UI_ACC_NONE;
    } else {
        ui->accordion_open = section;
    }
}

void draw_accordion_header(SDL_Renderer *r, int y, const char *title, int open, int hover) {
    if (open) {
        fill_rect(r, 0, y, UI_SIDEBAR_W, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    } else {
        fill_rect(r, 0, y, UI_SIDEBAR_W, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
    }
    font_draw_centered(r, 0, y, UI_SIDEBAR_W, UI_BTN_H, title, 230, 230, 230);
    if (hover && !UI_ACCORDION_ALWAYS_EXPANDED) {
        hover_overlay(r, 0, y, UI_SIDEBAR_W, UI_BTN_H);
    }
}

int metasprites_list_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    AccordionLayout lo;
    const R01World *w;
    int row, idx;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.metasprites_body_h < 1) {
        return 0;
    }
    w = r01_project_active_world_const(ui->project);
    if (!w || w->metasprite_count < 1) {
        return 0;
    }
    if (lx < UI_WORLDS_X || lx >= UI_SIDEBAR_W || ly < lo.metasprites_body_y ||
        ly >= lo.metasprites_body_y + lo.metasprites_body_h) {
        return 0;
    }
    row = (ly - lo.metasprites_body_y) / UI_SPRITE_ROW_H;
    idx = ui->metasprites_scroll + row;
    if (idx < 0 || idx >= w->metasprite_count) {
        return 0;
    }
    if (out_idx) {
        *out_idx = idx;
    }
    return 1;
}

int metatiles_list_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    AccordionLayout lo;
    const R01World *w;
    int row, idx;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.metatiles_body_h < 1) {
        return 0;
    }
    w = r01_project_active_world_const(ui->project);
    if (!w || w->metatile_count < 1) {
        return 0;
    }
    if (lx < UI_WORLDS_X || lx >= UI_SIDEBAR_W || ly < lo.metatiles_body_y ||
        ly >= lo.metatiles_body_y + lo.metatiles_body_h) {
        return 0;
    }
    row = (ly - lo.metatiles_body_y) / UI_SPRITE_ROW_H;
    idx = ui->metatiles_scroll + row;
    if (idx < 0 || idx >= w->metatile_count) {
        return 0;
    }
    if (out_idx) {
        *out_idx = idx;
    }
    return 1;
}

int metatiles_add_hit(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    int add_y;
    int add_w;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.metatiles_body_h < UI_BTN_H) {
        return 0;
    }
    add_y = lo.metatiles_body_y + UI_METATILES_BODY_H - UI_BTN_H;
    add_w = label_width("Add");
    return point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H) &&
           ly < lo.metatiles_body_y + lo.metatiles_body_h;
}

int metasprites_add_hit(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    int add_y;
    int add_w;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.metasprites_body_h < UI_BTN_H) {
        return 0;
    }
    add_y = lo.metasprites_body_y + UI_METASPRITES_BODY_H - UI_BTN_H;
    add_w = label_width("Add");
    return point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H) &&
           ly < lo.metasprites_body_y + lo.metasprites_body_h;
}

int entities_list_hit(const UiState *ui, int lx, int ly, int *out_type_idx) {
    AccordionLayout lo;
    const R01World *w;
    int row, idx;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.entities_body_h < 1) {
        return 0;
    }
    w = r01_project_active_world_const(ui->project);
    if (!w || w->entity_count < 1) {
        return 0;
    }
    if (lx < UI_WORLDS_X || lx >= UI_SIDEBAR_W || ly < lo.entities_body_y ||
        ly >= lo.entities_body_y + lo.entities_body_h) {
        return 0;
    }
    row = (ly - lo.entities_body_y) / UI_SPRITE_ROW_H;
    idx = ui->entities_scroll + row;
    if (idx < 0 || idx >= w->entity_count) {
        return 0;
    }
    if (out_type_idx) {
        *out_type_idx = idx;
    }
    return 1;
}

int entities_add_hit(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    int add_y;
    int add_w;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.entities_body_h < UI_BTN_H) {
        return 0;
    }
    add_y = lo.entities_body_y + UI_ENTITIES_BODY_H - UI_BTN_H;
    add_w = label_width("Add");
    return point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H) &&
           ly < lo.entities_body_y + lo.entities_body_h;
}

int entities_import_hit(const UiState *ui, int lx, int ly) {
    AccordionLayout lo;
    int add_y;
    int add_w;
    int imp_x;
    int imp_w;
    if (!ui || ui->play.active) {
        return 0;
    }
    accordion_layout(ui, &lo);
    if (lo.entities_body_h < UI_BTN_H) {
        return 0;
    }
    add_y = lo.entities_body_y + UI_ENTITIES_BODY_H - UI_BTN_H;
    add_w = label_width("Add");
    imp_w = label_width("Import");
    imp_x = UI_WORLDS_X + UI_UNIT + add_w + UI_UNIT;
    return point_in_rect(lx, ly, imp_x, add_y, imp_w, UI_BTN_H) &&
           ly < lo.entities_body_y + lo.entities_body_h;
}

int sprites_list_hit(const UiState *ui, int lx, int ly, int *out_catalog_idx) {
    (void)ui;
    (void)lx;
    (void)ly;
    (void)out_catalog_idx;
    /* Sprites list replaced by Banks grid. */
    return 0;
}

int sprites_add_hit(const UiState *ui, int lx, int ly) {
    (void)ui;
    (void)lx;
    (void)ly;
    return 0;
}

void app_mode_tabs_prepare(const UiState *ui, UiTabsLayout *out) {
    static const char *const labs[] = {"Graphics", "Audio", "Code"};
    int ga_w;
    int code_w;
    (void)ui;
    if (!out) {
        return;
    }
    /* Graphics|Audio keep the historic equal split of the left sidebar width. Code sits to the right. */
    ga_w = UI_SIDEBAR_W / 2;
    code_w = label_width("Code");
    if (code_w < UI_UNIT * 4) {
        code_w = UI_UNIT * 4;
    }
    ui_tabs_layout(labs, 3, 0, 0, ga_w, out);
    out->tab_ws[0] = ga_w;
    out->tab_ws[1] = ga_w;
    out->tab_ws[2] = code_w;
    out->tab_h = UI_BTN_H;
}

int app_mode_tab_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    UiTabsLayout tabs;
    app_mode_tabs_prepare(ui, &tabs);
    return ui_tabs_hit(&tabs, ui ? ui->app_mode : 0, lx, ly, out_idx);
}

void draw_app_mode_tabs(UiState *ui, SDL_Renderer *r) {
    UiTabsLayout tabs;
    if (!ui || !r) {
        return;
    }
    fill_rect(r, 0, 0, ui_logic_w(ui), UI_APP_CHROME_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
    app_mode_tabs_prepare(ui, &tabs);
    ui_tabs_draw(r, &tabs, ui->app_mode, ui->mouse_x, ui->mouse_y);
}

void sound_editor_layout(const UiState *ui, SoundEditorLayout *lo) {
    int chrome = UI_APP_CHROME_H;
    int tab_w;
    int btn_gap = UI_UNIT;
    if (!lo) {
        return;
    }
    memset(lo, 0, sizeof(*lo));
    lo->content_y = chrome;
    /* Compact text-sized tabs, flush left (same edge as Graphics|Audio). */
    tab_w = font_text_width("BGM");
    if (font_text_width("SFX") > tab_w) {
        tab_w = font_text_width("SFX");
    }
    tab_w += UI_UNIT;
    tab_w = ((tab_w + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    if (tab_w < UI_UNIT) {
        tab_w = UI_UNIT;
    }
    lo->plane_tab_w = tab_w;
    lo->plane_tabs_x = 0;
    lo->plane_tabs_y = chrome;
    lo->track_list_y = chrome + UI_BTN_H + UI_UNIT;
    lo->track_row_h = UI_SPRITE_ROW_H; /* 16 */
    lo->add_w = ((label_width("Add") + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    lo->add_x = UI_WORLDS_X + UI_UNIT;
    lo->add_y = ui_logic_h(ui) - UI_BTN_H - UI_UNIT;
    if (lo->add_y < lo->track_list_y + UI_BTN_H) {
        lo->add_y = lo->track_list_y + UI_BTN_H;
    }
    lo->zoom_s = UI_BTN_H;
    lo->zoom_out_x = UI_UNIT;
    lo->zoom_in_x = lo->zoom_out_x + lo->zoom_s + UI_UNIT;
    {
        int n = ui->sound.track_count;
        if (n < 0) {
            n = 0;
        }
        if (n > UI_SOUND_TRACKS_MAX) {
            n = UI_SOUND_TRACKS_MAX;
        }
        lo->zoom_y = lo->track_list_y + n * lo->track_row_h + UI_UNIT;
    }

    lo->lane_label_w = ((label_width("Pulse1") + UI_UNIT + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    lo->lane_label_x = UI_SIDEBAR_W + UI_UNIT;
    lo->timeline_x = lo->lane_label_x + lo->lane_label_w;
    lo->hdr_y = chrome + UI_UNIT;
    lo->ruler_h = UI_BTN_H; /* 16 */
    lo->timeline_y = lo->hdr_y + UI_BTN_H + lo->ruler_h;
    lo->lane_h = UI_SOUND_LANE_H;
    lo->lane_gap = UI_SOUND_LANE_GAP;
    {
        int z = ui->sound.zoom_h;
        if (z < UI_SOUND_ZOOM_MIN) {
            z = UI_SOUND_ZOOM_MIN;
        }
        if (z > UI_SOUND_ZOOM_MAX) {
            z = UI_SOUND_ZOOM_MAX;
        }
        lo->px_per_tick = UI_SOUND_PX_PER_TICK * z;
    }
    lo->timeline_h = UI_SOUND_BGM_CH * (lo->lane_h + lo->lane_gap) - lo->lane_gap;
    lo->minimap_h = UI_SOUND_MINIMAP_H;
    lo->minimap_y = lo->timeline_y + lo->timeline_h + UI_UNIT;
    {
        int max_w = ui_ctrl_x(ui) - UI_UNIT - lo->timeline_x;
        max_w = (max_w / UI_UNIT) * UI_UNIT;
        if (max_w < lo->px_per_tick * 4) {
            max_w = lo->px_per_tick * 4;
        }
        lo->timeline_w = max_w;
    }
    lo->visible_ticks = lo->timeline_w / lo->px_per_tick;
    if (lo->visible_ticks < 1) {
        lo->visible_ticks = 1;
    }

    lo->insp_x = ui_ctrl_x(ui) + UI_UNIT;
    lo->insp_y = chrome + UI_UNIT;
    lo->play_w = ((label_width("Play") + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    lo->pause_w = ((label_width("Pause") + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    lo->stop_w = ((label_width("Stop") + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    lo->play_x = lo->insp_x;
    lo->play_y = lo->insp_y;
    lo->pause_x = lo->play_x + lo->play_w + btn_gap;
    lo->pause_y = lo->play_y;
    lo->stop_x = lo->pause_x + lo->pause_w + btn_gap;
    lo->stop_y = lo->play_y;
    /* Isolate radios: All on ruler row, channels centered on each lane. */
    lo->ch_radio_y0 = lo->timeline_y - lo->ruler_h;
}

void sound_plane_tabs_prepare(const UiState *ui, UiTabsLayout *out) {
    static const char *const labs[] = {"BGM", "SFX"};
    SoundEditorLayout lo;
    sound_editor_layout(ui, &lo);
    if (!out) {
        return;
    }
    ui_tabs_layout(labs, 2, lo.plane_tabs_x, lo.plane_tabs_y, lo.plane_tab_w, out);
    out->tab_h = UI_BTN_H;
}

int sound_plane_tab_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    UiTabsLayout tabs;
    sound_plane_tabs_prepare(ui, &tabs);
    return ui_tabs_hit(&tabs, ui ? ui->sound.plane : 0, lx, ly, out_idx);
}

int sound_track_hit(const UiState *ui, int lx, int ly, int *out_idx) {
    SoundEditorLayout lo;
    int i;
    int n;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    n = ui->sound.track_count;
    if (n < 0) {
        n = 0;
    }
    if (n > UI_SOUND_TRACKS_MAX) {
        n = UI_SOUND_TRACKS_MAX;
    }
    for (i = 0; i < n; i++) {
        int y = lo.track_list_y + i * lo.track_row_h;
        if (point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, lo.track_row_h)) {
            if (out_idx) {
                *out_idx = i;
            }
            return 1;
        }
    }
    return 0;
}

int sound_add_hit(const UiState *ui, int lx, int ly) {
    SoundEditorLayout lo;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    return point_in_rect(lx, ly, lo.add_x, lo.add_y, lo.add_w, UI_BTN_H);
}

int sound_zoom_out_hit(const UiState *ui, int lx, int ly) {
    SoundEditorLayout lo;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    return point_in_rect(lx, ly, lo.zoom_out_x, lo.zoom_y, lo.zoom_s, lo.zoom_s);
}

int sound_zoom_in_hit(const UiState *ui, int lx, int ly) {
    SoundEditorLayout lo;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    return point_in_rect(lx, ly, lo.zoom_in_x, lo.zoom_y, lo.zoom_s, lo.zoom_s);
}

int sound_timeline_hit(const UiState *ui, int lx, int ly, int *out_ch, int *out_tick) {
    SoundEditorLayout lo;
    int ch;
    int tick;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    if (!point_in_rect(lx, ly, lo.timeline_x, lo.timeline_y, lo.timeline_w, lo.timeline_h)) {
        return 0;
    }
    ch = (ly - lo.timeline_y) / (lo.lane_h + lo.lane_gap);
    if (ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return 0;
    }
    tick = ui->sound.scroll_x + (lx - lo.timeline_x) / lo.px_per_tick;
    if (tick < 0) {
        tick = 0;
    }
    if (out_ch) {
        *out_ch = ch;
    }
    if (out_tick) {
        *out_tick = tick;
    }
    return 1;
}

int sound_region_hit(const UiState *ui, int lx, int ly, int *out_ch, int *out_region, int *out_handle) {
    SoundEditorLayout lo;
    int ch, i;
    int track;
    int vis0, vis1;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    if (!point_in_rect(lx, ly, lo.timeline_x, lo.timeline_y, lo.timeline_w, lo.timeline_h)) {
        return 0;
    }
    ch = (ly - lo.timeline_y) / (lo.lane_h + lo.lane_gap);
    if (ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return 0;
    }
    track = ui->sound.track_idx;
    if (track < 0 || track >= ui->sound.track_count) {
        track = 0;
    }
    vis0 = ui->sound.scroll_x;
    vis1 = vis0 + lo.visible_ticks + 1;
    for (i = 0; i < ui->sound.region_count[track][ch]; i++) {
        const UiBgmRegion *rg = &ui->sound.region[track][ch][i];
        int x0, x1, y0, hw;
        int handle = 1;
        if (rg->start + rg->len <= vis0 || rg->start >= vis1) {
            continue;
        }
        x0 = lo.timeline_x + (rg->start - vis0) * lo.px_per_tick;
        x1 = lo.timeline_x + (rg->start + rg->len - vis0) * lo.px_per_tick;
        y0 = lo.timeline_y + ch * (lo.lane_h + lo.lane_gap);
        if (!point_in_rect(lx, ly, x0, y0, x1 - x0, lo.lane_h)) {
            continue;
        }
        hw = UI_SOUND_HANDLE_W;
        if (hw * 2 >= (x1 - x0)) {
            hw = (x1 - x0) / 3;
            if (hw < 1) {
                hw = 1;
            }
        }
        if (lx < x0 + hw) {
            handle = 2;
        } else if (lx >= x1 - hw) {
            handle = 3;
        }
        if (out_ch) {
            *out_ch = ch;
        }
        if (out_region) {
            *out_region = i;
        }
        if (out_handle) {
            *out_handle = handle;
        }
        return handle;
    }
    return 0;
}

int sound_channel_hit(const UiState *ui, int lx, int ly, int *out_ch) {
    SoundEditorLayout lo;
    int i;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    /* All: ruler band; channels: each lane. */
    for (i = 0; i < UI_SOUND_BGM_CH + 1; i++) {
        int solo = i - 1;
        int y;
        if (solo < 0) {
            y = lo.timeline_y - lo.ruler_h;
            if (point_in_rect(lx, ly, lo.insp_x, y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, lo.ruler_h)) {
                if (out_ch) {
                    *out_ch = UI_SOUND_SOLO_ALL;
                }
                return 1;
            }
        } else {
            y = lo.timeline_y + solo * (lo.lane_h + lo.lane_gap);
            if (point_in_rect(lx, ly, lo.insp_x, y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, lo.lane_h)) {
                if (out_ch) {
                    *out_ch = solo;
                }
                return 1;
            }
        }
    }
    return 0;
}

int sound_play_hit(const UiState *ui, int lx, int ly) {
    SoundEditorLayout lo;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    return point_in_rect(lx, ly, lo.play_x, lo.play_y, lo.play_w, UI_BTN_H);
}

int sound_pause_hit(const UiState *ui, int lx, int ly) {
    SoundEditorLayout lo;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    return point_in_rect(lx, ly, lo.pause_x, lo.pause_y, lo.pause_w, UI_BTN_H);
}

int sound_stop_hit(const UiState *ui, int lx, int ly) {
    SoundEditorLayout lo;
    if (!ui) {
        return 0;
    }
    sound_editor_layout(ui, &lo);
    return point_in_rect(lx, ly, lo.stop_x, lo.stop_y, lo.stop_w, UI_BTN_H);
}

void ui_sound_init(UiState *ui) {
    UiSoundEdit *s;
    if (!ui) {
        return;
    }
    s = &ui->sound;
    memset(s, 0, sizeof(*s));
    s->plane = UI_SOUND_PLANE_BGM;
    s->track_count = 2;
    s->track_idx = 0;
    s->solo_ch = UI_SOUND_SOLO_ALL;
    s->scroll_x = 0;
    s->zoom_h = UI_SOUND_ZOOM_MIN;
    s->sel_kind = UI_SOUND_SEL_NONE;
    s->playing = 0;
    s->paused = 0;
    s->play_pos = -1.f;
    s->play_step_tick = -1;
    snprintf(s->track_name[0], sizeof(s->track_name[0]), "Track 1");
    snprintf(s->track_name[1], sizeof(s->track_name[1]), "Track 2");
}
