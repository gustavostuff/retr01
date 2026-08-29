#include "compositor.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void comp_drive_out(R01sCompositor *c) {
    R01sEntity *e = &c->base;
    int i;
    char name[4];
    for (i = 0; i < 6; i++) {
        snprintf(name, sizeof(name), "A%d", i);
        r01s_entity_drive(e, name, (c->out_index & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void comp_recompute(R01sCompositor *c) {
    if (c->spr_enable && (c->spr_index & 63u) != 0) {
        c->out_index = (uint8_t)(c->spr_index & 63u);
    } else {
        c->out_index = (uint8_t)(c->bg_index & 63u);
    }
    comp_drive_out(c);
}

static void comp_reset(R01sEntity *e) {
    R01sCompositor *c = (R01sCompositor *)e;
    c->bg_index = 0;
    c->spr_index = 0;
    c->spr_enable = 0;
    c->out_index = 0;
    comp_drive_out(c);
}

static void comp_eval(R01sEntity *e) {
    comp_recompute((R01sCompositor *)e);
}

static void comp_tick(R01sEntity *e) {
    (void)e;
}

static void comp_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable COMP_VT = {comp_reset, comp_eval, comp_tick, comp_destroy};

void r01s_compositor_init(R01sCompositor *chip, const char *refdes) {
    static const char *const BG_NAMES[6] = {"BG0", "BG1", "BG2", "BG3", "BG4", "BG5"};
    static const char *const SPR_NAMES[6] = {"SP0", "SP1", "SP2", "SP3", "SP4", "SP5"};
    static const char *const A_NAMES[6] = {"A0", "A1", "A2", "A3", "A4", "A5"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &COMP_VT, "ATF22V10", refdes ? refdes : "UPLDV");
    chip->base.impl = chip;
    for (i = 0; i < 6; i++) {
        r01s_entity_add_pin(&chip->base, 1 + i, BG_NAMES[i], R01S_PIN_IN);
    }
    for (i = 0; i < 6; i++) {
        r01s_entity_add_pin(&chip->base, 7 + i, SPR_NAMES[i], R01S_PIN_IN);
    }
    r01s_entity_add_pin(&chip->base, 13, "SPEN", R01S_PIN_IN);
    for (i = 0; i < 6; i++) {
        r01s_entity_add_pin(&chip->base, 14 + i, A_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 20);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_compositor_entity(R01sCompositor *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_compositor_set_bg(R01sCompositor *chip, uint8_t index) {
    if (!chip) {
        return;
    }
    chip->bg_index = (uint8_t)(index & 63u);
    comp_recompute(chip);
}

void r01s_compositor_set_sprite(R01sCompositor *chip, uint8_t index, int enable) {
    if (!chip) {
        return;
    }
    chip->spr_index = (uint8_t)(index & 63u);
    chip->spr_enable = enable ? 1 : 0;
    comp_recompute(chip);
}

uint8_t r01s_compositor_out(const R01sCompositor *chip) {
    return chip ? chip->out_index : 0;
}
