#include "ui/widgets/widgets.h"
#include "ui/internal.h"
#include "font/font.h"

void ui_button_draw(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover) {
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
