#include "sprite_fetch.h"

#include <string.h>

static void spr_reset(R01sEntity *e) {
    R01sSpriteFetch *c = (R01sSpriteFetch *)e;
    c->fill_count = 0;
    c->pixel_count = 0;
    c->last_ly = 0;
    c->last_hit_x = 0;
    c->last_hit_color = 0;
    c->sprites_on_line = 0;
}

static void spr_eval(R01sEntity *e) {
    (void)e;
}

static void spr_tick(R01sEntity *e) {
    (void)e;
}

static void spr_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable SPR_VT = {spr_reset, spr_eval, spr_tick, spr_destroy};

void r01s_sprite_fetch_init(R01sSpriteFetch *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &SPR_VT, "SPRITE_FETCH", refdes ? refdes : "UPLDN");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "HB", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "LY0", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 3, "LY1", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 4, "LY2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 5, "LY3", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 6, "LY4", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 7, "LY5", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 8, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 9, "HIT", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 10, "PX0", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 11, "PX1", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 12, "PX2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "PX3", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "VCC", R01S_PIN_PWR);
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_NONE, 0, 0);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sprite_fetch_entity(R01sSpriteFetch *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_sprite_fetch_note_fill(R01sSpriteFetch *chip, uint8_t ly, uint8_t sprites,
                                 uint32_t pixels, uint8_t hit_x, uint8_t hit_color) {
    if (!chip) {
        return;
    }
    chip->fill_count++;
    chip->pixel_count += pixels;
    chip->last_ly = ly;
    chip->sprites_on_line = sprites;
    if (hit_color) {
        chip->last_hit_x = hit_x;
        chip->last_hit_color = hit_color;
    }
}

uint32_t r01s_sprite_fetch_fill_count(const R01sSpriteFetch *chip) {
    return chip ? chip->fill_count : 0;
}

uint32_t r01s_sprite_fetch_pixel_count(const R01sSpriteFetch *chip) {
    return chip ? chip->pixel_count : 0;
}

uint8_t r01s_sprite_fetch_last_ly(const R01sSpriteFetch *chip) {
    return chip ? chip->last_ly : 0;
}

uint8_t r01s_sprite_fetch_last_hit_x(const R01sSpriteFetch *chip) {
    return chip ? chip->last_hit_x : 0;
}

uint8_t r01s_sprite_fetch_last_hit_color(const R01sSpriteFetch *chip) {
    return chip ? chip->last_hit_color : 0;
}
