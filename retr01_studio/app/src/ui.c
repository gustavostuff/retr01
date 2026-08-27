#include "ui.h"
#include "font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Cursor *g_cursor_arrow;
static SDL_Cursor *g_cursor_hand;

static int snap8(int v) {
    if (v < UI_UNIT) {
        return UI_UNIT;
    }
    return (v + UI_UNIT - 1) & ~(UI_UNIT - 1);
}

static void ui_toast(UiState *ui, const char *msg, int is_error) {
    if (!ui || !msg) {
        return;
    }
    snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
    ui->toast_error = is_error;
    ui->toast_until = SDL_GetTicks() + UI_TOAST_MS;
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

static void hover_overlay(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 77);
    SDL_RenderFillRect(r, &rc);
}

static int point_in_rect(int lx, int ly, int x, int y, int w, int h) {
    return lx >= x && ly >= y && lx < x + w && ly < y + h;
}

static int label_width(const char *text) {
    return snap8(font_text_width(text) + UI_UNIT);
}

static void draw_label(SDL_Renderer *r, int x, int y, const char *text) {
    int w = label_width(text);
    font_draw_centered(r, x, y, w, UI_BTN_H, text, 230, 230, 230);
}

static void draw_button(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover) {
    if (active) {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
    } else {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    }
    font_draw_centered(r, x, y, w, UI_BTN_H, text, 240, 240, 240);
    if (hover) {
        hover_overlay(r, x, y, w, UI_BTN_H);
    }
}

static int play_btn_w(const UiState *ui) {
    return label_width(ui->play.active ? "Stop" : "Play");
}

static int play_btn_x(const UiState *ui) {
    return UI_SCREEN_X + (UI_SCREEN_W - play_btn_w(ui)) / 2;
}

static int play_btn_y(void) {
    return UI_SCREEN_Y - UI_BTN_H - UI_UNIT;
}

static int play_button_hit(const UiState *ui, int lx, int ly) {
    int x = play_btn_x(ui);
    int y = play_btn_y();
    int w = play_btn_w(ui);
    return lx >= x && lx < x + w && ly >= y && ly < y + UI_BTN_H;
}

static void screen_origin(int *ox, int *oy) {
    *ox = UI_SCREEN_X;
    *oy = UI_SCREEN_Y;
}

static int screen_hit(int lx, int ly, int *out_tx, int *out_ty) {
    int ox, oy;
    int lx0, ly0;
    screen_origin(&ox, &oy);
    if (lx < ox || ly < oy || lx >= ox + UI_SCREEN_W || ly >= oy + UI_SCREEN_H) {
        return 0;
    }
    lx0 = (lx - ox) / UI_SCREEN_SCALE;
    ly0 = (ly - oy) / UI_SCREEN_SCALE;
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

static int world_cell_hit(int lx, int ly, int *out_col, int *out_row) {
    int x0 = UI_WORLDS_X;
    int y0 = UI_WORLD_VIEW_Y;
    int col, row;
    if (lx < x0 || ly < y0 || lx >= x0 + UI_WORLD_VIEW || ly >= y0 + UI_WORLD_VIEW) {
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

static int world_btn_hit(int lx, int ly, int *out_wi) {
    int i;
    int y = UI_WORLD_BTNS_Y;
    if (ly < y || ly >= y + UI_WORLD_BTN) {
        return 0;
    }
    for (i = 0; i < R01_MAX_WORLDS; i++) {
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        if (lx >= x && lx < x + UI_WORLD_BTN) {
            if (out_wi) {
                *out_wi = i;
            }
            return 1;
        }
    }
    return 0;
}

static void menu_close(UiState *ui) {
    ui->menu.open = 0;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    ui->menu.item_count = 0;
}

static void menu_open_root(UiState *ui, int x, int y, int tx, int ty) {
    ui->menu.open = 1;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    ui->menu.x = x;
    ui->menu.y = y;
    ui->menu.screen_tx = tx;
    ui->menu.screen_ty = ty;
    ui->menu.item_count = 0;
    snprintf(ui->menu.items[ui->menu.item_count++], 24, "Move to tile bank");
    snprintf(ui->menu.items[ui->menu.item_count++], 24, "Edit tile");
    snprintf(ui->menu.items[ui->menu.item_count++], 24, "Set tile palette");
    snprintf(ui->menu.items[ui->menu.item_count++], 24, "Set Anim mode");
    snprintf(ui->menu.items[ui->menu.item_count++], 24, "Set Solid");
}

static void menu_open_bank_sub(UiState *ui) {
    int i;
    ui->menu.submenu = UI_MENU_SUB_BANK;
    ui->menu.item_count = 0;
    for (i = 0; i < 4; i++) {
        snprintf(ui->menu.items[ui->menu.item_count++], 24, "%d", i + 1);
    }
}

static void menu_open_pal_sub(UiState *ui) {
    int i;
    ui->menu.submenu = UI_MENU_SUB_PAL;
    ui->menu.item_count = 0;
    for (i = 0; i < 4; i++) {
        snprintf(ui->menu.items[ui->menu.item_count++], 24, "%d", i + 1);
    }
}

static void menu_open(UiState *ui, int x, int y, int tx, int ty) {
    menu_open_root(ui, x, y, tx, ty);
}

static int menu_item_w(const UiState *ui) {
    int i, w = UI_UNIT * 10;
    for (i = 0; i < ui->menu.item_count; i++) {
        int tw = label_width(ui->menu.items[i]);
        if (tw > w) {
            w = tw;
        }
    }
    return w;
}

static int menu_hit(const UiState *ui, int lx, int ly, int *out_item) {
    int w, h;
    if (!ui->menu.open) {
        return 0;
    }
    w = menu_item_w(ui);
    h = ui->menu.item_count * UI_BTN_H;
    if (lx < ui->menu.x || ly < ui->menu.y || lx >= ui->menu.x + w || ly >= ui->menu.y + h) {
        return 0;
    }
    if (out_item) {
        *out_item = (ly - ui->menu.y) / UI_BTN_H;
    }
    return 1;
}

static void ui_toggle_play(UiState *ui) {
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

static int screen_sel_cell(const UiState *ui) {
    if (!ui || ui->sel_tx < 0 || ui->sel_ty < 0) {
        return -1;
    }
    return ui->sel_ty * R01_SCREEN_TILES_X + ui->sel_tx;
}

static void screen_set_sel_attr(UiState *ui, int bank, int pal, int flip_h, int flip_v) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int cell = screen_sel_cell(ui);
    uint8_t old;
    if (!w || !s || cell < 0) {
        return;
    }
    old = s->attrs[cell];
    s->attrs[cell] = r01_attr_merge(old, bank, pal, flip_h, flip_v);
    screen_refresh_tile(w, s);
}

static void menu_set_tile_bank(UiState *ui, int bank) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int cell;
    uint8_t old;
    if (!w || !s || ui->menu.screen_tx < 0 || ui->menu.screen_ty < 0) {
        return;
    }
    cell = ui->menu.screen_ty * R01_SCREEN_TILES_X + ui->menu.screen_tx;
    old = s->attrs[cell];
    s->attrs[cell] =
        r01_attr_merge(old, bank, r01_attr_pal(old), r01_attr_flip_h(old), r01_attr_flip_v(old));
    screen_refresh_tile(w, s);
}

static void menu_set_tile_pal(UiState *ui, int pal) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int cell;
    uint8_t old;
    if (!w || !s || ui->menu.screen_tx < 0 || ui->menu.screen_ty < 0) {
        return;
    }
    cell = ui->menu.screen_ty * R01_SCREEN_TILES_X + ui->menu.screen_tx;
    old = s->attrs[cell];
    s->attrs[cell] =
        r01_attr_merge(old, r01_attr_bank(old), pal, r01_attr_flip_h(old), r01_attr_flip_v(old));
    screen_refresh_tile(w, s);
}

static void menu_toggle_tile_flag(UiState *ui, uint8_t flag) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int cell;
    if (!w || !s || ui->menu.screen_tx < 0 || ui->menu.screen_ty < 0) {
        return;
    }
    cell = ui->menu.screen_ty * R01_SCREEN_TILES_X + ui->menu.screen_tx;
    s->attrs[cell] ^= flag;
}

static void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int ox, oy, y, x;
    R01World *w = r01_project_active_world(ui->project);
    screen_origin(&ox, &oy);
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
    if (ui->sel_tx >= 0 && ui->sel_ty >= 0) {
        int sx = ox + ui->sel_tx * 8 * UI_SCREEN_SCALE;
        int sy = oy + ui->sel_ty * 8 * UI_SCREEN_SCALE;
        int sz = 8 * UI_SCREEN_SCALE;
        draw_rect(r, sx, sy, sz, sz, 255, 255, 255);
    }
}

static void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int ox, oy, vy, vx;
    screen_origin(&ox, &oy);
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

static void draw_chess_grid(SDL_Renderer *r, int x0, int y0, int cols, int rows, int cell) {
    int col, row;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            int x = x0 + col * cell;
            int y = y0 + row * cell;
            int light = ((col + row) & 1) != 0;
            if (light) {
                fill_rect(r, x, y, cell, cell, UI_COL_CHESS_A_R, UI_COL_CHESS_A_G, UI_COL_CHESS_A_B);
            } else {
                fill_rect(r, x, y, cell, cell, UI_COL_CHESS_B_R, UI_COL_CHESS_B_G, UI_COL_CHESS_B_B);
            }
        }
    }
}

static void ui_update_cursor(const UiState *ui) {
    int hand = 0;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    if (ui->tile_edit.open) {
        int mx = (UI_LOGIC_W - UI_MODAL_W) / 2;
        int my = (UI_LOGIC_H - UI_MODAL_H) / 2;
        int pal_x = mx + UI_UNIT * 2;
        int pal_y = my + UI_UNIT * 4;
        int canvas_x = mx + UI_MODAL_W - UI_UNIT - UI_TILE_CANVAS;
        int canvas_y = my + UI_UNIT;
        int save_w = label_width("Save");
        int cancel_w = label_width("Cancel");
        int by = my + UI_MODAL_H - UI_BTN_H - UI_UNIT;
        hand = point_in_rect(lx, ly, pal_x, pal_y, 4 * UI_PAL_SWATCH, 4 * UI_PAL_SWATCH) ||
               point_in_rect(lx, ly, canvas_x, canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, pal_x, by, save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, pal_x + save_w + UI_UNIT, by, cancel_w, UI_BTN_H);
    } else if (ui->menu.open) {
        hand = menu_hit(ui, lx, ly, NULL);
    } else {
        hand = play_button_hit(ui, lx, ly) || world_btn_hit(lx, ly, NULL) ||
               world_cell_hit(lx, ly, NULL, NULL);
    }
    SDL_SetCursor(hand && g_cursor_hand ? g_cursor_hand : g_cursor_arrow);
}

static void draw_worlds_panel(UiState *ui, SDL_Renderer *r) {
    R01World *w = r01_project_active_world(ui->project);
    int i, col, row;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    fill_rect(r, 0, 0, UI_SIDEBAR_W, UI_LOGIC_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
    draw_label(r, UI_WORLDS_X, UI_WORLDS_Y, "Worlds");
    for (i = 0; i < R01_MAX_WORLDS; i++) {
        char num[4];
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        int y = UI_WORLD_BTNS_Y;
        int on = (i == ui->project->active_world);
        int hover = point_in_rect(lx, ly, x, y, UI_WORLD_BTN, UI_WORLD_BTN);

        if (on) {
            fill_rect(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        snprintf(num, sizeof(num), "%d", i);
        font_draw(r, x + 1, y, num, 240, 240, 240);
        if (hover) {
            hover_overlay(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN);
        }
    }
    draw_chess_grid(r, UI_WORLDS_X, UI_WORLD_VIEW_Y, R01_GRID_MAX, R01_GRID_MAX, UI_WORLD_CELL);
    if (!w || !w->present) {
        return;
    }
    {
        int mark_idx = ui->play.active ? r01_play_screen_index(&ui->play, w) : w->default_screen;
        if (mark_idx < 0 || mark_idx >= w->screen_count || !w->screens[mark_idx].present) {
            mark_idx = r01_world_default_screen(w);
        }
        for (row = 0; row < R01_GRID_MAX; row++) {
            for (col = 0; col < R01_GRID_MAX; col++) {
                int idx = r01_world_screen_index(w, col, row);
                int x = UI_WORLDS_X + col * UI_WORLD_CELL;
                int y = UI_WORLD_VIEW_Y + row * UI_WORLD_CELL;
                int present = (idx >= 0 && w->screens[idx].present);
                int marked = present && idx == mark_idx;
                int hover = point_in_rect(lx, ly, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                if (present && !marked) {
                    fill_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_PRESENT_R, UI_COL_PRESENT_G,
                              UI_COL_PRESENT_B);
                }
                if (marked) {
                    fill_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_MARK_R, UI_COL_MARK_G, UI_COL_MARK_B);
                }
                if (hover) {
                    hover_overlay(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                }
            }
        }
    }
}

static void draw_palettes(UiState *ui, SDL_Renderer *r) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = w ? w->default_pal_row : 0;
    int pal, c;
    int strip_w = R01_PALS_PER_ROW * R01_PAL_COLORS * UI_PAL_SWATCH;
    int block_h = UI_BTN_H + UI_UNIT + UI_BTN_H;
    int y0 = UI_LOGIC_H - block_h - UI_UNIT;
    int x_label = UI_LOGIC_W - strip_w - UI_PAL_LABEL_W - UI_UNIT;
    int x_strip = x_label + UI_PAL_LABEL_W;
    int bg_row_y = y0;
    int spr_row_y = y0 + UI_BTN_H + UI_UNIT;
    int bg_strip_y = bg_row_y + (UI_BTN_H - UI_PAL_SWATCH) / 2;
    int spr_strip_y = spr_row_y + (UI_BTN_H - UI_PAL_SWATCH) / 2;
    if (row < 0) {
        row = 0;
    }
    if (row >= R01_PAL_ROWS) {
        row = R01_PAL_ROWS - 1;
    }
    font_draw_centered(r, x_label, bg_row_y, UI_PAL_LABEL_W, UI_BTN_H, "BG", 200, 200, 210);
    font_draw_centered(r, x_label, spr_row_y, UI_PAL_LABEL_W, UI_BTN_H, "SPR", 200, 200, 210);
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            r01_kit_rgb(ui->project->global_pal_bg[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, x_strip + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, bg_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
            r01_kit_rgb(ui->project->global_pal_spr[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, x_strip + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, spr_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
        }
    }
}

static void draw_menu(UiState *ui, SDL_Renderer *r) {
    int i, w;
    if (!ui->menu.open) {
        return;
    }
    w = menu_item_w(ui);
    for (i = 0; i < ui->menu.item_count; i++) {
        int y = ui->menu.y + i * UI_BTN_H;
        int hover = point_in_rect(ui->mouse_x, ui->mouse_y, ui->menu.x, y, w, UI_BTN_H);
        fill_rect(r, ui->menu.x, y, w, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
        draw_rect(r, ui->menu.x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        font_draw_centered(r, ui->menu.x, y, w, UI_BTN_H, ui->menu.items[i], 230, 230, 230);
        if (hover) {
            hover_overlay(r, ui->menu.x, y, w, UI_BTN_H);
        }
    }
}

static void tile_edit_open(UiState *ui, int tx, int ty) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s = r01_project_active_screen(ui->project);
    int cell;
    uint8_t attr;
    memset(&ui->tile_edit, 0, sizeof(ui->tile_edit));
    ui->tile_edit.open = 1;
    ui->tile_edit.paint_tx = tx;
    ui->tile_edit.paint_ty = ty;
    ui->tile_edit.bank = 0;
    ui->tile_edit.pal = 0;
    ui->tile_edit.color = 1;
    if (s && w && tx >= 0 && ty >= 0) {
        cell = ty * R01_SCREEN_TILES_X + tx;
        attr = s->attrs[cell];
        ui->tile_edit.tile_id = s->tiles[cell];
        ui->tile_edit.pal = r01_attr_pal(attr);
        ui->tile_edit.bank = r01_attr_bank(attr);
        ui->tile_edit.flip_h = r01_attr_flip_h(attr);
        ui->tile_edit.flip_v = r01_attr_flip_v(attr);
        if (ui->tile_edit.tile_id < w->bg_banks[ui->tile_edit.bank].tile_count) {
            const uint8_t *raw =
                w->bg_banks[ui->tile_edit.bank].chr + (size_t)ui->tile_edit.tile_id * R01_TILE_BYTES;
            r01_tile_orient(raw, ui->tile_edit.flip_h, ui->tile_edit.flip_v, ui->tile_edit.chr);
            ui->tile_edit.is_new = 0;
        } else {
            ui->tile_edit.is_new = 1;
            ui->tile_edit.tile_id = -1;
            memset(ui->tile_edit.chr, 0, sizeof(ui->tile_edit.chr));
        }
    } else {
        ui->tile_edit.is_new = 1;
        ui->tile_edit.tile_id = -1;
    }
}

static void tile_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    R01Screen *s;
    int id;
    int si;
    uint8_t canonical[R01_TILE_BYTES];
    if (!w) {
        return;
    }
    if (ui->tile_edit.is_new || ui->tile_edit.tile_id < 0) {
        id = r01_chr_alloc_tile(w, ui->tile_edit.bank);
        if (id < 0) {
            ui_toast(ui, "CHR bank full", 1);
            return;
        }
        ui->tile_edit.tile_id = id;
        ui->tile_edit.is_new = 0;
    } else {
        id = ui->tile_edit.tile_id;
    }
    r01_tile_orient(ui->tile_edit.chr, ui->tile_edit.flip_h, ui->tile_edit.flip_v, canonical);
    r01_chr_write_tile(w, ui->tile_edit.bank, id, canonical);
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            r01_screen_fill_pixels_from_bank(w, &w->screens[si]);
        }
    }
    s = r01_project_active_screen(ui->project);
    if (s && ui->tile_edit.paint_tx >= 0 && ui->tile_edit.paint_ty >= 0) {
        r01_screen_paint_tile(w, s, ui->tile_edit.paint_tx, ui->tile_edit.paint_ty, (uint8_t)id,
                              r01_attr_pack(ui->tile_edit.bank, ui->tile_edit.pal, ui->tile_edit.flip_h,
                                            ui->tile_edit.flip_v));
    }
    ui->brush.armed = 1;
    ui->brush.bank = ui->tile_edit.bank;
    ui->brush.tile_id = id;
    ui->brush.pal = ui->tile_edit.pal;
    ui->brush.flip_h = ui->tile_edit.flip_h;
    ui->brush.flip_v = ui->tile_edit.flip_v;
    memcpy(ui->brush.chr, ui->tile_edit.chr, R01_TILE_BYTES);
    ui->tile_edit.open = 0;
    ui_toast(ui, "tile saved - Ctrl+click to paint", 0);
}

static int modal_x(void) {
    return (UI_LOGIC_W - UI_MODAL_W) / 2;
}

static int modal_y(void) {
    return (UI_LOGIC_H - UI_MODAL_H) / 2;
}

static void draw_tile_modal(UiState *ui, SDL_Renderer *r) {
    int mx = modal_x();
    int my = modal_y();
    int canvas_x = mx + UI_MODAL_W - UI_UNIT - UI_TILE_CANVAS;
    int canvas_y = my + UI_UNIT;
    int pal_x = mx + UI_UNIT * 2;
    int pal_y = my + UI_UNIT * 4;
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = w ? w->default_pal_row : 0;
    int pal, c, sy, sx;
    int title_w;

    fill_rect(r, 0, 0, UI_LOGIC_W, UI_LOGIC_H, 0, 0, 0);
    {
        /* dim overlay */
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        {
            SDL_Rect full = {0, 0, UI_LOGIC_W, UI_LOGIC_H};
            SDL_RenderFillRect(r, &full);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    }
    fill_rect(r, mx, my, UI_MODAL_W, UI_MODAL_H, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B);
    draw_rect(r, mx, my, UI_MODAL_W, UI_MODAL_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);

    title_w = label_width("Edit tile");
    font_draw_centered(r, mx + (UI_MODAL_W - title_w) / 2, my, title_w, UI_BTN_H, "Edit tile", 240, 240, 240);

    draw_label(r, pal_x, my + UI_UNIT * 2, "Palette/color");
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            int x = pal_x + c * UI_PAL_SWATCH;
            int y = pal_y + pal * UI_PAL_SWATCH;
            r01_kit_rgb(ui->project->global_pal_bg[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, x, y, UI_PAL_SWATCH, UI_PAL_SWATCH, cr, cg, cb);
            if (pal == ui->tile_edit.pal && c == ui->tile_edit.color) {
                draw_rect(r, x, y, UI_PAL_SWATCH, UI_PAL_SWATCH, 240, 240, 240);
            }
        }
    }

    fill_rect(r, canvas_x, canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(ui->tile_edit.chr, sx, sy);
            uint8_t cr, cg, cb;
            int cell = 16;
            r01_kit_rgb(ui->project->global_pal_bg[row][ui->tile_edit.pal].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, canvas_x + sx * cell, canvas_y + sy * cell, cell - 1, cell - 1, cr, cg, cb);
        }
    }

    {
        int save_w = label_width("Save");
        int cancel_w = label_width("Cancel");
        int by = my + UI_MODAL_H - UI_BTN_H - UI_UNIT;
        int save_hover = point_in_rect(ui->mouse_x, ui->mouse_y, pal_x, by, save_w, UI_BTN_H);
        int cancel_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, pal_x + save_w + UI_UNIT, by, cancel_w, UI_BTN_H);
        draw_button(r, pal_x, by, save_w, "Save", 1, save_hover);
        draw_button(r, pal_x + save_w + UI_UNIT, by, cancel_w, "Cancel", 0, cancel_hover);
    }
}

static int tile_modal_handle(UiState *ui, int lx, int ly, int down) {
    int mx = modal_x();
    int my = modal_y();
    int canvas_x = mx + UI_MODAL_W - UI_UNIT - UI_TILE_CANVAS;
    int canvas_y = my + UI_UNIT;
    int pal_x = mx + UI_UNIT * 2;
    int pal_y = my + UI_UNIT * 4;
    int save_w = label_width("Save");
    int cancel_w = label_width("Cancel");
    int by = my + UI_MODAL_H - UI_BTN_H - UI_UNIT;

    if (!down) {
        return 1;
    }
    if (lx >= pal_x && lx < pal_x + 4 * UI_PAL_SWATCH && ly >= pal_y && ly < pal_y + 4 * UI_PAL_SWATCH) {
        ui->tile_edit.color = (lx - pal_x) / UI_PAL_SWATCH;
        ui->tile_edit.pal = (ly - pal_y) / UI_PAL_SWATCH;
        return 1;
    }
    if (lx >= canvas_x && lx < canvas_x + UI_TILE_CANVAS && ly >= canvas_y && ly < canvas_y + UI_TILE_CANVAS) {
        int sx = (lx - canvas_x) / 16;
        int sy = (ly - canvas_y) / 16;
        r01_tile_set_pixel(ui->tile_edit.chr, sx, sy, (uint8_t)ui->tile_edit.color);
        return 1;
    }
    if (lx >= pal_x && lx < pal_x + save_w && ly >= by && ly < by + UI_BTN_H) {
        tile_edit_save(ui);
        return 1;
    }
    if (lx >= pal_x + save_w + UI_UNIT && lx < pal_x + save_w + UI_UNIT + cancel_w && ly >= by &&
        ly < by + UI_BTN_H) {
        ui->tile_edit.open = 0;
        return 1;
    }
    return 1;
}

int ui_init(UiState *ui) {
    if (!ui) {
        return -1;
    }
    memset(ui, 0, sizeof(*ui));
    if (font_init() != 0) {
        return -1;
    }
    g_cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    g_cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    ui->project = (R01Project *)calloc(1, sizeof(R01Project));
    if (!ui->project) {
        return -1;
    }
    snprintf(ui->project_path, sizeof(ui->project_path), "%s", R01_DEFAULT_PROJECT);
    r01_project_init(ui->project, "untitled");
    ui->last_click_col = -1;
    ui->last_click_row = -1;
    ui->sel_tx = -1;
    ui->sel_ty = -1;
    return 0;
}

void ui_shutdown(UiState *ui) {
    if (!ui) {
        return;
    }
    free(ui->project);
    ui->project = NULL;
    if (g_cursor_arrow) {
        SDL_FreeCursor(g_cursor_arrow);
        g_cursor_arrow = NULL;
    }
    if (g_cursor_hand) {
        SDL_FreeCursor(g_cursor_hand);
        g_cursor_hand = NULL;
    }
    font_shutdown();
}

void ui_tick(UiState *ui) {
    int dx = 0, dy = 0;
    if (!ui || !ui->play.active) {
        return;
    }
    {
        Uint32 now = SDL_GetTicks();
        if (now - ui->play_last_tick < 16u) {
            return;
        }
        ui->play_last_tick = now;
    }
    if (ui->keys[SDL_SCANCODE_W] || ui->keys[SDL_SCANCODE_UP]) {
        dy = -1;
    }
    if (ui->keys[SDL_SCANCODE_S] || ui->keys[SDL_SCANCODE_DOWN]) {
        dy = 1;
    }
    if (ui->keys[SDL_SCANCODE_A] || ui->keys[SDL_SCANCODE_LEFT]) {
        dx = -1;
    }
    if (ui->keys[SDL_SCANCODE_D] || ui->keys[SDL_SCANCODE_RIGHT]) {
        dx = 1;
    }
    r01_play_tick(&ui->play, ui->project, dx, dy);
}

void ui_draw(UiState *ui, SDL_Renderer *r) {
    if (!ui || !ui->project || !r) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B, 255);
    SDL_RenderClear(r);

    draw_worlds_panel(ui, r);
    draw_button(r, play_btn_x(ui), play_btn_y(), play_btn_w(ui), ui->play.active ? "Stop" : "Play", 1,
                play_button_hit(ui, ui->mouse_x, ui->mouse_y));

    if (ui->play.active) {
        draw_play_view(ui, r);
    } else {
        draw_screen_editor(ui, r, r01_project_active_screen(ui->project));
    }
    draw_palettes(ui, r);

    if (ui->toast_until > SDL_GetTicks() && ui->toast[0]) {
        int tw = label_width(ui->toast);
        int ty = UI_LOGIC_H - UI_BTN_H - UI_UNIT;
        fill_rect(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast_error ? 60 : 30, ui->toast_error ? 24 : 36,
                  ui->toast_error ? 24 : 42);
        font_draw_centered(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast, 240, 240, 240);
    }

    draw_menu(ui, r);
    if (ui->tile_edit.open) {
        draw_tile_modal(ui, r);
    }
}

static void ui_save(UiState *ui) {
    char err[128];
    if (r01_project_save_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s saved", ui->project_path);
        ui_toast(ui, msg, 0);
    }
}

static void ui_export(UiState *ui) {
    char err[128];
    if (r01_export_bundle(ui->project, R01_DEFAULT_CART_STEM, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    ui_toast(ui, R01_DEFAULT_CART_STEM ".retr01 exported", 0);
}

int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly) {
    char err[128];
    (void)lx;
    (void)ly;
    if (!ui || !path) {
        return 0;
    }
    if (r01_project_import_png(ui->project, path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return 1;
    }
    r01_project_select_start_screen(ui->project);
    ui_toast(ui, "png imported", 0);
    return 1;
}

static void handle_world_click(UiState *ui, int col, int row, int ctrl, int dbl) {
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
        }
        return;
    }
    idx = r01_world_find_screen(w, col, row);
    if (idx >= 0) {
        ui->project->active_screen = idx;
    }
}

static void handle_menu_pick(UiState *ui, int item) {
    if (item < 0 || item >= ui->menu.item_count) {
        menu_close(ui);
        return;
    }
    if (ui->menu.submenu == UI_MENU_SUB_BANK) {
        menu_set_tile_bank(ui, item);
        menu_close(ui);
        return;
    }
    if (ui->menu.submenu == UI_MENU_SUB_PAL) {
        menu_set_tile_pal(ui, item);
        menu_close(ui);
        return;
    }
    switch (item) {
    case 0:
        menu_open_bank_sub(ui);
        return;
    case 1:
        tile_edit_open(ui, ui->menu.screen_tx, ui->menu.screen_ty);
        break;
    case 2:
        menu_open_pal_sub(ui);
        return;
    case 3:
        menu_toggle_tile_flag(ui, R01_ATTR_ANIM);
        break;
    case 4:
        menu_toggle_tile_flag(ui, R01_ATTR_SOLID);
        break;
    default:
        break;
    }
    menu_close(ui);
}

int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly) {
    if (!ui) {
        return 0;
    }
    if (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        ui->mouse_x = lx;
        ui->mouse_y = ly;
    }
    if (e->type == SDL_MOUSEMOTION) {
        ui_update_cursor(ui);
    }
    if (e->type == SDL_KEYDOWN) {
        ui->keys[e->key.keysym.scancode] = 1;
        if (ui->tile_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                ui->tile_edit.open = 0;
                return 1;
            }
            return 1;
        }
        if (e->key.keysym.mod & KMOD_CTRL) {
            if (e->key.keysym.sym == SDLK_s) {
                ui_save(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_e) {
                ui_export(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_o) {
                char err[128];
                if (r01_project_load_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
                    ui_toast(ui, err, 1);
                } else {
                    r01_chr_pack_world_bank0(r01_project_world0(ui->project));
                    ui_toast(ui, "loaded", 0);
                }
                return 1;
            }
            if (e->key.keysym.sym == SDLK_f) {
                return 2;
            }
        }
        if (e->key.keysym.sym == SDLK_ESCAPE && ui->menu.open) {
            if (ui->menu.submenu != UI_MENU_SUB_NONE) {
                menu_open_root(ui, ui->menu.x, ui->menu.y, ui->menu.screen_tx, ui->menu.screen_ty);
            } else {
                menu_close(ui);
            }
            return 1;
        }
        if (!ui->play.active && !ui->menu.open && screen_sel_cell(ui) >= 0) {
            R01World *w = r01_project_active_world(ui->project);
            R01Screen *s = r01_project_active_screen(ui->project);
            int cell = screen_sel_cell(ui);
            uint8_t old;
            if (w && s && cell >= 0) {
                old = s->attrs[cell];
                if (e->key.keysym.sym == SDLK_h) {
                    screen_set_sel_attr(ui, r01_attr_bank(old), r01_attr_pal(old), !r01_attr_flip_h(old),
                                        r01_attr_flip_v(old));
                    return 1;
                }
                if (e->key.keysym.sym == SDLK_v) {
                    screen_set_sel_attr(ui, r01_attr_bank(old), r01_attr_pal(old), r01_attr_flip_h(old),
                                        !r01_attr_flip_v(old));
                    return 1;
                }
                if (e->key.keysym.sym >= SDLK_1 && e->key.keysym.sym <= SDLK_4) {
                    int pal = (int)(e->key.keysym.sym - SDLK_1);
                    screen_set_sel_attr(ui, r01_attr_bank(old), pal, r01_attr_flip_h(old),
                                        r01_attr_flip_v(old));
                    return 1;
                }
            }
        }
        if (e->key.keysym.sym == SDLK_SPACE) {
            ui_toggle_play(ui);
            return 1;
        }
        if (ui->play.active) {
            if (e->key.keysym.sym == SDLK_x) {
                r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_X);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_y) {
                r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_Y);
                return 1;
            }
        }
    }
    if (e->type == SDL_KEYUP) {
        ui->keys[e->key.keysym.scancode] = 0;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;

        if (ui->tile_edit.open) {
            tile_modal_handle(ui, lx, ly, 1);
            return 1;
        }

        if (ui->menu.open) {
            int item;
            if (menu_hit(ui, lx, ly, &item)) {
                handle_menu_pick(ui, item);
            } else {
                menu_close(ui);
            }
            return 1;
        }

        if (e->button.button == SDL_BUTTON_RIGHT && !ui->play.active) {
            int tx, ty;
            if (screen_hit(lx, ly, &tx, &ty) && r01_project_active_screen(ui->project)) {
                ui->sel_tx = tx;
                ui->sel_ty = ty;
                menu_open(ui, lx, ly, tx, ty);
                return 1;
            }
        }

        if (e->button.button == SDL_BUTTON_LEFT) {
            int wi, col, row, tx, ty;

            if (play_button_hit(ui, lx, ly)) {
                ui_toggle_play(ui);
                return 1;
            }
            if (world_btn_hit(lx, ly, &wi)) {
                r01_project_set_active_world(ui->project, wi);
                return 1;
            }
            if (!ui->play.active && world_cell_hit(lx, ly, &col, &row)) {
                Uint32 now = SDL_GetTicks();
                int dbl = (col == ui->last_click_col && row == ui->last_click_row &&
                           now - ui->last_click_ms < 350u);
                handle_world_click(ui, col, row, ctrl, dbl);
                ui->last_click_ms = now;
                ui->last_click_col = col;
                ui->last_click_row = row;
                return 1;
            }
            if (!ui->play.active && screen_hit(lx, ly, &tx, &ty)) {
                R01World *w = r01_project_active_world(ui->project);
                R01Screen *s = r01_project_active_screen(ui->project);
                ui->sel_tx = tx;
                ui->sel_ty = ty;
                if (ctrl && ui->brush.armed && w && s) {
                    r01_screen_paint_tile(w, s, tx, ty, (uint8_t)ui->brush.tile_id,
                                          r01_attr_pack(ui->brush.bank, ui->brush.pal, ui->brush.flip_h,
                                                        ui->brush.flip_v));
                }
                return 1;
            }
        }
    }

    if (e->type == SDL_MOUSEMOTION && ui->tile_edit.open && (e->motion.state & SDL_BUTTON_LMASK)) {
        tile_modal_handle(ui, lx, ly, 1);
        return 1;
    }
    return 0;
}
