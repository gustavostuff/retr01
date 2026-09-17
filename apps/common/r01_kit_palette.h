#ifndef R01_KIT_PALETTE_H
#define R01_KIT_PALETTE_H

#include <stdint.h>

/* Locked motherboard 64-color kit (AT27C256R indices 0..63).
 * SoT for Studio and Emu preview RGB. PROM burn uses R3G3B2 of these values. */

#define R01_KIT_COLORS 64

/* Preview RGB 0..255 per kit index. Defined in r01_kit_palette.c. */
extern const uint8_t R01_KIT_RGB[R01_KIT_COLORS][3];

void r01_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
