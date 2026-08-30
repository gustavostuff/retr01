#include "ui/widgets/widgets.h"
#include "ui/internal.h"
#include "font/font.h"

#include <string.h>

void ui_tabs_layout(const char *const *labels, int count, int x, int y, int tab_w, UiTabsLayout *out) {
    int i;
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (count < 1) {
        count = 0;
    }
    if (count > UI_TABS_MAX) {
        count = UI_TABS_MAX;
    }
    out->count = count;
    out->x = x;
    out->y = y;
    out->tab_w = tab_w > 0 ? tab_w : UI_WORLD_BTN;
    out->tab_h = UI_BTN_H;
    for (i = 0; i < count; i++) {
        out->label[i] = labels && labels[i] ? labels[i] : "";
    }
}

void ui_tabs_draw(SDL_Renderer *r, const UiTabsLayout *lo, int selected, int mouse_x, int mouse_y) {
    int i;
    if (!r || !lo) {
        return;
    }
    for (i = 0; i < lo->count; i++) {
        int tx = lo->x + i * lo->tab_w;
        int ty = lo->y;
        int hover = point_in_rect(mouse_x, mouse_y, tx, ty, lo->tab_w, lo->tab_h);
        int on = (i == selected);
        if (on) {
            fill_rect(r, tx, ty, lo->tab_w, lo->tab_h, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, tx, ty, lo->tab_w, lo->tab_h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        font_draw_centered(r, tx, ty, lo->tab_w, lo->tab_h, lo->label[i], 240, 240, 240);
        if (hover) {
            hover_overlay(r, tx, ty, lo->tab_w, lo->tab_h);
        }
    }
}

int ui_tabs_hit(const UiTabsLayout *lo, int lx, int ly, int *out_idx) {
    int i;
    if (!lo || lx < lo->x || ly < lo->y || ly >= lo->y + lo->tab_h) {
        return 0;
    }
    for (i = 0; i < lo->count; i++) {
        int tx = lo->x + i * lo->tab_w;
        if (lx >= tx && lx < tx + lo->tab_w) {
            if (out_idx) {
                *out_idx = i;
            }
            return 1;
        }
    }
    return 0;
}
