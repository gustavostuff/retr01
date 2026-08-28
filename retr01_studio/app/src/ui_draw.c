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

void draw_screen_mode(UiState *ui, SDL_Renderer *r) {
    static const char *const labels[2] = {"Tile selection", "Tile paint"};
    int sx, sy, mx, my0;
    int row;
    if (ui->play.active) {
        return;
    }
    ui_editor_layout(ui, &sx, &sy, &mx, &my0);
    for (row = 0; row < 2; row++) {
        int y = my0 + row * UI_MODE_ROW_H;
        int selected = ui->screen_mode == row;
        int hover = screen_mode_row_hit(ui, ui->mouse_x, ui->mouse_y, row);
        draw_radio_sprite(r, mx, y + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2, selected);
        font_draw_centered(r, ui_mode_label_x(mx), y, label_width(labels[row]), UI_MODE_ROW_H, labels[row], 230,
                           230, 230);
        if (hover) {
            hover_overlay(r, mx, y, ui_mode_panel_w(), UI_MODE_ROW_H);
        }
    }
}
void ui_update_cursor(const UiState *ui) {
    int hand = 0;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    if (ui->pal_edit.open) {
        PalModalLayout lo;
        pal_modal_layout(&lo);
        hand = pal_modal_master_hit(lx, ly, NULL, NULL) || pal_modal_plane_hit(lx, ly, 0, NULL, NULL) ||
               pal_modal_plane_hit(lx, ly, 1, NULL, NULL) ||
               point_in_rect(lx, ly, lo.master_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.master_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->tile_edit.open) {
        TileModalLayout lo;
        tile_modal_layout(&lo);
        hand = point_in_rect(lx, ly, lo.pal_x, lo.pal_y, 4 * UI_PAL_SWATCH, 4 * UI_PAL_SWATCH) ||
               point_in_rect(lx, ly, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, lo.pal_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.pal_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->menu.open) {
        hand = menu_hit(ui, lx, ly, NULL, NULL);
    } else {
        hand = play_button_hit(ui, lx, ly) || accordion_header_hit(ui, lx, ly, NULL) ||
               world_btn_hit(ui, lx, ly, NULL) || world_cell_hit(ui, lx, ly, NULL, NULL) ||
               palette_strip_hit(ui, lx, ly) || palette_row_btn_hit(ui, lx, ly, NULL) ||
               (!ui->play.active && screen_mode_hit(ui, lx, ly, NULL));
    }
    SDL_SetCursor(hand && g_cursor_hand ? g_cursor_hand : g_cursor_arrow);
}

static void draw_worlds_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    R01World *w = r01_project_active_world(ui->project);
    int i, col, row;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    for (i = 0; i < R01_MAX_WORLDS; i++) {
        char num[4];
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        int y = lo->worlds_btns_y;
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
    draw_chess_grid(r, UI_WORLDS_X, lo->worlds_grid_y, R01_GRID_MAX, R01_GRID_MAX, UI_WORLD_CELL);
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
                int y = lo->worlds_grid_y + row * UI_WORLD_CELL;
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
        if (!ui->play.active) {
            int sel = ui->project->active_screen;
            if (sel >= 0 && sel < w->screen_count && w->screens[sel].present) {
                int x = UI_WORLDS_X + w->screens[sel].col * UI_WORLD_CELL;
                int y = lo->worlds_grid_y + w->screens[sel].row * UI_WORLD_CELL;
                draw_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, 255, 255, 255);
            }
        }
    }
}

static void draw_palettes(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = ui->pal_edit.open ? ui->pal_edit.row : (w ? w->default_pal_row : 0);
    int pal, c, i;
    int bg_strip_y = lo->pals_body_y;
    int spr_strip_y = lo->pals_body_y + UI_PAL_SWATCH;
    int row_btns_y = lo->pals_body_y + UI_PAL_SWATCH * 2;
    if (row < 0) {
        row = 0;
    }
    if (row >= R01_PAL_ROWS) {
        row = R01_PAL_ROWS - 1;
    }
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            r01_kit_rgb(ui->project->global_pal_bg[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, UI_WORLDS_X + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, bg_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
            r01_kit_rgb(ui->project->global_pal_spr[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, UI_WORLDS_X + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, spr_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
        }
    }
    for (i = 0; i < R01_PAL_ROWS; i++) {
        char num[4];
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        int on = (i == row);
        int hover = point_in_rect(ui->mouse_x, ui->mouse_y, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H);
        if (on) {
            fill_rect(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        snprintf(num, sizeof(num), "%d", i);
        font_draw_centered(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, num, 240, 240, 240);
        if (hover) {
            hover_overlay(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H);
        }
    }
}

void draw_sidebar(UiState *ui, SDL_Renderer *r) {
    AccordionLayout lo;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    accordion_layout(ui, &lo);
    fill_rect(r, 0, 0, UI_SIDEBAR_W, UI_LOGIC_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);

    draw_accordion_header(r, lo.worlds_hdr_y, "Worlds", lo.worlds_open,
                          point_in_rect(lx, ly, 0, lo.worlds_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.worlds_open) {
        draw_worlds_body(ui, r, &lo);
    }

    draw_accordion_header(r, lo.pals_hdr_y, "Palettes", lo.pals_open,
                          point_in_rect(lx, ly, 0, lo.pals_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.pals_open) {
        draw_palettes(ui, r, &lo);
    }
}

static void draw_menu_panel(SDL_Renderer *r, int x, int y, int w, int count, char items[UI_MENU_MAX][32],
                            uint8_t *item_sub, uint8_t *item_disabled, int mx, int my) {
    int i;
    int text_y_off = (UI_BTN_H - font_line_h()) / 2;
    if (text_y_off < 0) {
        text_y_off = 0;
    }
    for (i = 0; i < count; i++) {
        int iy = y + i * UI_BTN_H;
        int ty = iy + text_y_off;
        int disabled = item_disabled && item_disabled[i];
        int hover = !disabled && point_in_rect(mx, my, x, iy, w, UI_BTN_H);
        fill_rect(r, x, iy, w, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
        font_draw(r, x + UI_UNIT / 2, ty, items[i], disabled ? 100 : 230, disabled ? 100 : 230,
                  disabled ? 105 : 230);
        if (item_sub && item_sub[i]) {
            font_draw(r, x + w - UI_UNIT, ty, ">", disabled ? 80 : 180, disabled ? 80 : 180,
                      disabled ? 85 : 190);
        }
        if (hover) {
            hover_overlay(r, x, iy, w, UI_BTN_H);
        }
    }
}

void draw_menu(UiState *ui, SDL_Renderer *r) {
    if (!ui->menu.open) {
        return;
    }
    draw_menu_panel(r, ui->menu.root_x, ui->menu.root_y, ui->menu.root_w, ui->menu.item_count, ui->menu.items,
                    ui->menu.item_sub, ui->menu.item_disabled, ui->mouse_x, ui->mouse_y);
    if (ui->menu.submenu != UI_MENU_SUB_NONE) {
        int i;
        for (i = 0; i < ui->menu.sub_count; i++) {
            int x = ui->menu.sub_x;
            int y = ui->menu.sub_y + i * UI_BTN_H;
            int hover = point_in_rect(ui->mouse_x, ui->mouse_y, x, y, ui->menu.sub_w, UI_BTN_H);
            fill_rect(r, x, y, ui->menu.sub_w, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
            font_draw_centered(r, x, y, ui->menu.sub_w, UI_BTN_H, ui->menu.sub_items[i], 230, 230, 230);
            if (hover) {
                hover_overlay(r, x, y, ui->menu.sub_w, UI_BTN_H);
            }
        }
    }
}
void ui_draw(UiState *ui, SDL_Renderer *r) {
    if (!ui || !ui->project || !r) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B, 255);
    SDL_RenderClear(r);

    draw_sidebar(ui, r);
    draw_button(r, play_btn_x(ui), play_btn_y(), play_btn_w(ui), ui->play.active ? "Stop" : "Play", 1,
                play_button_hit(ui, ui->mouse_x, ui->mouse_y));

    if (ui->play.active) {
        draw_play_view(ui, r);
    } else {
        draw_screen_editor(ui, r, r01_project_active_screen(ui->project));
        draw_screen_mode(ui, r);
    }

    if (ui->toast_until > SDL_GetTicks() && ui->toast[0]) {
        int tw = label_width(ui->toast);
        int ty = UI_LOGIC_H - UI_BTN_H - UI_UNIT;
        fill_rect(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast_error ? 60 : 30, ui->toast_error ? 24 : 36,
                  ui->toast_error ? 24 : 42);
        font_draw_centered(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast, 240, 240, 240);
    }

    draw_menu(ui, r);
    if (ui->pal_edit.open) {
        draw_pal_modal(ui, r);
    } else if (ui->tile_edit.open) {
        draw_tile_modal(ui, r);
    }
}
