#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

void draw_screen_mode(UiState *ui, SDL_Renderer *r) {
    static const char *const work_labels[3] = {"BG layer", "Sprite layer", "Both"};
    static const int work_layers[3] = {UI_SCREEN_LAYER_BG, UI_SCREEN_LAYER_SPR, UI_SCREEN_LAYER_BOTH};
    static const char *const hide_labels[2] = {"BG layer", "Sprite layer"};
    int layer_x, mx, my0;
    int work_radio_y0;
    int hide_label_y;
    int hide_check_y0;
    int row;
    int dim_all = ui->play.active;
    Uint8 tr = dim_all ? UI_COL_TEXT_DIM_R : UI_COL_TEXT_R;
    Uint8 tg = dim_all ? UI_COL_TEXT_DIM_G : UI_COL_TEXT_G;
    Uint8 tb = dim_all ? UI_COL_TEXT_DIM_B : UI_COL_TEXT_B;

    ui_editor_layout(ui, NULL, NULL, &layer_x, &mx, &my0);
    work_radio_y0 = my0 + UI_MODE_ROW_H;
    hide_label_y = work_radio_y0 + 3 * UI_MODE_ROW_H + UI_UNIT;
    hide_check_y0 = hide_label_y + UI_MODE_ROW_H;

    font_draw(r, layer_x, my0 + (UI_MODE_ROW_H - 8) / 2, "Work on:", tr, tg, tb);

    for (row = 0; row < 3; row++) {
        int y = work_radio_y0 + row * UI_MODE_ROW_H;
        int selected = ui->screen_layer == work_layers[row];
        int hover = !dim_all && screen_layer_row_hit(ui, ui->mouse_x, ui->mouse_y, row);
        ui_radio_draw(r, layer_x, y + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2, selected && !dim_all);
        font_draw(r, ui_mode_label_x(layer_x), y + (UI_MODE_ROW_H - 8) / 2, work_labels[row], tr, tg, tb);
        if (hover) {
            hover_overlay(r, layer_x, y, ui_layer_panel_w(), UI_MODE_ROW_H);
        }
    }

    font_draw(r, layer_x, hide_label_y + (UI_MODE_ROW_H - 8) / 2, "Hide:", tr, tg, tb);

    for (row = 0; row < 2; row++) {
        int y = hide_check_y0 + row * UI_MODE_ROW_H;
        int checked = row == 0 ? ui->hide_bg_layer : ui->hide_spr_layer;
        int hover = !dim_all && screen_hide_row_hit(ui, ui->mouse_x, ui->mouse_y, row);
        ui_checkbox_draw(r, layer_x, y + (UI_MODE_ROW_H - UI_CHECKBOX) / 2, checked && !dim_all);
        font_draw(r, ui_mode_label_x(layer_x), y + (UI_MODE_ROW_H - 8) / 2, hide_labels[row], tr, tg, tb);
        if (hover) {
            hover_overlay(r, layer_x, y, ui_layer_panel_w(), UI_MODE_ROW_H);
        }
    }
}

void draw_ctrl_sidebar(UiState *ui, SDL_Renderer *r) {
    if (!ui || !r) {
        return;
    }
    fill_rect(r, ui_ctrl_x(ui), UI_APP_CHROME_H, UI_CTRL_SIDEBAR_W, ui_logic_h(ui) - UI_APP_CHROME_H, UI_COL_PANEL_R,
              UI_COL_PANEL_G, UI_COL_PANEL_B);
    draw_screen_mode(ui, r);
}

static void fill_lock(SDL_Renderer *r, int x, int y, int w, int h) {
    if (w < 1 || h < 1) {
        return;
    }
    fill_rect_alpha(r, x, y, w, h, 0, 0, 0, 120);
}

void draw_play_lock_overlay(UiState *ui, SDL_Renderer *r) {
    int cx, cw, sx, sy, sw, sh, bx, by, bw, h, chrome;
    if (!ui || !r || !ui->play.active) {
        return;
    }
    h = ui_logic_h(ui);
    chrome = UI_APP_CHROME_H;
    cx = UI_SIDEBAR_W;
    cw = ui_ctrl_x(ui) - UI_SIDEBAR_W;
    ui_editor_layout(ui, &sx, &sy, NULL, NULL, NULL);
    sw = ui_screen_w(ui);
    sh = ui_screen_h(ui);
    bx = play_btn_x(ui);
    by = play_btn_y(ui);
    bw = play_btn_w(ui);

    fill_lock(r, 0, 0, ui_logic_w(ui), chrome);
    fill_lock(r, 0, chrome, UI_SIDEBAR_W, h - chrome);
    fill_lock(r, ui_ctrl_x(ui), chrome, UI_CTRL_SIDEBAR_W, h - chrome);
    fill_lock(r, cx, chrome, cw, by - chrome);
    fill_lock(r, cx, by, bx - cx, UI_BTN_H);
    fill_lock(r, bx + bw, by, cx + cw - (bx + bw), UI_BTN_H);
    fill_lock(r, cx, by + UI_BTN_H, cw, sy - (by + UI_BTN_H));
    fill_lock(r, cx, sy, sx - cx, sh);
    fill_lock(r, sx + sw, sy, cx + cw - (sx + sw), sh);
    fill_lock(r, cx, sy + sh, cw, h - (sy + sh));
}

void draw_play_button(UiState *ui, SDL_Renderer *r) {
    int hover;
    if (!ui || !r) {
        return;
    }
    hover = play_button_hit(ui, ui->mouse_x, ui->mouse_y);
    if (ui->play.active) {
        ui_button_draw_fill(r, play_btn_x(ui), play_btn_y(ui), play_btn_w(ui), "Stop", UI_COL_DANGER_R, UI_COL_DANGER_G,
                            UI_COL_DANGER_B, hover, 1);
    } else {
        ui_button_draw(r, play_btn_x(ui), play_btn_y(ui), play_btn_w(ui), "Play", 1, hover);
    }
}

void ui_update_cursor(const UiState *ui) {
    int hand = 0;
    int sizewe = 0;
    int sizens = 0;
    int sizenwse = 0;
    int sizenesw = 0;
    int sizese = 0;
    int sizesw = 0;
    int sizeall = 0;
    int no = 0;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    if (ui->pal_edit.open) {
        PalModalLayout lo;
        pal_modal_layout(ui, &lo);
        hand = pal_modal_master_hit(ui, lx, ly, NULL, NULL) || pal_modal_plane_hit(ui, lx, ly, 0, NULL, NULL) ||
               pal_modal_plane_hit(ui, lx, ly, 1, NULL, NULL) ||
               point_in_rect(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_btn_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->tile_edit.open) {
        TileModalLayout lo;
        tile_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_btn_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->sprite_edit.open) {
        SpriteModalLayout lo;
        sprite_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.canvas_x, lo.canvas_y, UI_TILE_CANVAS, UI_TILE_CANVAS) ||
               point_in_rect(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_btn_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->metasprite_edit.open) {
        MetaspriteModalLayout lo;
        metasprite_modal_layout(ui, &lo);
        hand = point_in_rect(lx, ly, lo.left_grid_x, lo.left_grid_y, UI_ENTITY_BANK_GRID, UI_ENTITY_BANK_GRID) ||
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_METASPRITE_COMPOSE, UI_METASPRITE_COMPOSE) ||
               point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               point_in_rect(lx, ly, lo.left_dots_x, lo.left_dots_y, UI_DOT_STRIP_N * (UI_DOT_SIZE + UI_DOT_GAP),
                             UI_DOT_SIZE) ||
               point_in_rect(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_btn_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H);
    } else if (ui->entity_edit.open) {
        EntityModalLayout lo;
        int dots_hit;
        int play_ok;
        int idx;
        entity_modal_layout(ui, &lo);
        dots_hit = ui_dot_strip_hit(lx, ly, lo.state_dots_x, lo.state_dots_y, UI_DOT_STRIP_N, &idx) ||
                   ui_dot_strip_hit(lx, ly, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, &idx);
        play_ok = ui->entity_edit.preview_playing || entity_edit_preview_can_play(ui);
        if (ui->entity_edit.preview_playing && dots_hit) {
            no = 1;
        }
        {
            int hb = entity_edit_guides_cursor(ui, lx, ly);
            if (hb == UI_ENTITY_HB_CUR_NWSE) {
                sizenwse = 1;
            } else if (hb == UI_ENTITY_HB_CUR_NESW) {
                sizenesw = 1;
            } else if (hb == UI_ENTITY_HB_CUR_SE) {
                sizese = 1;
            } else if (hb == UI_ENTITY_HB_CUR_SW) {
                sizesw = 1;
            } else if (hb == UI_ENTITY_HB_CUR_WE) {
                sizewe = 1;
            } else if (hb == UI_ENTITY_HB_CUR_NS) {
                sizens = 1;
            } else if (hb == UI_ENTITY_HB_CUR_MOVE) {
                sizeall = 1;
            }
        }
        hand = (!ui->entity_edit.preview_playing &&
                point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) ||
               point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE) ||
               (!ui->entity_edit.preview_playing && dots_hit) ||
               point_in_rect(lx, ly, lo.guides_x, lo.guides_y, lo.mode_x + lo.mode_w - lo.guides_x, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.mode_x, lo.mode_y, lo.mode_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.add_spr_x, lo.add_spr_y, lo.add_spr_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.rem_spr_x, lo.rem_spr_y, lo.rem_spr_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.highlight_x, lo.highlight_y, lo.highlight_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.brush_x, lo.brush_y, lo.brush_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, UI_BTN_H) ||
               point_in_rect(lx, ly, lo.left_btn_x + lo.save_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H) ||
               (play_ok && point_in_rect(lx, ly, lo.play_x, lo.btn_y, lo.play_w, UI_BTN_H));
    } else if (ui->menu.open) {
        hand = menu_hit(ui, lx, ly, NULL, NULL);
    } else if (ui->play.active) {
        hand = play_button_hit(ui, lx, ly);
    } else if (ui->app_mode == UI_APP_SOUNDS) {
        int handle = 0;
        if (ui->sound.plane == UI_SOUND_PLANE_BGM) {
            int hit = sound_region_hit(ui, lx, ly, NULL, NULL, &handle);
            if (hit == 2 || hit == 3 || ui->sound.drag == UI_SOUND_DRAG_RESIZE_L ||
                ui->sound.drag == UI_SOUND_DRAG_RESIZE_R) {
                sizewe = 1;
            }
        }
        hand = !sizewe && (app_mode_tab_hit(ui, lx, ly, NULL) || sound_plane_tab_hit(ui, lx, ly, NULL) ||
                           sound_track_hit(ui, lx, ly, NULL) || sound_add_hit(ui, lx, ly) ||
                           sound_zoom_out_hit(ui, lx, ly) || sound_zoom_in_hit(ui, lx, ly) ||
                           sound_note_hit(ui, lx, ly) ||
                           sound_play_hit(ui, lx, ly) || sound_pause_hit(ui, lx, ly) || sound_stop_hit(ui, lx, ly) ||
                           sound_channel_hit(ui, lx, ly, NULL) || sound_timeline_hit(ui, lx, ly, NULL, NULL));
    } else if (ui->app_mode == UI_APP_CODE) {
        hand = app_mode_tab_hit(ui, lx, ly, NULL);
    } else {
        hand = app_mode_tab_hit(ui, lx, ly, NULL) || play_button_hit(ui, lx, ly) ||
               accordion_header_hit(ui, lx, ly, NULL) || world_btn_hit(ui, lx, ly, NULL) || world_sub_hit(ui, lx, ly) ||
               world_cell_hit(ui, lx, ly, NULL, NULL) || palette_strip_hit(ui, lx, ly) ||
               palette_row_btn_hit(ui, lx, ly, NULL) || banks_tab_hit(ui, lx, ly, NULL) || banks_sub_hit(ui, lx, ly) ||
               banks_cell_hit(ui, lx, ly, NULL) || global_banks_tab_hit(ui, lx, ly, NULL) ||
               global_banks_sub_hit(ui, lx, ly) || global_banks_cell_hit(ui, lx, ly, NULL) ||
               metatiles_add_hit(ui, lx, ly) ||
               metatiles_list_hit(ui, lx, ly, NULL) || metasprites_add_hit(ui, lx, ly) ||
               metasprites_list_hit(ui, lx, ly, NULL) || entities_add_hit(ui, lx, ly) ||
               entities_import_hit(ui, lx, ly) ||
               entities_list_hit(ui, lx, ly, NULL) ||
               (!ui->play.active && (screen_layer_hit(ui, lx, ly, NULL) || screen_hide_hit(ui, lx, ly, NULL) ||
                                     screen_hit(ui, lx, ly, NULL, NULL)));
    }
    if (sizenwse && g_cursor_sizenwse) {
        SDL_SetCursor(g_cursor_sizenwse);
    } else if (sizenesw && g_cursor_sizenesw) {
        SDL_SetCursor(g_cursor_sizenesw);
    } else if (sizese && g_cursor_sizese) {
        SDL_SetCursor(g_cursor_sizese);
    } else if (sizesw && g_cursor_sizesw) {
        SDL_SetCursor(g_cursor_sizesw);
    } else if (sizens && g_cursor_sizens) {
        SDL_SetCursor(g_cursor_sizens);
    } else if (sizewe && g_cursor_sizewe) {
        SDL_SetCursor(g_cursor_sizewe);
    } else if (sizeall && g_cursor_sizeall) {
        SDL_SetCursor(g_cursor_sizeall);
    } else if (no && g_cursor_no) {
        SDL_SetCursor(g_cursor_no);
    } else {
        SDL_SetCursor(hand && g_cursor_hand ? g_cursor_hand : g_cursor_arrow);
    }
}
