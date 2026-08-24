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

static void pld_reset(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    c->p_bus = 0;
    c->q_bus = 0;
    c->eq = 0;
    pld_drive_byte(e, "Y", 0);
    if (c->role == R01S_PLD_BEAM_Y) {
        r01s_entity_drive(e, "EQ#", R01S_LVL_H);
    }
}

static void pld_eval(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    int i;
    uint8_t p = 0;
    uint8_t q = 0;
    char pn[8], qn[8];

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

    /* Decode / VRAM glue: visible passthrough stub for bench bring-up. */
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
        r01s_entity_set_dip(&chip->base, 24, 72);
    } else {
        r01s_entity_set_dip(&chip->base, 24, 64);
    }
    r01s_entity_add_pin(&chip->base, role == R01S_PLD_BEAM_Y ? 35 : 17, "VCC", R01S_PIN_PWR);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_atf22v10_entity(R01sAtf22v10 *chip) {
    return chip ? &chip->base : NULL;
}

int r01s_atf22v10_eq(const R01sAtf22v10 *chip) {
    return chip && chip->eq;
}
