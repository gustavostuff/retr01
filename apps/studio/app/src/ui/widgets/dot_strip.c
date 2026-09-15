#include "ui/widgets/widgets.h"
#include "ui/internal.h"

void ui_dot_strip_draw(SDL_Renderer *r, int x, int y, int count, int selected, int unlocked_count) {
    int i;
    if (count < 1) {
        count = UI_DOT_STRIP_N;
    }
    for (i = 0; i < count; i++) {
        int dx = x + i * (UI_DOT_SIZE + UI_DOT_GAP);
        int unlocked = (unlocked_count < 0) || (i < unlocked_count);
        int on = (i == selected) && unlocked;
        int px, py;
        if (on) {
            fill_rect(r, dx, y, UI_DOT_SIZE, UI_DOT_SIZE, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else if (unlocked) {
            fill_rect(r, dx, y, UI_DOT_SIZE, UI_DOT_SIZE, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);
        } else {
            fill_rect(r, dx, y, UI_DOT_SIZE, UI_DOT_SIZE, 40, 40, 46);
        }
        if (g_dot_rgba && g_dot_w == UI_DOT_SIZE && g_dot_h == UI_DOT_SIZE) {
            for (py = 0; py < UI_DOT_SIZE; py++) {
                for (px = 0; px < UI_DOT_SIZE; px++) {
                    const uint8_t *p = &g_dot_rgba[(py * g_dot_w + px) * 4u];
                    if (p[3] > 128) {
                        if (unlocked) {
                            fill_rect(r, dx + px, y + py, 1, 1, p[0], p[1], p[2]);
                        } else {
                            fill_rect(r, dx + px, y + py, 1, 1, (Uint8)(p[0] / 2), (Uint8)(p[1] / 2),
                                      (Uint8)(p[2] / 2));
                        }
                    }
                }
            }
        } else {
            fill_rect(r, dx + 2, y + 2, 4, 4, unlocked ? 240 : 100, unlocked ? 240 : 100, unlocked ? 240 : 100);
        }
    }
}

int ui_dot_strip_hit(int lx, int ly, int x, int y, int count, int *out_idx) {
    int i;
    if (count < 1) {
        count = UI_DOT_STRIP_N;
    }
    if (ly < y || ly >= y + UI_DOT_SIZE) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        int dx = x + i * (UI_DOT_SIZE + UI_DOT_GAP);
        if (lx >= dx && lx < dx + UI_DOT_SIZE) {
            if (out_idx) {
                *out_idx = i;
            }
            return 1;
        }
    }
    return 0;
}
