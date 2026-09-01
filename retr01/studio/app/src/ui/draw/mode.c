#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

void draw_screen_mode(UiState *ui, SDL_Renderer *r) {
    static const char *const mode_labels[2] = {"Tile selection", "Tile paint"};
    static const char *const layer_labels[2] = {"BG layer", "Sprite layer"};
    int layer_x, mx, my0;
    int row;
    int dim_all = ui->play.active;

    ui_editor_layout(ui, NULL, NULL, &layer_x, &mx, &my0);

    for (row = 0; row < 2; row++) {
        int y = my0 + row * UI_MODE_ROW_H;
        int selected = ui->screen_layer == row;
        int hover = !dim_all && screen_layer_row_hit(ui, ui->mouse_x, ui->mouse_y, row);
        ui_radio_draw(r, layer_x, y + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2, selected && !dim_all);
        font_draw_centered(r, ui_mode_label_x(layer_x), y, label_width(layer_labels[row]), UI_MODE_ROW_H,
                           layer_labels[row], dim_all ? 120 : 230, dim_all ? 120 : 230, dim_all ? 130 : 230);
        if (hover) {
            hover_overlay(r, layer_x, y, ui_layer_panel_w(), UI_MODE_ROW_H);
        }
    }

    for (row = 0; row < 2; row++) {
        int y = my0 + (2 + row) * UI_MODE_ROW_H;
        int selected = ui->screen_mode == row;
        int hover = !dim_all && screen_mode_row_hit(ui, ui->mouse_x, ui->mouse_y, row);
        int dim = dim_all || ui->screen_layer != UI_SCREEN_LAYER_BG;
        ui_radio_draw(r, mx, y + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2, selected && !dim);
        font_draw_centered(r, ui_mode_label_x(mx), y, label_width(mode_labels[row]), UI_MODE_ROW_H, mode_labels[row],
                           dim ? 120 : 230, dim ? 120 : 230, dim ? 130 : 230);
        if (hover && !dim) {
            hover_overlay(r, mx, y, ui_mode_panel_w(), UI_MODE_ROW_H);
        }
    }
}

void draw_ctrl_sidebar(UiState *ui, SDL_Renderer *r) {
    if (!ui || !r) {
        return;
    }
    fill_rect(r, ui_ctrl_x(ui), 0, UI_CTRL_SIDEBAR_W, ui_logic_h(ui), UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
    draw_button(r, play_btn_x(ui), play_btn_y(ui), play_btn_w(ui), ui->play.active ? "Stop" : "Play", 1,
                play_button_hit(ui, ui->mouse_x, ui->mouse_y));
    draw_screen_mode(ui, r);
}

void ui_update_cursor(const UiState *ui) {
    int hand = 0;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    if (ui->pal_edit.open) {
        PalModalLayout lo;
        pal_modal_layout(ui, &lo);
        hand = pal_modal_master_hit(ui, lx, ly, NULL, NULL) || pal_modal_plane_hit(ui, lx, ly, 0, NULL, NULL) ||
               pal_modal_plane_hit(ui, lx, ly, 1, NULL, NULL) ||
               point_in_rect(lx, ly, lo.master_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.master_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->tile_edit.open) {
        TileModalLayout lo;
        tile_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, lo.pal_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.pal_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->sprite_edit.open) {
        SpriteModalLayout lo;
        sprite_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, lo.pal_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.pal_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->metasprite_edit.open) {
        MetaspriteModalLayout lo;
        metasprite_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID) ||
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE) ||
               point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N * (UI_DOT_SIZE + UI_DOT_GAP),
                             UI_DOT_SIZE) ||
               point_in_rect(lx, ly, lo.left_grid_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_grid_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->entity_edit.open) {
        EntityModalLayout lo;
        entity_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.left_list_x, lo.left_list_y, UI_ENTITY_BANK_GRID, lo.left_list_h) ||
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE) ||
               point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N * UI_DOT_SIZE, UI_DOT_SIZE) ||
               point_in_rect(lx, ly, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N * UI_DOT_SIZE, UI_DOT_SIZE) ||
               point_in_rect(lx, ly, lo.guides_x, lo.guides_y, UI_CHECKBOX + UI_UNIT * 12, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_list_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_list_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->menu.open) {
        hand = menu_hit(ui, lx, ly, NULL, NULL);
    } else {
        hand = play_button_hit(ui, lx, ly) || accordion_header_hit(ui, lx, ly, NULL) ||
               world_btn_hit(ui, lx, ly, NULL) || world_sub_hit(ui, lx, ly) ||
               world_cell_hit(ui, lx, ly, NULL, NULL) || palette_strip_hit(ui, lx, ly) ||
               palette_row_btn_hit(ui, lx, ly, NULL) || banks_tab_hit(ui, lx, ly, NULL) || banks_sub_hit(ui, lx, ly) ||
               banks_cell_hit(ui, lx, ly, NULL) || metatiles_add_hit(ui, lx, ly) ||
               metatiles_list_hit(ui, lx, ly, NULL) || metasprites_add_hit(ui, lx, ly) ||
               metasprites_list_hit(ui, lx, ly, NULL) || entities_add_hit(ui, lx, ly) ||
               entities_list_hit(ui, lx, ly, NULL) ||
               (!ui->play.active && (screen_mode_hit(ui, lx, ly, NULL) || screen_layer_hit(ui, lx, ly, NULL) ||
                                     screen_hit(ui, lx, ly, NULL, NULL)));
    }
    SDL_SetCursor(hand && g_cursor_hand ? g_cursor_hand : g_cursor_arrow);
}
