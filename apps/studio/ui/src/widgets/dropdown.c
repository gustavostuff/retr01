#include "retr01_ui/widgets.h"
#include "retr01_ui/font.h"

static int dropdown_menu_y(int y, int count, int open_up) {
    int h = count * UI_BTN_H;
    if (open_up) {
        return y - h;
    }
    return y + UI_BTN_H;
}

void ui_dropdown_draw(SDL_Renderer *r, int x, int y, int w, const char *const *labels, int count, int selected,
                      int open, int open_up, int mouse_x, int mouse_y) {
    const char *label;
    int hover;
    int ty;
    int i;
    int my;
    int mh;
    if (!r || !labels || count < 1 || w < 1) {
        return;
    }
    if (count > UI_DROPDOWN_MAX) {
        count = UI_DROPDOWN_MAX;
    }
    if (selected < 0) {
        selected = 0;
    }
    if (selected >= count) {
        selected = count - 1;
    }
    label = labels[selected] ? labels[selected] : "";
    hover = point_in_rect(mouse_x, mouse_y, x, y, w, UI_BTN_H);
    if (open) {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
    } else {
        fill_rect(r, x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    }
    ty = y + (UI_BTN_H - 8) / 2;
    font_draw(r, x + 2, ty, label, UI_COL_TEXT_R, UI_COL_TEXT_G, UI_COL_TEXT_B);
    font_draw(r, x + w - UI_UNIT, ty, "v", UI_COL_TEXT_R, UI_COL_TEXT_G, UI_COL_TEXT_B);
    if (hover && !open) {
        hover_overlay(r, x, y, w, UI_BTN_H);
    }
    if (!open) {
        return;
    }
    my = dropdown_menu_y(y, count, open_up);
    mh = count * UI_BTN_H;
    fill_rect(r, x, my, w, mh, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    for (i = 0; i < count; i++) {
        int iy = my + i * UI_BTN_H;
        int item_hover = point_in_rect(mouse_x, mouse_y, x, iy, w, UI_BTN_H);
        const char *item = labels[i] ? labels[i] : "";
        if (i == selected) {
            fill_rect(r, x, iy, w, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        }
        font_draw(r, x + 2, iy + (UI_BTN_H - 8) / 2, item, UI_COL_TEXT_R, UI_COL_TEXT_G, UI_COL_TEXT_B);
        if (item_hover) {
            hover_overlay(r, x, iy, w, UI_BTN_H);
        }
    }
}

int ui_dropdown_hit(int lx, int ly, int x, int y, int w, int count, int open, int open_up, int *out_idx) {
    int my;
    int i;
    if (w < 1 || count < 1) {
        return 0;
    }
    if (count > UI_DROPDOWN_MAX) {
        count = UI_DROPDOWN_MAX;
    }
    if (open) {
        my = dropdown_menu_y(y, count, open_up);
        for (i = 0; i < count; i++) {
            if (point_in_rect(lx, ly, x, my + i * UI_BTN_H, w, UI_BTN_H)) {
                if (out_idx) {
                    *out_idx = i;
                }
                return 2;
            }
        }
    }
    if (point_in_rect(lx, ly, x, y, w, UI_BTN_H)) {
        return 1;
    }
    return 0;
}
