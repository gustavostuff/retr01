#include "retr01_ui/widgets.h"

void ui_checkbox_draw(SDL_Renderer *r, int dx, int dy, int checked) {
    int x, y;
    int src_y0 = checked ? 8 : 0;
    const R01UiChrome *ch = r01_ui_chrome();
    if (!ch->checkbox.rgba || ch->checkbox.w != 8 || ch->checkbox.h != 16) {
        return;
    }
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            const uint8_t *p = &ch->checkbox.rgba[((src_y0 + y) * ch->checkbox.w + x) * 4u];
            if (p[3] > 128) {
                fill_rect(r, dx + x, dy + y, 1, 1, p[0], p[1], p[2]);
            }
        }
    }
}
