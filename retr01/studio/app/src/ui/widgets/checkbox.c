#include "ui/widgets/widgets.h"
#include "ui/internal.h"

void ui_checkbox_draw(SDL_Renderer *r, int dx, int dy, int checked) {
    int x, y;
    int src_y0 = checked ? 8 : 0;
    if (!g_checkbox_rgba || g_checkbox_w != 8 || g_checkbox_h != 16) {
        draw_rect(r, dx, dy, 8, 8, checked ? 240 : 120, checked ? 240 : 120, checked ? 240 : 120);
        return;
    }
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            const uint8_t *p = &g_checkbox_rgba[((src_y0 + y) * g_checkbox_w + x) * 4u];
            if (p[3] > 128) {
                fill_rect(r, dx + x, dy + y, 1, 1, p[0], p[1], p[2]);
            }
        }
    }
}
