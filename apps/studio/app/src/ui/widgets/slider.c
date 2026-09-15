#include "ui/widgets/widgets.h"
#include "ui/internal.h"

#define UI_SLIDER_TRACK_H 4
#define UI_SLIDER_THUMB 8

void ui_slider_discrete_draw(SDL_Renderer *r, int x, int y, int w, int value, int count) {
    int track_y;
    int thumb_x;
    int span;
    if (!r || w < UI_SLIDER_THUMB || count < 1) {
        return;
    }
    if (value < 0) {
        value = 0;
    }
    if (value >= count) {
        value = count - 1;
    }
    track_y = y + (UI_BTN_H - UI_SLIDER_TRACK_H) / 2;
    fill_rect(r, x, track_y, w, UI_SLIDER_TRACK_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    span = w - UI_SLIDER_THUMB;
    if (count > 1) {
        thumb_x = x + (span * value) / (count - 1);
    } else {
        thumb_x = x;
    }
    fill_rect(r, thumb_x, y + (UI_BTN_H - UI_SLIDER_THUMB) / 2, UI_SLIDER_THUMB, UI_SLIDER_THUMB, UI_COL_ACTIVE_R,
              UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
}

int ui_slider_discrete_hit(int lx, int ly, int x, int y, int w, int count, int *out_value) {
    int rel;
    int span;
    int v;
    if (count < 1 || !point_in_rect(lx, ly, x, y, w, UI_BTN_H)) {
        return 0;
    }
    span = w - UI_SLIDER_THUMB;
    if (span < 1 || count == 1) {
        v = 0;
    } else {
        rel = lx - x - UI_SLIDER_THUMB / 2;
        if (rel < 0) {
            rel = 0;
        }
        if (rel > span) {
            rel = span;
        }
        v = (rel * (count - 1) + span / 2) / span;
        if (v < 0) {
            v = 0;
        }
        if (v >= count) {
            v = count - 1;
        }
    }
    if (out_value) {
        *out_value = v;
    }
    return 1;
}
