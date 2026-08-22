#ifndef RETR01_STUDIO_PALETTE_H
#define RETR01_STUDIO_PALETTE_H

#include "retr01_studio/types.h"

/* Kit logical 24-bit Color PROM mirror (docs/02). */
void r01_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);

/* Pack / unpack board R3G3B2 for PROM burn helpers. */
uint8_t r01_quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b);
void r01_r3g3b2_to_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b);

void r01_pal_row_init_default(R01PalRow *row, int row_index);
void r01_project_init_default_pals(R01Project *p);

/* Resolve active BG palette rows for a world (world override or globals). */
const R01PalRow *r01_world_bg_pals(const R01Project *p, const R01World *w);
const R01PalRow *r01_world_spr_pals(const R01Project *p, const R01World *w);

/* Screen / sprite tilemap pixel -> kit RGB. */
void r01_tilemap_pixel_rgb(const R01Project *p, const R01World *w, const uint8_t *pixels, const uint8_t *attrs,
                           int px, int py, uint8_t *r, uint8_t *g, uint8_t *b);

void r01_screen_pixel_rgb(const R01Project *p, const R01World *w, const R01Screen *s, int px, int py,
                          uint8_t *r, uint8_t *g, uint8_t *b);

/* Sprite CHR pixel through sprite palette row from OAM attr. Color 0 = transparent (a=0). */
void r01_spr_chr_rgb(const R01Project *p, const R01World *w, int bank, int tile, uint8_t attr, int px, int py,
                     uint8_t *r, uint8_t *g, uint8_t *b, int *opaque);

#endif
