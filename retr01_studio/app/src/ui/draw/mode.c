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
    } else if (ui->sprite_edit.open) {
        SpriteModalLayout lo;
        sprite_modal_layout(&lo);
        hand = point_in_rect(lx, ly, lo.pal_x, lo.pal_y, 4 * UI_PAL_SWATCH, 4 * UI_PAL_SWATCH) ||
               point_in_rect(lx, ly, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, lo.pal_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.pal_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->entity_edit.open) {
        EntityModalLayout lo;
        entity_modal_layout(&lo);
        hand = point_in_rect(lx, ly, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID) ||
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE) ||
               point_in_rect(lx, ly, lo.left_grid_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_grid_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->menu.open) {
        hand = menu_hit(ui, lx, ly, NULL, NULL);
    } else {
        hand = play_button_hit(ui, lx, ly) || accordion_header_hit(ui, lx, ly, NULL) ||
               world_btn_hit(ui, lx, ly, NULL) || world_cell_hit(ui, lx, ly, NULL, NULL) ||
               palette_strip_hit(ui, lx, ly) || palette_row_btn_hit(ui, lx, ly, NULL) ||
               sprites_add_hit(ui, lx, ly) || sprites_list_hit(ui, lx, ly, NULL) ||
               entities_add_hit(ui, lx, ly) || entities_list_hit(ui, lx, ly, NULL) ||
               (!ui->play.active && screen_mode_hit(ui, lx, ly, NULL));
    }
    SDL_SetCursor(hand && g_cursor_hand ? g_cursor_hand : g_cursor_arrow);
}
