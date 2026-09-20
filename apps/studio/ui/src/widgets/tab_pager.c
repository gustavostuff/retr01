#include "retr01_ui/widgets.h"
#include "retr01_ui/font.h"

#include <stdio.h>
#include <string.h>

void ui_tab_pager_layout(int x, int y, int w, int count, int selected, int plane_w, UiTabPager *out) {
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (count < 0) {
        count = 0;
    }
    if (selected < 0) {
        selected = 0;
    }
    if (count > 0 && selected >= count) {
        selected = count - 1;
    }
    if (w < UI_TAB_PAGER_BTN_W * 2 + UI_TAB_PAGER_LABEL_W) {
        w = UI_TAB_PAGER_BTN_W * 2 + UI_TAB_PAGER_LABEL_W;
    }
    if (plane_w < 0) {
        plane_w = 0;
    }
    out->x = x;
    out->y = y;
    out->w = w;
    out->h = UI_TAB_PAGER_H;
    out->count = count;
    out->selected = selected;
    out->prev_x = x;
    out->prev_w = UI_TAB_PAGER_BTN_W;
    out->label_x = x + UI_TAB_PAGER_BTN_W;
    out->label_w = UI_TAB_PAGER_LABEL_W;
    out->next_x = out->label_x + out->label_w;
    out->next_w = UI_TAB_PAGER_BTN_W;
    out->plane_w = plane_w;
    out->plane_x = plane_w > 0 ? x + w - plane_w : x + w;
}

int ui_tab_pager_content_y(const UiTabPager *lo) {
    if (!lo) {
        return 0;
    }
    return lo->y + lo->h;
}

void ui_tab_pager_draw(SDL_Renderer *r, const UiTabPager *lo, int mouse_x, int mouse_y) {
    char lab[16];
    int n;
    int m;
    int prev_hover;
    int next_hover;
    if (!r || !lo) {
        return;
    }
    n = lo->count < 1 ? 0 : lo->selected + 1;
    m = lo->count < 1 ? 0 : lo->count;
    if (n > 999) {
        n = 999;
    }
    if (m > 999) {
        m = 999;
    }
    snprintf(lab, sizeof(lab), "%d/%d", n, m);
    prev_hover = point_in_rect(mouse_x, mouse_y, lo->prev_x, lo->y, lo->prev_w, lo->h);
    next_hover = point_in_rect(mouse_x, mouse_y, lo->next_x, lo->y, lo->next_w, lo->h);
    ui_button_draw(r, lo->prev_x, lo->y, lo->prev_w, "<", 0, prev_hover);
    fill_rect(r, lo->label_x, lo->y, lo->label_w, lo->h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    font_draw_centered(r, lo->label_x, lo->y, lo->label_w, lo->h, lab, 240, 240, 240);
    ui_button_draw(r, lo->next_x, lo->y, lo->next_w, ">", 0, next_hover);
}

int ui_tab_pager_hit(const UiTabPager *lo, int lx, int ly) {
    if (!lo) {
        return UI_TAB_PAGER_HIT_NONE;
    }
    if (point_in_rect(lx, ly, lo->prev_x, lo->y, lo->prev_w, lo->h)) {
        return UI_TAB_PAGER_HIT_PREV;
    }
    if (point_in_rect(lx, ly, lo->next_x, lo->y, lo->next_w, lo->h)) {
        return UI_TAB_PAGER_HIT_NEXT;
    }
    return UI_TAB_PAGER_HIT_NONE;
}

int ui_tab_pager_step(const UiTabPager *lo, int hit) {
    int sel;
    int n;
    if (!lo || lo->count < 1) {
        return 0;
    }
    sel = lo->selected;
    n = lo->count;
    if (hit == UI_TAB_PAGER_HIT_PREV) {
        sel--;
        if (sel < 0) {
            sel = n - 1;
        }
        return sel;
    }
    if (hit == UI_TAB_PAGER_HIT_NEXT) {
        sel++;
        if (sel >= n) {
            sel = 0;
        }
        return sel;
    }
    return sel;
}
