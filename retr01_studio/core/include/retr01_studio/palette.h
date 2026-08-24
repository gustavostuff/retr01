#ifndef RETR01_STUDIO_PALETTE_H
#define RETR01_STUDIO_PALETTE_H

#include "retr01_studio/types.h"

void r01_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);
uint8_t r01_quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b);

void r01_project_init_phase1_pals(R01Project *p);
void r01_screen_pixel_rgb(const R01Project *p, const R01Screen *s, int px, int py, uint8_t *r, uint8_t *g,
                          uint8_t *b);

#endif
