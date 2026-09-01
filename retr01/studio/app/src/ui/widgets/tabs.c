#include "ui/widgets/widgets.h"
#include "ui/ui.h"
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
    out->tab_h = UI_TABS_TAB_H;
    out->dual_view = 0;
    out->view = 0;
    out->use_dot = 0;
    out->sub_rgba[0] = NULL;
    out->sub_rgba[1] = NULL;
    out->sub_label[0] = NULL;
    out->sub_label[1] = NULL;
    out->sub_w = UI_TABS_SUB_W;
    out->sub_h = UI_TABS_SUB_H;
    out->sub_iw[0] = out->sub_iw[1] = UI_TABS_SUB_W;
    out->sub_ih[0] = out->sub_ih[1] = UI_TABS_SUB_H;
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
    lo->sub_w = UI_TABS_SUB_W;
    lo->sub_h = UI_TABS_SUB_H;
    lo->sub_iw[0] = (wa > 0 && rgba_a) ? wa : UI_TABS_SUB_W;
    lo->sub_ih[0] = (ha > 0 && rgba_a) ? ha : UI_TABS_SUB_H;
    lo->sub_iw[1] = (wb > 0 && rgba_b) ? wb : lo->sub_iw[0];
    lo->sub_ih[1] = (hb > 0 && rgba_b) ? hb : lo->sub_ih[0];
}

void ui_tabs_set_dot(UiTabsLayout *lo, int enabled) {
    if (!lo) {
        return;
    }
    lo->use_dot = enabled ? 1 : 0;
}

void ui_tabs_set_sub_labels(UiTabsLayout *lo, const char *label_a, const char *label_b) {
    if (!lo) {
        return;
    }
    lo->sub_label[0] = label_a;
    lo->sub_label[1] = label_b;
}

int ui_tabs_body_y(const UiTabsLayout *lo) {
    if (!lo) {
        return 0;
    }
    if (lo->dual_view) {
        return lo->y + UI_WORLDS_TAB_STACK_H;
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

static void draw_tab_dot(SDL_Renderer *r, int tx, int ty, int tw, int th) {
    int dx;
    int dy;
    int px, py;
    if (!g_dot_rgba || g_dot_w < 1 || g_dot_h < 1) {
        fill_rect(r, tx + (tw - 4) / 2, ty + (th - 4) / 2, 4, 4, 240, 240, 240);
        return;
    }
    dx = tx + (tw - g_dot_w) / 2;
    dy = ty + (th - g_dot_h) / 2;
    for (py = 0; py < g_dot_h; py++) {
        for (px = 0; px < g_dot_w; px++) {
            const uint8_t *p = &g_dot_rgba[(py * g_dot_w + px) * 4u];
            if (p[3] > 128) {
                fill_rect(r, dx + px, dy + py, 1, 1, p[0], p[1], p[2]);
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
        int on = (i == selected);
        /* Inactive dual tabs fill the full 16px stack. Active main is 8px (sub is separate). */
        int th = (lo->dual_view && !on) ? UI_WORLDS_TAB_STACK_H : lo->tab_h;
        int hover = point_in_rect(mouse_x, mouse_y, tx, ty, lo->tab_w, th);
        if (on) {
            fill_rect(r, tx, ty, lo->tab_w, th, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, tx, ty, lo->tab_w, th, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        if (lo->use_dot) {
            draw_tab_dot(r, tx, ty, lo->tab_w, th);
        } else if (lo->label[i] && lo->label[i][0]) {
            font_draw_centered(r, tx, ty, lo->tab_w, th, lo->label[i], 240, 240, 240);
        }
        if (hover) {
            hover_overlay(r, tx, ty, lo->tab_w, th);
        }
    }
    if (lo->dual_view && selected >= 0 && selected < lo->count) {
        int sx = lo->x + selected * lo->tab_w;
        int sy = lo->y + lo->tab_h;
        const uint8_t *rgba = lo->sub_rgba[lo->view ? 1 : 0];
        const char *slab = lo->sub_label[lo->view ? 1 : 0];
        int hover;
        fill_rect(r, sx, sy, lo->tab_w, lo->sub_h, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        if (rgba) {
            int v = lo->view ? 1 : 0;
            int iw = lo->sub_iw[v];
            int ih = lo->sub_ih[v];
            int bx = sx + (lo->tab_w - iw) / 2;
            int by = sy + (lo->sub_h - ih) / 2;
            if (by < sy) {
                by = sy;
            }
            blit_rgba(r, rgba, iw, ih, bx, by);
        } else if (slab && slab[0]) {
            font_draw_centered(r, sx, sy, lo->tab_w, lo->sub_h, slab, 240, 240, 240);
        } else {
            font_draw_centered(r, sx, sy, lo->tab_w, lo->sub_h, lo->view ? "B" : "A", 240, 240, 240);
        }
        hover = point_in_rect(mouse_x, mouse_y, sx, sy, lo->tab_w, lo->sub_h);
        if (hover) {
            hover_overlay(r, sx, sy, lo->tab_w, lo->sub_h);
        }
    }
}

int ui_tabs_hit(const UiTabsLayout *lo, int selected, int lx, int ly, int *out_idx) {
    int i;
    int max_h;
    if (!lo) {
        return 0;
    }
    max_h = lo->dual_view ? UI_WORLDS_TAB_STACK_H : lo->tab_h;
    if (lx < lo->x || ly < lo->y || ly >= lo->y + max_h) {
        return 0;
    }
    for (i = 0; i < lo->count; i++) {
        int tx = lo->x + i * lo->tab_w;
        int on = (i == selected);
        if (lx < tx || lx >= tx + lo->tab_w) {
            continue;
        }
        /* Active dual tab: bottom 8px is the sub-button only. Inactive: full 16x16. */
        if (lo->dual_view && on && ly >= lo->y + lo->tab_h) {
            return 0;
        }
        if (out_idx) {
            *out_idx = i;
        }
        return 1;
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
