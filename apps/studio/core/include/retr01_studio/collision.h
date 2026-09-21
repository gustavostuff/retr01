#ifndef retr01_STUDIO_COLLISION_H
#define retr01_STUDIO_COLLISION_H

#include "retr01_studio/types.h"

/* Hardware attr fields used for paint grouping (bank, pal, flips). */
#define R01_ATTR_HW_MASK 0xFFu
/* Solid patterns key on bank only. Pal and H/V flip do not matter. */
#define R01_ATTR_SOLID_BANK_MASK R01_ATTR_BANK_MASK

static inline uint8_t r01_attr_hw(uint8_t a) {
    return (uint8_t)(a & R01_ATTR_HW_MASK);
}

static inline int r01_attr_hw_match(uint8_t a, uint8_t b) {
    return r01_attr_hw(a) == r01_attr_hw(b);
}

static inline int r01_attr_solid_bank(uint8_t a) {
    return (int)(a & R01_ATTR_SOLID_BANK_MASK);
}

/* Attr byte at world pixel, or -1 if no screen / OOB. */
int r01_world_attr_at(const R01World *w, int wx, int wy, uint8_t *out_attr);

int r01_project_pattern_solid(const R01Project *p, int bank, int tile);
int r01_ctx_pattern_solid(const uint8_t *banks, const uint8_t *tiles, int count, int bank, int tile);
int r01_project_set_pattern_solid(R01Project *p, int bank, int tile, int on);
int r01_project_toggle_pattern_solid(R01Project *p, int bank, int tile);
void r01_project_sync_solids(R01Project *p);
void r01_project_copy_solid_pats(const R01Project *p, uint8_t *count, uint8_t *banks, uint8_t *tiles);
void r01_project_add_custom_logic_solids(R01Project *p, const char *custom_logic_path);

/* Derived overlay: solids[] cache. Collision uses bank+tile vs the pattern list. */
static inline int r01_screen_cell_is_solid(const R01Screen *s, int cell) {
    if (!s || cell < 0 || cell >= R01_TILES_PER_SCREEN) {
        return 0;
    }
    return s->solids[cell] != 0;
}

int r01_world_solid_at(const R01Project *p, const R01World *w, int wx, int wy);
int r01_world_solid_at_list(const R01World *w, int wx, int wy, const uint8_t *banks, const uint8_t *tiles,
                            int count);

/* AABB vs present screens and BG solid tiles (all overlapping 8x8 cells). */
int r01_world_aabb_ok(const R01Project *p, const R01World *w, int px, int py, int bw, int bh);
int r01_world_aabb_ok_list(const R01World *w, int px, int py, int bw, int bh, const uint8_t *banks,
                           const uint8_t *tiles, int count);

/* Player stub AABB (8x8) vs present screens and BG solid tiles. */
int r01_world_player_aabb_ok(const R01Project *p, const R01World *w, int px, int py);

#endif
