#include "retr01_ui/widgets.h"
#include "retr01_ui/font.h"

void ui_button_draw(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover) {
    ui_button_draw_ex(r, x, y, w, text, active, hover, 1);
}

void ui_button_draw_fill(SDL_Renderer *r, int x, int y, int w, const char *text, Uint8 fr, Uint8 fg, Uint8 fb,
                         int hover, int enabled) {
    Uint8 tr = UI_COL_TEXT_R, tg = UI_COL_TEXT_G, tb = UI_COL_TEXT_B;
    if (!enabled) {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        font_draw_centered(r, x, y, w, UI_BTN_H, text, UI_COL_TEXT_DIM_R, UI_COL_TEXT_DIM_G, UI_COL_TEXT_DIM_B);
        return;
    }
    fill_rect(r, x, y, w, UI_BTN_H, fr, fg, fb);
    font_draw_centered(r, x, y, w, UI_BTN_H, text, tr, tg, tb);
    if (hover) {
        hover_overlay(r, x, y, w, UI_BTN_H);
    }
}

void ui_button_draw_ex(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover, int enabled) {
    if (active) {
        ui_button_draw_fill(r, x, y, w, text, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B, hover, enabled);
    } else {
        ui_button_draw_fill(r, x, y, w, text, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B, hover, enabled);
    }
}
