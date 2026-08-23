#include "at28c16.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

/* docs/02 kit swatches (same table as retr01_emu / Studio). */
static const uint8_t KIT_RGB[64][3] = {
    {0x00, 0x00, 0x00}, {0x29, 0x05, 0x14}, {0x2A, 0x05, 0x07}, {0x23, 0x0F, 0x06},
    {0x1E, 0x13, 0x06}, {0x1A, 0x16, 0x05}, {0x14, 0x18, 0x07}, {0x06, 0x1A, 0x07},
    {0x05, 0x1A, 0x13}, {0x07, 0x19, 0x18}, {0x08, 0x18, 0x1C}, {0x07, 0x17, 0x22},
    {0x03, 0x0B, 0x3D}, {0x16, 0x03, 0x3A}, {0x20, 0x05, 0x2D}, {0x26, 0x04, 0x20},
    {0x36, 0x36, 0x36}, {0x74, 0x0A, 0x40}, {0x77, 0x09, 0x1A}, {0x69, 0x35, 0x12},
    {0x5D, 0x3F, 0x0E}, {0x51, 0x46, 0x17}, {0x42, 0x4C, 0x19}, {0x13, 0x51, 0x1A},
    {0x16, 0x50, 0x3F}, {0x11, 0x4E, 0x4D}, {0x16, 0x4D, 0x58}, {0x16, 0x4A, 0x66},
    {0x16, 0x37, 0x94}, {0x47, 0x29, 0x90}, {0x5F, 0x16, 0x7D}, {0x6C, 0x11, 0x5F},
    {0x94, 0x94, 0x94}, {0xC0, 0x4A, 0x7A}, {0xC5, 0x4A, 0x4D}, {0xB8, 0x60, 0x1B},
    {0xA2, 0x73, 0x26}, {0x8F, 0x7E, 0x2F}, {0x77, 0x87, 0x2D}, {0x20, 0x90, 0x30},
    {0x2E, 0x8E, 0x72}, {0x31, 0x8B, 0x89}, {0x1F, 0x88, 0x9C}, {0x24, 0x83, 0xB5},
    {0x4D, 0x77, 0xD7}, {0x7E, 0x6A, 0xD3}, {0x9D, 0x5D, 0xBF}, {0xB3, 0x52, 0xA0},
    {0xFF, 0xFF, 0xFF}, {0xF1, 0xA2, 0xBB}, {0xF1, 0xA6, 0xA1}, {0xF1, 0xA9, 0x83},
    {0xEE, 0xAC, 0x44}, {0xD4, 0xBA, 0x33}, {0xB0, 0xC8, 0x41}, {0x73, 0xD2, 0x75},
    {0x22, 0xD0, 0xA6}, {0x3B, 0xCD, 0xC9}, {0x48, 0xC9, 0xE4}, {0x88, 0xC4, 0xED},
    {0xA4, 0xBD, 0xEF}, {0xBB, 0xB5, 0xF1}, {0xD5, 0xA9, 0xEF}, {0xF0, 0x9B, 0xDD},
};

static uint8_t quantize_r3g3b2(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t rr = (uint8_t)((r * 7 + 127) / 255);
    uint8_t gg = (uint8_t)((g * 7 + 127) / 255);
    uint8_t bb = (uint8_t)((b * 3 + 127) / 255);
    return (uint8_t)((rr << 5) | (gg << 2) | bb);
}

void r01s_at28c16_unpack_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t rr = (uint8_t)((packed >> 5) & 7u);
    uint8_t gg = (uint8_t)((packed >> 2) & 7u);
    uint8_t bb = (uint8_t)(packed & 3u);
    if (r) {
        *r = (uint8_t)((rr * 255) / 7);
    }
    if (g) {
        *g = (uint8_t)((gg * 255) / 7);
    }
    if (b) {
        *b = (uint8_t)((bb * 255) / 3);
    }
}

static int prom_addr(R01sEntity *e) {
    static const char *const names[6] = {"A0", "A1", "A2", "A3", "A4", "A5"};
    int addr = 0;
    int i;
    for (i = 0; i < 6; i++) {
        if (r01s_level_is_high(r01s_entity_sense(e, names[i]))) {
            addr |= (1 << i);
        }
    }
    return addr & 63;
}

static void prom_reset(R01sEntity *e) {
    r01s_bus_hiz(e, "IO", 8);
}

static void prom_eval(R01sEntity *e) {
    R01sAt28c16 *c = (R01sAt28c16 *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int we = r01s_level_is_high(r01s_entity_sense(e, "WE#"));

    if (!ce || !oe || !we) {
        r01s_bus_hiz(e, "IO", 8);
        return;
    }
    r01s_bus_write(e, "IO", 8, c->mem[prom_addr(e)]);
}

static void prom_tick(R01sEntity *e) {
    (void)e;
}

static void prom_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable PROM_VT = {prom_reset, prom_eval, prom_tick, prom_destroy};

void r01s_at28c16_init(R01sAt28c16 *chip, const char *refdes) {
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &PROM_VT, "AT28C16", refdes ? refdes : "U?");
    chip->base.impl = chip;

    for (i = 0; i < 6; i++) {
        char name[4];
        snprintf(name, sizeof(name), "A%d", i);
        r01s_entity_add_pin(&chip->base, 1 + i, name, R01S_PIN_IN);
    }
    for (i = 0; i < 8; i++) {
        char name[4];
        snprintf(name, sizeof(name), "IO%d", i);
        r01s_entity_add_pin(&chip->base, 7 + i, name, R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 15, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 16, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 17, "WE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 18, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 19, "VSS", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 24, 48);
    r01s_at28c16_load_kit(chip);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_at28c16_entity(R01sAt28c16 *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_at28c16_load_kit(R01sAt28c16 *chip) {
    int i;
    if (!chip) {
        return;
    }
    for (i = 0; i < R01S_COLOR_PROM_ENTRIES; i++) {
        chip->mem[i] = quantize_r3g3b2(KIT_RGB[i][0], KIT_RGB[i][1], KIT_RGB[i][2]);
    }
}

uint8_t r01s_at28c16_peek(const R01sAt28c16 *chip, int index) {
    if (!chip) {
        return 0;
    }
    return chip->mem[index & 63];
}
