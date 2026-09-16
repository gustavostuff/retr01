#include "retr01_ui/widgets.h"

void ui_radio_draw(SDL_Renderer *r, int dx, int dy, int selected) {
    int x, y;
    int src_y0 = selected ? 8 : 0;
    const R01UiChrome *ch = r01_ui_chrome();
    if (!ch->radio.rgba || ch->radio.w != 8 || ch->radio.h != 16) {
        return;
    }
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            const uint8_t *p = &ch->radio.rgba[((src_y0 + y) * ch->radio.w + x) * 4u];
            if (p[3] > 128) {
                fill_rect(r, dx + x, dy + y, 1, 1, p[0], p[1], p[2]);
            }
        }
    }
}
