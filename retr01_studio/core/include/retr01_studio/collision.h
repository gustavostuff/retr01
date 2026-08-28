#ifndef retr01_STUDIO_COLLISION_H
#define retr01_STUDIO_COLLISION_H

#include "retr01_studio/types.h"

/* Hardware attr fields used for solid grouping (bank, pal, flips). */
#define R01_ATTR_HW_MASK 0x3Fu

static inline uint8_t r01_attr_hw(uint8_t a) {
    return (uint8_t)(a & R01_ATTR_HW_MASK);
}

static inline int r01_attr_hw_match(uint8_t a, uint8_t b) {
    return r01_attr_hw(a) == r01_attr_hw(b);
}

/* Attr byte at world pixel, or -1 if no screen / OOB. */
int r01_world_attr_at(const R01World *w, int wx, int wy, uint8_t *out_attr);

int r01_world_solid_at(const R01World *w, int wx, int wy);

/* Player AABB (8x8) vs present screens and BG solid tiles. */
int r01_world_player_aabb_ok(const R01World *w, int px, int py);

/*
 * Set or clear R01_ATTR_SOLID on every tile in w whose hardware attrs match hw_key.
 * Returns number of cells touched.
 */
int r01_world_apply_solid_hw(R01World *w, uint8_t hw_key, int set_solid);

#endif
