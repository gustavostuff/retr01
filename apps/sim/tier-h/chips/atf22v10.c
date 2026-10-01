#include "atf22v10.h"

#include "retr01_sim/bus.h"
#include "retr01_sim/timing.h"

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

static void pld_drive_sel_mask(R01sEntity *e, uint8_t off, int hit) {
    pld_drive_named(e, "SEL_7F02", hit && off == 0x02u);
    pld_drive_named(e, "SEL_7F03", hit && off == 0x03u);
    pld_drive_named(e, "SEL_7F04", hit && off == 0x04u);
    pld_drive_named(e, "SEL_7F08", hit && off == 0x08u);
    pld_drive_named(e, "SEL_7F10", hit && off == 0x10u);
    pld_drive_named(e, "SEL_7F11", hit && off == 0x11u);
    pld_drive_named(e, "SEL_7F12", hit && off == 0x12u);
    pld_drive_named(e, "SEL_7F90", hit && off == 0x90u);
    pld_drive_named(e, "SEL_7F91", hit && off == 0x91u);
    pld_drive_named(e, "SEL_7F92", hit && off == 0x92u);
    pld_drive_named(e, "SEL_7F93", hit && off == 0x93u);
}

static void pld_reset(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    c->p_bus = 0;
    c->q_bus = 0;
    c->eq = 0;
    r01s_delay_u8_reset(&c->out_delay, 0xFFu); /* 0xFF = no SEL */
    if (c->role == R01S_PLD_BEAM_Y) {
        r01s_delay_u8_reset(&c->out_delay, 0);
        pld_drive_byte(e, "Y", 0);
        r01s_entity_drive(e, "EQ#", R01S_LVL_H);
        return;
    }
    if (c->role == R01S_PLD_DECODE) {
        pld_drive_sel_mask(e, 0, 0);
        return;
    }
    /* VRAM glue: I->Y passthrough until interleave equations land. */
    r01s_delay_u8_reset(&c->out_delay, 0);
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
    int fe = r01s_level_is_low(r01s_entity_sense(e, "SOFT#")); /* active-low page hit */
    int be = r01s_level_is_high(r01s_entity_sense(e, "BE"));
    uint8_t off = pld_sense_a_lo(e);
    int hit = fe && be;
    uint8_t ideal = hit ? off : 0xFFu;

    /*
     * Decode SEL must be combinatorial in this netlist: soft / PLD loads sample
     * on the same settle pass. Deferred SEL misses STA $7Fxx (catchup FAIL).
     * Path delay is still counted in r01s_timing_path_decode_bus_reg_ns().
     */
    (void)r01s_delay_u8_update(&c->out_delay, ideal, 0);
    if (ideal == 0xFFu) {
        pld_drive_sel_mask(e, 0, 0);
    } else {
        pld_drive_sel_mask(e, ideal, 1);
    }
    c->p_bus = off;
    c->q_bus = hit ? off : 0;
}

static void pld_eval(R01sEntity *e) {
    R01sAtf22v10 *c = (R01sAtf22v10 *)e;
    int i;
    uint8_t p = 0;
    uint8_t q = 0;
    char pn[8], qn[8];
    uint8_t delayed;

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
        delayed = r01s_delay_u8_update(&c->out_delay, (uint8_t)(c->eq ? 1u : 0u),
                                      r01s_timing_pin_tpd_ns(R01S_TPD_PART_ATF22));
        r01s_entity_drive(e, "EQ#", (delayed & 1u) ? R01S_LVL_L : R01S_LVL_H);
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
    delayed = r01s_delay_u8_update(&c->out_delay, p, r01s_timing_pin_tpd_ns(R01S_TPD_PART_ATF22));
    c->q_bus = delayed;
    pld_drive_byte(e, "Y", delayed);
}

static void pld_tick(R01sEntity *e) {
    (void)e;
}

static void pld_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable ATF22_VT = {pld_reset, pld_eval, pld_tick, pld_destroy};

static int entity_has_pin_number(const R01sEntity *e, int number) {
    int i;
    if (!e) {
        return 0;
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].number == number) {
            return 1;
        }
    }
    return 0;
}

int r01s_atf22v10_alloc_pin(int *next) {
    int p;
    if (!next) {
        return 0;
    }
    p = *next;
    if (p < 1) {
        p = 1;
    }
    if (p == 12) {
        p = 13;
    }
    if (p < 1 || p > 23) {
        return 0;
    }
    *next = p + 1;
    if (*next == 12) {
        *next = 13;
    }
    return p;
}

void r01s_atf22v10_add_shell_pins(R01sEntity *e) {
    static const char *const in_names[10] = {"IN2", "IN3", "IN4", "IN5", "IN6", "IN7", "IN8", "IN9", "IN10", "IN11"};
    static const char *const io_names[10] = {"IO14", "IO15", "IO16", "IO17", "IO18",
                                              "IO19", "IO20", "IO21", "IO22", "IO23"};
    int i;
    if (!e) {
        return;
    }
    if (!entity_has_pin_number(e, 1)) {
        r01s_entity_add_pin(e, 1, "CLK", R01S_PIN_IN);
    }
    for (i = 0; i < 10; i++) {
        int n = 2 + i;
        if (!entity_has_pin_number(e, n)) {
            r01s_entity_add_pin(e, n, in_names[i], R01S_PIN_IN);
        }
    }
    if (!entity_has_pin_number(e, 13)) {
        r01s_entity_add_pin(e, 13, "IN13", R01S_PIN_IN);
    }
    for (i = 0; i < 10; i++) {
        int n = 14 + i;
        if (!entity_has_pin_number(e, n)) {
            r01s_entity_add_pin(e, n, io_names[i], R01S_PIN_IO);
        }
    }
}

static void atf_add_sig(R01sEntity *e, int *next, const char *name, R01sPinDir dir) {
    int n = r01s_atf22v10_alloc_pin(next);
    if (n <= 0) {
        /* Package is full. Keep the name for the sim, off the drawn legs. */
        n = 100 + e->pin_count;
    }
    r01s_entity_add_pin(e, n, name, dir);
}

static const char *const PLD_I_NAMES[8] = {"I0", "I1", "I2", "I3", "I4", "I5", "I6", "I7"};
static const char *const PLD_Y_NAMES[8] = {"Y0", "Y1", "Y2", "Y3", "Y4", "Y5", "Y6", "Y7"};
static const char *const PLD_P_NAMES[8] = {"P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"};
static const char *const PLD_Q_NAMES[8] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"};

void r01s_atf22v10_init(R01sAtf22v10 *chip, const char *refdes, int role) {
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    chip->role = role;
    r01s_entity_init(&chip->base, &ATF22_VT, "ATF22V10", refdes ? refdes : "UPLD");
    chip->base.impl = chip;

    /* Placement stand-in. Each signal gets its own leg. Not a JEDEC map. */
    r01s_entity_add_pin(&chip->base, 12, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 24, "VCC", R01S_PIN_PWR);
    {
        int next = 1;
        if (role == R01S_PLD_DECODE) {
            static const char *const A_NAMES[8] = {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7"};
            static const char *const SEL_NAMES[11] = {"SEL_7F02", "SEL_7F03", "SEL_7F04", "SEL_7F08", "SEL_7F10",
                                                      "SEL_7F11", "SEL_7F12", "SEL_7F90", "SEL_7F91", "SEL_7F92",
                                                      "SEL_7F93"};
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, A_NAMES[i], R01S_PIN_IN);
            }
            atf_add_sig(&chip->base, &next, "SOFT#", R01S_PIN_IN);
            atf_add_sig(&chip->base, &next, "BE", R01S_PIN_IN);
            atf_add_sig(&chip->base, &next, "RWB", R01S_PIN_IN);
            for (i = 0; i < 11; i++) {
                atf_add_sig(&chip->base, &next, SEL_NAMES[i], R01S_PIN_OUT);
            }
        } else if (role == R01S_PLD_BEAM_Y) {
            /* EQ# and P0-P7 are the nets on the canvas. They take legs before the spare names. */
            atf_add_sig(&chip->base, &next, "EQ#", R01S_PIN_OUT);
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, PLD_P_NAMES[i], R01S_PIN_IN);
            }
            atf_add_sig(&chip->base, &next, "OE#", R01S_PIN_IN);
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, PLD_I_NAMES[i], R01S_PIN_IN);
            }
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, PLD_Y_NAMES[i], R01S_PIN_OUT);
            }
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, PLD_Q_NAMES[i], R01S_PIN_IN);
            }
        } else {
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, PLD_I_NAMES[i], R01S_PIN_IN);
            }
            for (i = 0; i < 8; i++) {
                atf_add_sig(&chip->base, &next, PLD_Y_NAMES[i], R01S_PIN_OUT);
            }
        }
    }
    r01s_atf22v10_add_shell_pins(&chip->base);
    r01s_entity_set_dip_mm(&chip->base, 24, 32, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_atf22v10_entity(R01sAtf22v10 *chip) {
    return chip ? &chip->base : NULL;
}

int r01s_atf22v10_eq(const R01sAtf22v10 *chip) {
    return chip && chip->eq;
}
