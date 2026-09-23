#include "r01_play_collision.h"

static uint16_t div_nonneg(uint16_t v, uint8_t d) {
    uint16_t q;
    if (d == 0u) {
        return 0;
    }
    if (d == 128u) {
        return (uint16_t)(v >> 7);
    }
    if (d == 8u) {
        return (uint16_t)(v >> 3);
    }
    q = 0;
    while (v >= (uint16_t)d) {
        v = (uint16_t)(v - (uint16_t)d);
        q++;
    }
    return q;
}

int r01_play_aabb_ok(uint16_t px, uint16_t py, uint8_t bw, uint8_t bh, uint8_t screen_px_w, uint8_t screen_px_h,
                     R01PlayHasScreenFn has_screen, R01PlaySolidAtFn solid_at, void *ctx) {
    uint16_t x1;
    uint16_t y1;
    uint16_t c0;
    uint16_t c1;
    uint16_t r0;
    uint16_t r1;
    uint16_t col;
    uint16_t row;
    uint16_t tx0;
    uint16_t ty0;
    uint16_t tx1;
    uint16_t ty1;
    uint16_t tx;
    uint16_t ty;
    const uint8_t tile = 8;
    if (!has_screen || !solid_at || bw < 1u || bh < 1u || screen_px_w < 1u || screen_px_h < 1u) {
        return 0;
    }
    x1 = (uint16_t)(px + (uint16_t)bw - 1u);
    y1 = (uint16_t)(py + (uint16_t)bh - 1u);
    c0 = div_nonneg(px, screen_px_w);
    c1 = div_nonneg(x1, screen_px_w);
    r0 = div_nonneg(py, screen_px_h);
    r1 = div_nonneg(y1, screen_px_h);
    for (col = c0; col <= c1; col++) {
        for (row = r0; row <= r1; row++) {
            if (col > 15u || row > 15u || !has_screen(ctx, (uint8_t)col, (uint8_t)row)) {
                return 0;
            }
        }
    }
    /* All overlapping BG tiles (not just AABB corners). */
    tx0 = (uint16_t)(px >> 3);
    ty0 = (uint16_t)(py >> 3);
    tx1 = (uint16_t)(x1 >> 3);
    ty1 = (uint16_t)(y1 >> 3);
    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            uint16_t wx = (uint16_t)(tx * (uint16_t)tile);
            uint16_t wy = (uint16_t)(ty * (uint16_t)tile);
            if (wx < px) {
                wx = px;
            }
            if (wy < py) {
                wy = py;
            }
            if (wx > x1) {
                wx = x1;
            }
            if (wy > y1) {
                wy = y1;
            }
            if (solid_at(ctx, wx, wy)) {
                return 0;
            }
        }
    }
    return 1;
}
