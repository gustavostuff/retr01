#ifndef RETR01_STUDIO_PALETTE_H
#define RETR01_STUDIO_PALETTE_H

#include "retr01_studio/types.h"

/* Phase 1 player fill: sprite row 0, pal 0, color index 1 (not a hardcoded master). */
#define R01_PLAYER_SPR_ROW 0
#define R01_PLAYER_SPR_PAL 0
#define R01_PLAYER_SPR_COLOR 1
#define R01_ACTIVE_PAL_SPR_BASE 16
#define R01_ACTIVE_PAL_PLAYER (R01_ACTIVE_PAL_SPR_BASE + R01_PLAYER_SPR_COLOR)

/* Kit bright red used when initializing phase 1 sprite palettes. */
#define R01_KIT_RED_MASTER 34

void r01_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);
uint8_t r01_project_player_master(const R01Project *p);
void r01_project_player_rgb(const R01Project *p, uint8_t *r, uint8_t *g, uint8_t *b);
int r01_kit_nearest_master(uint8_t r, uint8_t g, uint8_t b);
uint8_t r01_quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b);

void r01_project_init_phase1_pals(R01Project *p);
void r01_project_set_bg_pals_from_png(R01Project *p, const uint8_t master_for_index[4]);
/* Shared backdrop = BG palette color 0 for the world's default_pal_row. */
void r01_project_backdrop_rgb(const R01Project *p, const R01World *w, uint8_t *r, uint8_t *g, uint8_t *b);
void r01_screen_pixel_rgb(const R01Project *p, const R01World *w, const R01Screen *s, int px, int py, uint8_t *r,
                          uint8_t *g, uint8_t *b);

#endif
