#include "atf22v10.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void pld_drive_byte(R01sEntity *e, const char *prefix, uint8_t v) {
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, (v & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void pld_drive_named(R01sEntity *e, const char *name, int on) {
    r01s_entity_drive(e, name, on ? R01S_LVL_H : R01S_LVL_L);
}

static void pld_reset(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    c->p_bus = 0;
    c->q_bus = 0;
    c->eq = 0;
    if (c->role == R01S_PLD_BEAM_Y) {
        pld_drive_byte(e, "Y", 0);
        r01s_entity_drive(e, "EQ#", R01S_LVL_H);
        return;
    }
    if (c->role == R01S_PLD_DECODE) {
        pld_drive_named(e, "SEL_FE02", 0);
        pld_drive_named(e, "SEL_FE03", 0);
        pld_drive_named(e, "SEL_FE04", 0);
        pld_drive_named(e, "SEL_FE08", 0);
        pld_drive_named(e, "SEL_FE10", 0);
        pld_drive_named(e, "SEL_FE11", 0);
        pld_drive_named(e, "SEL_FE90", 0);
        pld_drive_named(e, "SEL_FE91", 0);
        pld_drive_named(e, "SEL_FE92", 0);
        pld_drive_named(e, "SEL_FE93", 0);
        pld_drive_named(e, "SEL_FE12", 0);
        return;
    }
    /* VRAM glue: I→Y passthrough until interleave equations land. */
    pld_drive_byte(e, "Y", 0);
}

static uint8_t pld_sense_a_lo(R01sEntity *e) {
    int i;
    uint8_t a = 0;
    char name[4];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "A%d", i);
        if (r01s_level_is_high(r01s_entity_sense(e, name))) {
            a |= (uint8_t)(1u << i);
        }
    }
    return a;
}

static void pld_eval_decode(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    int fe = r01s_level_is_low(r01s_entity_sense(e, "FE#")); /* active-low page hit */
    int be = r01s_level_is_high(r01s_entity_sense(e, "BE"));
    uint8_t off = pld_sense_a_lo(e);
    int hit = fe && be;

    pld_drive_named(e, "SEL_FE02", hit && off == 0x02u);
    pld_drive_named(e, "SEL_FE03", hit && off == 0x03u);
    pld_drive_named(e, "SEL_FE04", hit && off == 0x04u);
    pld_drive_named(e, "SEL_FE08", hit && off == 0x08u);
    pld_drive_named(e, "SEL_FE10", hit && off == 0x10u);
    pld_drive_named(e, "SEL_FE11", hit && off == 0x11u);
    pld_drive_named(e, "SEL_FE12", hit && off == 0x12u);
    pld_drive_named(e, "SEL_FE90", hit && off == 0x90u);
    pld_drive_named(e, "SEL_FE91", hit && off == 0x91u);
    pld_drive_named(e, "SEL_FE92", hit && off == 0x92u);
    pld_drive_named(e, "SEL_FE93", hit && off == 0x93u);
    c->p_bus = off;
    c->q_bus = hit ? off : 0;
}

static void pld_eval(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    int i;
    uint8_t p = 0;
    uint8_t q = 0;
    char pn[8], qn[8];

    if (c->role == R01S_PLD_DECODE) {
        pld_eval_decode(e);
        return;
    }

    if (c->role == R01S_PLD_BEAM_Y) {
        for (i = 0; i < 8; i++) {
            snprintf(pn, sizeof(pn), "P%d", i);
            snprintf(qn, sizeof(qn), "Q%d", i);
            if (r01s_level_is_high(r01s_entity_sense(e, pn))) {
                p |= (uint8_t)(1u << i);
            }
            if (r01s_level_is_high(r01s_entity_sense(e, qn))) {
                q |= (uint8_t)(1u << i);
            }
        }
        c->p_bus = p;
        c->q_bus = q;
        c->eq = (p == q);
        r01s_entity_drive(e, "EQ#", c->eq ? R01S_LVL_L : R01S_LVL_H);
        return;
    }

    /* VRAM glue: visible passthrough stub for bench bring-up. */
    for (i = 0; i < 8; i++) {
        char in[8];
        snprintf(in, sizeof(in), "I%d", i);
        if (r01s_level_is_high(r01s_entity_sense(e, in))) {
            p |= (uint8_t)(1u << i);
        }
    }
    c->p_bus = p;
    c->q_bus = p;
    pld_drive_byte(e, "Y", c->q_bus);
}

static void pld_tick(R01sEntity *e) {
    (void)e;
}

static void pld_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable ATF22_VT = {pld_reset, pld_eval, pld_tick, pld_destroy};

void r01s_atf22v10_init(R01sAtf22v10 *chip, const char *refdes, int role) {
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    chip->role = role;
    r01s_entity_init(&chip->base, &ATF22_VT, "ATF22V10", refdes ? refdes : "UPLD");
    chip->base.impl = chip;

    if (role == R01S_PLD_DECODE) {
        static const char *const A_NAMES[8] = {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7"};
        for (i = 0; i < 8; i++) {
            r01s_entity_add_pin(&chip->base, 1 + i, A_NAMES[i], R01S_PIN_IN);
        }
        r01s_entity_add_pin(&chip->base, 9, "FE#", R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, 10, "BE", R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, 11, "RWB", R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, 12, "SEL_FE02", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 13, "SEL_FE03", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 14, "SEL_FE04", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 15, "SEL_FE08", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 16, "SEL_FE10", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 17, "SEL_FE11", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 18, "SEL_FE12", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 19, "SEL_FE90", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 20, "SEL_FE91", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 21, "SEL_FE92", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 22, "SEL_FE93", R01S_PIN_OUT);
        r01s_entity_add_pin(&chip->base, 23, "VCC", R01S_PIN_PWR);
        r01s_entity_add_pin(&chip->base, 24, "GND", R01S_PIN_PWR);
        r01s_entity_set_dip_mm(&chip->base, 24, 32, 8);
        r01s_entity_reset(&chip->base);
        return;
    }

    for (i = 0; i < 8; i++) {
        char in[8], out[8];
        snprintf(in, sizeof(in), "I%d", i);
        snprintf(out, sizeof(out), "Y%d", i);
        r01s_entity_add_pin(&chip->base, i + 1, in, R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, i + 9, out, R01S_PIN_OUT);
    }
    if (role == R01S_PLD_BEAM_Y) {
        for (i = 0; i < 8; i++) {
            char pn[8], qn[8];
            snprintf(pn, sizeof(pn), "P%d", i);
            snprintf(qn, sizeof(qn), "Q%d", i);
            r01s_entity_add_pin(&chip->base, 17 + i, pn, R01S_PIN_IN);
            r01s_entity_add_pin(&chip->base, 25 + i, qn, R01S_PIN_IN);
        }
        r01s_entity_add_pin(&chip->base, 33, "OE#", R01S_PIN_IN);
        r01s_entity_add_pin(&chip->base, 34, "EQ#", R01S_PIN_OUT);
        /* Package outline is 24-pin; P0-P7 occupy 17-24. Extra compare pins are logical-only. */
        r01s_entity_add_pin(&chip->base, 35, "VCC", R01S_PIN_PWR);
        r01s_entity_set_dip_mm(&chip->base, 24, 32, 8);
    } else {
        /* VRAM glue (UPLDB): fill PDIP-24 so both rows are even (was 12 vs 5). */
        r01s_entity_add_pin(&chip->base, 17, "NC17", R01S_PIN_NC);
        r01s_entity_add_pin(&chip->base, 18, "NC18", R01S_PIN_NC);
        r01s_entity_add_pin(&chip->base, 19, "NC19", R01S_PIN_NC);
        r01s_entity_add_pin(&chip->base, 20, "NC20", R01S_PIN_NC);
        r01s_entity_add_pin(&chip->base, 21, "NC21", R01S_PIN_NC);
        r01s_entity_add_pin(&chip->base, 22, "NC22", R01S_PIN_NC);
        r01s_entity_add_pin(&chip->base, 23, "GND", R01S_PIN_PWR);
        r01s_entity_add_pin(&chip->base, 24, "VCC", R01S_PIN_PWR);
        r01s_entity_set_dip_mm(&chip->base, 24, 32, 8);
    }
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_atf22v10_entity(R01sAtf22v10 *chip) {
    return chip ? &chip->base : NULL;
}

int r01s_atf22v10_eq(const R01sAtf22v10 *chip) {
    return chip && chip->eq;
}
