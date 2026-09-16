#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"
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

int ui_mode_panel_w(void) {
    int label_x = UI_MODE_RADIO + UI_MODE_GAP;
    int w0 = label_x + label_width("BG layer");
    int w1 = label_x + label_width("Sprite layer");
    int w = w0;
    if (w1 > w) {
        w = w1;
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
        *mode_y0 = radios_y;
    }
}

int ui_mode_label_x(int mode_x) {
    return mode_x + UI_MODE_RADIO + UI_MODE_GAP;
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
    int sx, sy, layer_x, mx, my0;
    int y;
    if (!ui || ui->play.active) {
        return 0;
    }
    ui_editor_layout(ui, &sx, &sy, &layer_x, &mx, &my0);
    y = my0 + row * UI_MODE_ROW_H;
    return point_in_rect(lx, ly, layer_x, y, ui_layer_panel_w(), UI_MODE_ROW_H);
}

int screen_layer_hit(const UiState *ui, int lx, int ly, int *out_layer) {
    if (screen_layer_row_hit(ui, lx, ly, 0)) {
        if (out_layer) {
            *out_layer = UI_SCREEN_LAYER_BG;
        }
        return 1;
    }
    if (screen_layer_row_hit(ui, lx, ly, 1)) {
        if (out_layer) {
            *out_layer = UI_SCREEN_LAYER_SPR;
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
    int metatiles_h;
    int metasprites_h;
    int entities_h;

    if (always) {
        worlds_h = UI_WORLDS_BODY_H;
        pals_h = UI_PAL_BODY_H;
        sprites_h = UI_SPRITES_BODY_H;
        metatiles_h = UI_SHOW_METATILES ? UI_METATILES_BODY_H : 0;
        metasprites_h = UI_SHOW_METASPRITES ? UI_METASPRITES_BODY_H : 0;
        entities_h = UI_ENTITIES_BODY_H;
    } else if (ui) {
        worlds_h = ui->accordion_body_h[UI_ACC_WORLDS];
        pals_h = ui->accordion_body_h[UI_ACC_PALS];
        sprites_h = ui->accordion_body_h[UI_ACC_BANKS];
        metatiles_h = UI_SHOW_METATILES ? ui->accordion_body_h[UI_ACC_METATILES] : 0;
        metasprites_h = UI_SHOW_METASPRITES ? ui->accordion_body_h[UI_ACC_METASPRITES] : 0;
        entities_h = ui->accordion_body_h[UI_ACC_ENTITIES];
    } else {
        worlds_h = 0;
        pals_h = 0;
        sprites_h = 0;
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
}

static int accordion_section_full_h(int section) {
    switch (section) {
    case UI_ACC_WORLDS:
        return UI_WORLDS_BODY_H;
    case UI_ACC_PALS:
        return UI_PAL_BODY_H;
    case UI_ACC_BANKS:
        return UI_BANKS_BODY_H;
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
    static const char *const world_labs[R01_MAX_WORLDS] = {"", "", "", "", "", "", "", ""};
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
static int modal_x(const UiState *ui) {
    return (ui_logic_w(ui) - UI_MODAL_W) / 2;
}

static int modal_y(const UiState *ui) {
    return (ui_logic_h(ui) - UI_MODAL_H) / 2;
}

void tile_modal_layout(const UiState *ui, TileModalLayout *lo) {
    lo->mx = modal_x(ui);
    lo->my = modal_y(ui);
    lo->pal_x = lo->mx + UI_UNIT * 2;
    lo->pal_label_y = lo->my + UI_MODAL_BODY_Y;
    lo->pal_y = lo->pal_label_y + UI_BTN_H;
    lo->canvas_x = lo->mx + UI_MODAL_W - UI_UNIT - UI_TILE_CANVAS;
    lo->canvas_y = lo->my + UI_MODAL_BODY_Y;
    lo->btn_y = lo->my + UI_MODAL_H - UI_BTN_H - UI_UNIT;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
}

static int pal_modal_x(const UiState *ui) {
    return (ui_logic_w(ui) - UI_PAL_MODAL_W) / 2;
}

static int pal_modal_y(const UiState *ui) {
    return (ui_logic_h(ui) - UI_PAL_MODAL_H) / 2;
}

void pal_modal_layout(const UiState *ui, PalModalLayout *lo) {
    lo->mx = pal_modal_x(ui);
    lo->my = pal_modal_y(ui);
    lo->master_x = lo->mx + UI_UNIT * 2;
    lo->master_y = lo->my + UI_MODAL_BODY_Y + UI_BTN_H;
    lo->bg_label_y = lo->master_y + UI_MASTER_ROWS * UI_MASTER_CELL + UI_UNIT;
    lo->bg_x = lo->master_x;
    lo->bg_y = lo->bg_label_y + UI_BTN_H;
    lo->spr_label_y = lo->bg_label_y;
    lo->spr_x = lo->bg_x + R01_PALS_PER_ROW * UI_PAL_EDIT_CELL + UI_UNIT * 2;
    lo->spr_y = lo->bg_y;
    lo->btn_y = lo->my + UI_PAL_MODAL_H - UI_BTN_H - UI_UNIT;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
}

void sprite_modal_layout(const UiState *ui, SpriteModalLayout *lo) {
    lo->mx = modal_x(ui);
    lo->my = modal_y(ui);
    lo->pal_x = lo->mx + UI_UNIT * 2;
    lo->pal_label_y = lo->my + UI_MODAL_BODY_Y;
    lo->pal_y = lo->pal_label_y + UI_BTN_H;
    lo->canvas_x = lo->mx + UI_MODAL_W - UI_UNIT - UI_TILE_CANVAS;
    lo->canvas_y = lo->my + UI_MODAL_BODY_Y;
    lo->btn_y = lo->my + UI_MODAL_H - UI_BTN_H - UI_UNIT;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
}

void metasprite_modal_layout(const UiState *ui, MetaspriteModalLayout *lo) {
    int mw = UI_UNIT + UI_ENTITY_BANK_GRID + UI_UNIT + UI_METASPRITE_COMPOSE + UI_UNIT;
    int mh;
    int mx;
    int my;
    int left_x;
    int right_x;
    lo->left_label_y = UI_BTN_H + UI_UNIT;
    lo->left_grid_y = lo->left_label_y + UI_BTN_H;
    lo->right_name_y = lo->left_label_y;
    lo->right_grid_y = lo->right_name_y + UI_BTN_H * 2;
    lo->pal_label_y = lo->right_grid_y + UI_METASPRITE_COMPOSE + UI_UNIT;
    lo->pal_y = lo->pal_label_y + UI_BTN_H;
    lo->btn_y = lo->pal_y + UI_PAL_GRID_SIZE + UI_UNIT;
    mh = lo->btn_y + UI_BTN_H + UI_UNIT;
    mx = (ui_logic_w(ui) - mw) / 2;
    my = (ui_logic_h(ui) - mh) / 2;
    left_x = mx + UI_UNIT;
    right_x = mx + UI_UNIT + UI_ENTITY_BANK_GRID + UI_UNIT;
    lo->mx = mx;
    lo->my = my;
    lo->mw = mw;
    lo->mh = mh;
    lo->left_label_y += my;
    lo->left_dots_x = left_x + label_width("Sprite bank") + UI_UNIT;
    lo->left_dots_y = lo->left_label_y + (UI_BTN_H - UI_DOT_SIZE) / 2;
    lo->left_grid_x = left_x;
    lo->left_grid_y += my;
    lo->right_name_x = right_x + label_width("Name") + UI_UNIT;
    lo->right_name_y += my;
    lo->right_name_w = mx + mw - UI_UNIT - lo->right_name_x;
    if (lo->right_name_w < UI_UNIT * 8) {
        lo->right_name_w = UI_UNIT * 8;
    }
    lo->right_grid_x = right_x;
    lo->right_grid_y += my;
    lo->pal_label_x = right_x;
    lo->pal_label_y += my;
    lo->pal_x = right_x;
    lo->pal_y += my;
    lo->btn_y += my;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
}

void entity_modal_layout(const UiState *ui, EntityModalLayout *lo) {
    /* Single panel: 3 cols (left 80 | gap 8 | right 160). Row heights follow shared
     * vertical boundaries of the left tools column and the right canvas column. */
    enum {
        E_NAME = 1,
        E_STATE_NAME,
        E_FOOTER,
        E_PAL,
        E_STATE,
        E_FRAME,
        E_ADD,
        E_REM,
        E_HIGHLIGHT,
        E_BRUSH_LAB,
        E_BRUSH,
        E_FRAME_ID,
        E_CANVAS,
        E_GUIDES,
        E_SPACER
    };
    /* Body-local row map (after header rows 0..3):
     *  0:16  1:8  2:40  3:8  4:16  5:16  6:8  7:16  8:8  9:16 10:8
     * 11:16 12:8 13:8 14:8 15:8 16:8  => 216px
     * Global body starts at row 4. */
    static const UiPanelCell cells[] = {
        {E_NAME, 0, 0, 3, 1},
        {E_STATE_NAME, 0, 2, 3, 1},
        {E_PAL, 0, 4, 1, 3},       /* 16+8+40 = 64 */
        {E_STATE, 0, 8, 1, 1},     /* global 4+4 */
        {E_FRAME, 0, 9, 1, 1},
        {E_ADD, 0, 11, 1, 1},
        {E_REM, 0, 13, 1, 1},
        {E_HIGHLIGHT, 0, 15, 1, 1},
        {E_BRUSH_LAB, 0, 17, 1, 2}, /* 8+8 = 16 */
        {E_BRUSH, 0, 19, 1, 2},     /* 8+8 = 16 */
        {E_SPACER, 1, 4, 1, 17},    /* full body height */
        {E_FRAME_ID, 2, 4, 1, 1},
        {E_CANVAS, 2, 6, 1, 11},    /* from y24 through y184 */
        {E_GUIDES, 2, 18, 1, 2},    /* y192-208 */
        {E_FOOTER, 0, 22, 3, 1},
    };
    static const int row_hs[] = {
        /* 0-3 header */
        UI_BTN_H, UI_UNIT, UI_BTN_H, UI_UNIT,
        /* 4-20 body (17 rows) */
        16, 8, 40, 8, 16, 16, 8, 16, 8, 16, 8, 16, 8, 8, 8, 8, 8,
        /* 21 gap, 22 footer */
        UI_UNIT, UI_BTN_H,
    };
    UiPanel panel;
    int pad = UI_UNIT;
    int mx, my, mw, mh;
    int content_x, content_y;
    int cx, cy, cw, ch;
    int name_lab;
    int dots_w;
    int spr_btn_w;
    int left_right;
    int i;

    (void)ui;

    ui_panel_init(&panel, 3, 23, UI_PANEL_CELL_MIN, UI_PANEL_CELL_MIN);
    ui_panel_set_cells(&panel, cells, (int)(sizeof(cells) / sizeof(cells[0])));
    ui_panel_set_col_w(&panel, 0, UI_ENTITY_LEFT_W);
    ui_panel_set_col_w(&panel, 1, pad);
    ui_panel_set_col_w(&panel, 2, UI_ENTITY_COMPOSE);
    for (i = 0; i < (int)(sizeof(row_hs) / sizeof(row_hs[0])); i++) {
        ui_panel_set_row_h(&panel, i, row_hs[i]);
    }
    ui_panel_layout(&panel, 0, 0);

    mw = pad + panel.total_w + pad;
    mh = UI_BTN_H + pad + panel.total_h + pad;
    mx = (ui_logic_w(ui) - mw) / 2;
    my = (ui_logic_h(ui) - mh) / 2;
    content_x = mx + pad;
    content_y = my + UI_BTN_H + pad;
    ui_panel_layout(&panel, content_x, content_y);

    lo->mx = mx;
    lo->my = my;
    lo->mw = mw;
    lo->mh = mh;

    lo->left_x = content_x;
    lo->left_w = UI_ENTITY_LEFT_W;
    lo->right_x = content_x + UI_ENTITY_LEFT_W + pad;
    lo->right_w = UI_ENTITY_COMPOSE;
    left_right = lo->left_x + lo->left_w;

    name_lab = label_width("State name");
    if (name_lab < label_width("Name")) {
        name_lab = label_width("Name");
    }
    name_lab = ((name_lab + pad - 1) / pad) * pad + pad;

    ui_panel_cell(&panel, E_NAME, &cx, &cy, &cw, &ch);
    lo->name_y = cy;
    lo->name_x = content_x + name_lab + pad;
    lo->name_w = (content_x + panel.total_w) - lo->name_x;
    lo->name_w = (lo->name_w / pad) * pad;
    if (lo->name_w < UI_UNIT * 8) {
        lo->name_w = UI_UNIT * 8;
    }

    ui_panel_cell(&panel, E_STATE_NAME, &cx, &cy, &cw, &ch);
    lo->state_name_y = cy;
    lo->state_name_x = lo->name_x;
    lo->state_name_w = lo->name_w;

    ui_panel_cell(&panel, E_FRAME_ID, &cx, &cy, &cw, &ch);
    lo->frame_id_x = cx;
    lo->frame_id_y = cy;
    lo->frame_id_w = cw;

    ui_panel_cell(&panel, E_CANVAS, &cx, &cy, &cw, &ch);
    lo->right_grid_x = cx;
    lo->right_grid_y = cy;

    ui_panel_cell(&panel, E_PAL, &cx, &cy, &cw, &ch);
    lo->pal_y = cy;
    lo->pal_x = left_right - UI_PAL_GRID_SIZE;

    dots_w = UI_DOT_STRIP_N * UI_DOT_SIZE + (UI_DOT_STRIP_N > 0 ? (UI_DOT_STRIP_N - 1) * UI_DOT_GAP : 0);
    ui_panel_cell(&panel, E_STATE, &cx, &cy, &cw, &ch);
    lo->state_y = cy;
    lo->state_dots_x = left_right - dots_w;
    lo->state_dots_y = cy + (UI_BTN_H - UI_DOT_SIZE) / 2;

    ui_panel_cell(&panel, E_FRAME, &cx, &cy, &cw, &ch);
    lo->frame_y = cy;
    lo->frame_dots_x = left_right - dots_w;
    lo->frame_dots_y = cy + (UI_BTN_H - UI_DOT_SIZE) / 2;

    spr_btn_w = label_width("Remove");
    if (spr_btn_w < label_width("Add")) {
        spr_btn_w = label_width("Add");
    }
    spr_btn_w = ((spr_btn_w + pad - 1) / pad) * pad;
    if (spr_btn_w > lo->left_w) {
        spr_btn_w = lo->left_w;
    }

    ui_panel_cell(&panel, E_ADD, &cx, &cy, &cw, &ch);
    lo->add_spr_y = cy;
    lo->add_spr_w = spr_btn_w;
    lo->add_spr_x = left_right - spr_btn_w;

    ui_panel_cell(&panel, E_REM, &cx, &cy, &cw, &ch);
    lo->rem_spr_y = cy;
    lo->rem_spr_w = spr_btn_w;
    lo->rem_spr_x = left_right - spr_btn_w;

    ui_panel_cell(&panel, E_HIGHLIGHT, &cx, &cy, &cw, &ch);
    lo->highlight_y = cy;
    lo->highlight_w = UI_CHECKBOX + UI_MODE_GAP + label_width("Highlight");
    lo->highlight_w = ((lo->highlight_w + pad - 1) / pad) * pad;
    if (lo->highlight_w > lo->left_w) {
        lo->highlight_w = lo->left_w;
    }
    lo->highlight_x = left_right - lo->highlight_w;

    ui_panel_cell(&panel, E_BRUSH_LAB, &cx, &cy, &cw, &ch);
    lo->brush_lab_y = cy;
    lo->brush_lab_x = left_right - label_width("Brush");

    ui_panel_cell(&panel, E_BRUSH, &cx, &cy, &cw, &ch);
    lo->brush_y = cy;
    lo->brush_w = lo->left_w;
    lo->brush_x = left_right - lo->brush_w;

    ui_panel_cell(&panel, E_GUIDES, &cx, &cy, &cw, &ch);
    lo->guides_y = cy;
    lo->mode_y = cy;
    lo->guides_x = cx;
    {
        static const char *const mode_labels[] = {"Sprite select", "Sprite paint", "Origin/hitbox"};
        lo->mode_w = ui_multi_state_pref_width(mode_labels, 3);
        lo->mode_w = ((lo->mode_w + pad - 1) / pad) * pad;
        lo->mode_x = cx + cw - lo->mode_w;
    }

    ui_panel_cell(&panel, E_FOOTER, &cx, &cy, &cw, &ch);
    lo->btn_y = cy;
    lo->left_btn_x = content_x;
    lo->save_w = ((label_width("Save") + pad - 1) / pad) * pad;
    lo->cancel_w = ((label_width("Cancel") + pad - 1) / pad) * pad;

#if UI_PANEL_DEBUG_GRID
    lo->dbg_panel = panel;
#endif
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

    lo->lane_label_w = ((label_width("Pulse1") + UI_UNIT + UI_UNIT - 1) / UI_UNIT) * UI_UNIT;
    lo->lane_label_x = UI_SIDEBAR_W + UI_UNIT;
    lo->timeline_x = lo->lane_label_x + lo->lane_label_w;
    lo->hdr_y = chrome + UI_UNIT;
    lo->ruler_h = UI_BTN_H; /* 16 */
    lo->timeline_y = lo->hdr_y + UI_BTN_H + lo->ruler_h;
    lo->lane_h = UI_SOUND_LANE_H;
    lo->lane_gap = UI_SOUND_LANE_GAP;
    lo->px_per_tick = UI_SOUND_PX_PER_TICK;
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
    s->sel_kind = UI_SOUND_SEL_NONE;
    s->playing = 0;
    s->paused = 0;
    s->play_pos = -1.f;
    snprintf(s->track_name[0], sizeof(s->track_name[0]), "Track 1");
    snprintf(s->track_name[1], sizeof(s->track_name[1]), "Track 2");
}
