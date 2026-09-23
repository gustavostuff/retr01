#ifndef R01_PLAY_COLLISION_H
#define R01_PLAY_COLLISION_H

#include "r01_cart_caps.h"

#include <stdint.h>

/* Shared AABB vs world-grid bounds + solid tiles from MAP + $8700 pattern list.
 * has_screen is a present BG1 slot (col/row 0..15). A missing BG1 screen blocks
 * motion (ledge / world edge). solid_at is world-space (Studio project or packed cart). */

typedef int (*R01PlayHasScreenFn)(void *ctx, uint8_t col, uint8_t row);
typedef int (*R01PlaySolidAtFn)(void *ctx, uint16_t wx, uint16_t wy);

int r01_play_aabb_ok(uint16_t px, uint16_t py, uint8_t bw, uint8_t bh, uint8_t screen_px_w, uint8_t screen_px_h,
                     R01PlayHasScreenFn has_screen, R01PlaySolidAtFn solid_at, void *ctx);

#endif
