#include "retr01_ui/widgets.h"
#include "retr01_ui/font.h"

void ui_button_draw(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover) {
    ui_button_draw_ex(r, x, y, w, text, active, hover, 1);
}

void ui_button_draw_ex(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover, int enabled) {
    Uint8 tr = 240, tg = 240, tb = 240;
    if (!enabled) {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        tr = tg = tb = 110;
        font_draw_centered(r, x, y, w, UI_BTN_H, text, tr, tg, tb);
        return;
    }
    if (active) {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
    } else {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    }
    font_draw_centered(r, x, y, w, UI_BTN_H, text, tr, tg, tb);
    if (hover) {
        hover_overlay(r, x, y, w, UI_BTN_H);
    }
}
