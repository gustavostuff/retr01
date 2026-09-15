#include "ui/widgets/widgets.h"
#include "ui/internal.h"

void ui_radio_draw(SDL_Renderer *r, int dx, int dy, int selected) {
    int x, y;
    int src_y0 = selected ? 8 : 0;
    if (!g_radio_rgba || g_radio_w != 8 || g_radio_h != 16) {
        return;
    }
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            const uint8_t *p = &g_radio_rgba[((src_y0 + y) * g_radio_w + x) * 4u];
            if (p[3] > 128) {
                fill_rect(r, dx + x, dy + y, 1, 1, p[0], p[1], p[2]);
            }
        }
    }
}
