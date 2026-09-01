#include "ui/widgets/widgets.h"
#include "ui/internal.h"
#include "font/font.h"

void ui_modal_scrim(SDL_Renderer *r, const UiState *ui) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    {
        SDL_Rect full = {0, 0, ui_logic_w(ui), ui_logic_h(ui)};
        SDL_RenderFillRect(r, &full);
    }
}

void ui_modal_panel(SDL_Renderer *r, int mx, int my, int w, int h, const char *title) {
    fill_rect(r, mx, my, w, h, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B);
    draw_rect(r, mx, my, w, h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (title) {
        font_draw_centered(r, mx, my, w, UI_BTN_H, title, 240, 240, 240);
    }
}

void ui_modal_save_cancel(SDL_Renderer *r, int x, int y, int save_w, int cancel_w, int mouse_x, int mouse_y) {
    int save_hover = point_in_rect(mouse_x, mouse_y, x, y, save_w, UI_BTN_H);
    int cancel_hover = point_in_rect(mouse_x, mouse_y, x + save_w + UI_UNIT, y, cancel_w, UI_BTN_H);
    ui_button_draw(r, x, y, save_w, "Save", 1, save_hover);
    ui_button_draw(r, x + save_w + UI_UNIT, y, cancel_w, "Cancel", 0, cancel_hover);
}

int ui_modal_save_hit(int lx, int ly, int x, int y, int save_w) {
    return point_in_rect(lx, ly, x, y, save_w, UI_BTN_H);
}

int ui_modal_cancel_hit(int lx, int ly, int x, int y, int save_w, int cancel_w) {
    return point_in_rect(lx, ly, x + save_w + UI_UNIT, y, cancel_w, UI_BTN_H);
}

int ui_modal_overlay_hit(int lx, int ly, int mx, int my, int w, int h) {
    return !point_in_rect(lx, ly, mx, my, w, h);
}
