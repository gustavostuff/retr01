#include "r01_play_collision.h"

int r01_play_aabb_ok(int px, int py, int bw, int bh, int screen_px_w, int screen_px_h,
                     R01PlayHasScreenFn has_screen, R01PlaySolidAtFn solid_at, void *ctx) {
    int x1, y1, c0, c1, r0, r1, col, row;
    int tx0, ty0, tx1, ty1, tx, ty;
    const int tile = 8;
    if (!has_screen || !solid_at || px < 0 || py < 0 || bw < 1 || bh < 1 || screen_px_w < 1 ||
        screen_px_h < 1) {
        return 0;
    }
    x1 = px + bw - 1;
    y1 = py + bh - 1;
    c0 = px / screen_px_w;
    c1 = x1 / screen_px_w;
    r0 = py / screen_px_h;
    r1 = y1 / screen_px_h;
    for (col = c0; col <= c1; col++) {
        for (row = r0; row <= r1; row++) {
            if (!has_screen(ctx, col, row)) {
                return 0;
            }
        }
    }
    /* All overlapping BG tiles (not just AABB corners). */
    tx0 = px / tile;
    ty0 = py / tile;
    tx1 = x1 / tile;
    ty1 = y1 / tile;
    for (ty = ty0; ty <= ty1; ty++) {
        for (tx = tx0; tx <= tx1; tx++) {
            int wx = tx * tile;
            int wy = ty * tile;
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
