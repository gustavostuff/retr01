#ifndef RETR01_SIM_SPRITE_FETCH_H
#define RETR01_SIM_SPRITE_FETCH_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Island N — sprite pipeline stub (needs L + M + J).
 * Board owns OAM→linebuf fill + compositor; this chip holds fill stats for UI/health.
 * CHR is stubbed as solid master-index = tile&0x3F until cart CHR is granted.
 */
typedef struct R01sSpriteFetch {
    R01sEntity base;
    uint32_t fill_count;
    uint32_t pixel_count;
    uint8_t last_ly;
    uint8_t last_hit_x;
    uint8_t last_hit_color;
    uint8_t sprites_on_line;
} R01sSpriteFetch;

void r01s_sprite_fetch_init(R01sSpriteFetch *chip, const char *refdes);
R01sEntity *r01s_sprite_fetch_entity(R01sSpriteFetch *chip);

void r01s_sprite_fetch_note_fill(R01sSpriteFetch *chip, uint8_t ly, uint8_t sprites,
                                 uint32_t pixels, uint8_t hit_x, uint8_t hit_color);

uint32_t r01s_sprite_fetch_fill_count(const R01sSpriteFetch *chip);
uint32_t r01s_sprite_fetch_pixel_count(const R01sSpriteFetch *chip);
uint8_t r01s_sprite_fetch_last_ly(const R01sSpriteFetch *chip);
uint8_t r01s_sprite_fetch_last_hit_x(const R01sSpriteFetch *chip);
uint8_t r01s_sprite_fetch_last_hit_color(const R01sSpriteFetch *chip);

#endif
