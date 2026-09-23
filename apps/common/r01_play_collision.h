#ifndef R01_PLAY_COLLISION_H
#define R01_PLAY_COLLISION_H

#include "r01_cart_caps.h"

/* Shared AABB vs world-grid bounds + solid tiles from MAP + $8700 pattern list.
 * has_screen is a present BG1 slot (col/row 0..15). A missing BG1 screen blocks
 * motion (ledge / world edge). solid_at is world-space (Studio project or packed cart). */

typedef int (*R01PlayHasScreenFn)(void *ctx, int col, int row);
typedef int (*R01PlaySolidAtFn)(void *ctx, int wx, int wy);

int r01_play_aabb_ok(int px, int py, int bw, int bh, int screen_px_w, int screen_px_h,
                     R01PlayHasScreenFn has_screen, R01PlaySolidAtFn solid_at, void *ctx);

#endif
