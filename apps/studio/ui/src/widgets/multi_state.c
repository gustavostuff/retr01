#include "retr01_ui/widgets.h"
#include "retr01_ui/font.h"

int ui_multi_state_pref_width(const char *const *labels, int count) {
    int i;
    int w = 0;
    int lw;
    if (!labels || count < 1) {
        return UI_UNIT * 4;
    }
    for (i = 0; i < count && i < UI_MULTI_STATE_MAX; i++) {
        lw = label_width(labels[i] ? labels[i] : "");
        if (lw > w) {
            w = lw;
        }
    }
    return w;
}

void ui_multi_state_draw(SDL_Renderer *r, int x, int y, int w, const char *const *labels, int count, int selected,
                         int mouse_x, int mouse_y) {
    const char *label;
    int hover;
    if (!r || !labels || count < 1 || w < 1) {
        return;
    }
    if (count > UI_MULTI_STATE_MAX) {
        count = UI_MULTI_STATE_MAX;
    }
    if (selected < 0) {
        selected = 0;
    }
    if (selected >= count) {
        selected = count - 1;
    }
    label = labels[selected] ? labels[selected] : "";
    hover = point_in_rect(mouse_x, mouse_y, x, y, w, UI_BTN_H);
    fill_rect(r, x, y, w, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
    font_draw_centered(r, x, y, w, UI_BTN_H, label, 240, 240, 240);
    if (hover) {
        hover_overlay(r, x, y, w, UI_BTN_H);
    }
}

int ui_multi_state_hit(int lx, int ly, int x, int y, int w, int count, int selected, int *out_idx) {
    int next;
    if (count < 1 || w < 1 || !point_in_rect(lx, ly, x, y, w, UI_BTN_H)) {
        return 0;
    }
    if (count > UI_MULTI_STATE_MAX) {
        count = UI_MULTI_STATE_MAX;
    }
    if (selected < 0) {
        selected = 0;
    }
    next = (selected + 1) % count;
    if (out_idx) {
        *out_idx = next;
    }
    return 1;
}
