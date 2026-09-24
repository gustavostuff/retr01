#include "at27c256r.h"

#include "retr01_sim/bus.h"

#include <string.h>

/* docs/graphics kit swatches (same table as retr01_emu / Studio). */
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

void r01s_at27c256r_unpack_rgb(uint8_t packed, uint8_t *r, uint8_t *g, uint8_t *b) {
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
    r01s_bus_hiz(e, "O", 8);
}

static void prom_eval(R01sEntity *e) {
    R01sAt27c256r *c = (R01sAt27c256r *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));

    if (!ce || !oe) {
        r01s_bus_hiz(e, "O", 8);
        return;
    }
    r01s_bus_write(e, "O", 8, c->mem[prom_addr(e)]);
}

static void prom_tick(R01sEntity *e) {
    (void)e;
}

static void prom_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable PROM_VT = {prom_reset, prom_eval, prom_tick, prom_destroy};

void r01s_at27c256r_init(R01sAt27c256r *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &PROM_VT, "AT27C256R", refdes ? refdes : "U24");
    chip->base.impl = chip;

    /* 28-pin PDIP, docs/ic_behavior/AT27C256R.md. Video uses A[5:0] only. */
    r01s_entity_add_pin(&chip->base, 1, "VPP", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "A12", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "A7", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "A6", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "A5", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "A4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "A3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "A2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 9, "A1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "O0", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 12, "O1", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "O2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 15, "O3", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 16, "O4", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 17, "O5", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 18, "O6", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 19, "O7", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 20, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 21, "A10", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 22, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 23, "A11", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 24, "A9", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 25, "A8", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 26, "A13", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 27, "PGM#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 28, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 28);
    r01s_at27c256r_load_kit(chip);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_at27c256r_entity(R01sAt27c256r *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_at27c256r_load_kit(R01sAt27c256r *chip) {
    int i;
    if (!chip) {
        return;
    }
    for (i = 0; i < R01S_COLOR_PROM_ENTRIES; i++) {
        chip->mem[i] = quantize_r3g3b2(KIT_RGB[i][0], KIT_RGB[i][1], KIT_RGB[i][2]);
    }
}

void r01s_at27c256r_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i = master_index & 63;
    if (r) {
        *r = KIT_RGB[i][0];
    }
    if (g) {
        *g = KIT_RGB[i][1];
    }
    if (b) {
        *b = KIT_RGB[i][2];
    }
}

uint8_t r01s_at27c256r_peek(const R01sAt27c256r *chip, int index) {
    if (!chip) {
        return 0;
    }
    return chip->mem[index & 63];
}
