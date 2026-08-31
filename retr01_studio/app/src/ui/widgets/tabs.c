#include "ui/widgets/widgets.h"
#include "ui/internal.h"
#include "font/font.h"

#include <string.h>

#define UI_SUB_BTN_W 16
#define UI_SUB_BTN_H 7

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
    out->dual_view = 0;
    out->view = 0;
    out->sub_rgba[0] = NULL;
    out->sub_rgba[1] = NULL;
    out->sub_w = UI_SUB_BTN_W;
    out->sub_h = UI_SUB_BTN_H;
    for (i = 0; i < count; i++) {
        out->label[i] = labels && labels[i] ? labels[i] : "";
    }
}

void ui_tabs_set_dual(UiTabsLayout *lo, int enabled, int view, const uint8_t *rgba_a, int wa, int ha,
                      const uint8_t *rgba_b, int wb, int hb) {
    if (!lo) {
        return;
    }
    lo->dual_view = enabled ? 1 : 0;
    lo->view = view ? 1 : 0;
    lo->sub_rgba[0] = rgba_a;
    lo->sub_rgba[1] = rgba_b;
    lo->sub_w = (wa > 0 && rgba_a) ? wa : UI_SUB_BTN_W;
    lo->sub_h = (ha > 0 && rgba_a) ? ha : UI_SUB_BTN_H;
    if (rgba_b && wb > 0 && hb > 0) {
        /* Prefer shared 16x7. Keep A dims if B differs. */
        (void)wb;
        (void)hb;
    }
}

int ui_tabs_body_y(const UiTabsLayout *lo) {
    if (!lo) {
        return 0;
    }
    if (lo->dual_view) {
        return lo->y + lo->tab_h + lo->sub_h;
    }
    return lo->y + lo->tab_h;
}

static void blit_rgba(SDL_Renderer *r, const uint8_t *rgba, int rw, int rh, int dx, int dy) {
    int x, y;
    if (!r || !rgba || rw < 1 || rh < 1) {
        return;
    }
    for (y = 0; y < rh; y++) {
        for (x = 0; x < rw; x++) {
            const uint8_t *p = &rgba[(y * rw + x) * 4u];
            if (p[3] > 128) {
                fill_rect(r, dx + x, dy + y, 1, 1, p[0], p[1], p[2]);
            }
        }
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
    if (lo->dual_view && selected >= 0 && selected < lo->count) {
        int sx = lo->x + selected * lo->tab_w + (lo->tab_w - lo->sub_w) / 2;
        int sy = lo->y + lo->tab_h;
        const uint8_t *rgba = lo->sub_rgba[lo->view ? 1 : 0];
        int hover;
        fill_rect(r, lo->x + selected * lo->tab_w, sy, lo->tab_w, lo->sub_h, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G,
                  UI_COL_ACTIVE_B);
        if (rgba) {
            sx = lo->x + selected * lo->tab_w + (lo->tab_w - lo->sub_w) / 2;
            blit_rgba(r, rgba, lo->sub_w, lo->sub_h, sx, sy);
        } else {
            font_draw_centered(r, lo->x + selected * lo->tab_w, sy, lo->tab_w, lo->sub_h,
                               lo->view ? "B" : "A", 240, 240, 240);
        }
        hover = point_in_rect(mouse_x, mouse_y, lo->x + selected * lo->tab_w, sy, lo->tab_w, lo->sub_h);
        if (hover) {
            hover_overlay(r, lo->x + selected * lo->tab_w, sy, lo->tab_w, lo->sub_h);
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

int ui_tabs_sub_hit(const UiTabsLayout *lo, int selected, int lx, int ly) {
    int sx;
    int sy;
    if (!lo || !lo->dual_view || selected < 0 || selected >= lo->count) {
        return 0;
    }
    sx = lo->x + selected * lo->tab_w;
    sy = lo->y + lo->tab_h;
    return point_in_rect(lx, ly, sx, sy, lo->tab_w, lo->sub_h);
}
