#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void draw_menu_panel(SDL_Renderer *r, int x, int y, int w, int count, char items[UI_MENU_MAX][32],
                            uint8_t *item_sub, uint8_t *item_disabled, int mx, int my) {
    int i;
    int text_y_off = (UI_BTN_H - font_line_h()) / 2;
    int h = count * UI_BTN_H;
    if (text_y_off < 0) {
        text_y_off = 0;
    }
    /* Well fill so menus stay readable over the sidebar panel. */
    fill_rect(r, x, y, w, h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    for (i = 0; i < count; i++) {
        int iy = y + i * UI_BTN_H;
        int ty = iy + text_y_off;
        int disabled = item_disabled && item_disabled[i];
        int hover = !disabled && point_in_rect(mx, my, x, iy, w, UI_BTN_H);
        if (hover) {
            fill_rect(r, x, iy, w, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
        }
        font_draw(r, x + UI_UNIT / 2, ty, items[i], disabled ? 100 : 230, disabled ? 100 : 230,
                  disabled ? 105 : 230);
        if (item_sub && item_sub[i]) {
            font_draw(r, x + w - UI_UNIT, ty, ">", disabled ? 80 : 180, disabled ? 80 : 180,
                      disabled ? 85 : 190);
        }
    }
}

void draw_menu(UiState *ui, SDL_Renderer *r) {
    if (!ui->menu.open) {
        return;
    }
    menu_sync_tile_edit_label(ui);
    draw_menu_panel(r, ui->menu.root_x, ui->menu.root_y, ui->menu.root_w, ui->menu.item_count, ui->menu.items,
                    ui->menu.item_sub, ui->menu.item_disabled, ui->mouse_x, ui->mouse_y);
    if (ui->menu.submenu != UI_MENU_SUB_NONE) {
        int i;
        int sh = ui->menu.sub_count * UI_BTN_H;
        int mx = ui->mouse_x;
        int my = ui->mouse_y;
        fill_rect(r, ui->menu.sub_x, ui->menu.sub_y, ui->menu.sub_w, sh, UI_COL_WELL_R, UI_COL_WELL_G,
                  UI_COL_WELL_B);
        for (i = 0; i < ui->menu.sub_count; i++) {
            int x = ui->menu.sub_x;
            int y = ui->menu.sub_y + i * UI_BTN_H;
            int hover = point_in_rect(mx, my, x, y, ui->menu.sub_w, UI_BTN_H);
            if (hover) {
                fill_rect(r, x, y, ui->menu.sub_w, UI_BTN_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
            }
            if (ui->menu.submenu == UI_MENU_SUB_EXISTING_SPR) {
                const R01World *w = r01_project_active_world_const(ui->project);
                int icon_x = x + UI_UNIT / 2;
                int icon_y = y + (UI_BTN_H - 8) / 2;
                int text_y_off = (UI_BTN_H - font_line_h()) / 2;
                fill_rect(r, icon_x, icon_y, 8, 8, UI_COL_CHESS_A_R, UI_COL_CHESS_A_G, UI_COL_CHESS_A_B);
                if (w && i >= 0 && i < w->sprite_count) {
                    R01EntityPart pt;
                    memset(&pt, 0, sizeof(pt));
                    pt.bank = w->sprites[i].bank;
                    pt.tile_id = w->sprites[i].tile_id;
                    pt.pal = w->sprites[i].pal & 3;
                    ui_compose_draw_part(r, ui->project, w, &pt, icon_x, icon_y, 1, 0, 0, 255);
                }
                if (text_y_off < 0) {
                    text_y_off = 0;
                }
                font_draw(r, icon_x + 8 + UI_UNIT / 2, y + text_y_off, ui->menu.sub_items[i], 230, 230, 230);
            } else {
                font_draw_centered(r, x, y, ui->menu.sub_w, UI_BTN_H, ui->menu.sub_items[i], 230, 230, 230);
            }
        }
    }
}
