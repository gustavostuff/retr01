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
