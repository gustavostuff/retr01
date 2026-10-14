#include "atf22v10.h"

#include "discrete_ic/bus.h"
#include "discrete_ic/health.h"
#include "r01a_raster.h"

#include <string.h>

static void drive_bit(NsEntity *e, const char *name, unsigned value, int bit) {
    ns_entity_drive(e, name, (value & (1u << bit)) ? NS_LVL_H : NS_LVL_L);
}

static void beam_x_drive(R01aAtf22v10 *c) {
    NsEntity *e = &c->base;
    unsigned x = (unsigned)c->x;
    int hs = (c->x >= R01A_HSYNC_START && c->x < R01A_HSYNC_END);

    ns_entity_drive(e, "HSYNC", hs ? NS_LVL_H : NS_LVL_L);
    drive_bit(e, "Q0", x, 0);
    drive_bit(e, "Q1", x, 1);
    drive_bit(e, "Q2", x, 2);
    drive_bit(e, "Q3", x, 3);
    drive_bit(e, "Q4", x, 4);
    drive_bit(e, "Q5", x, 5);
    drive_bit(e, "Q6", x, 6);
    drive_bit(e, "Q7", x, 7);
    drive_bit(e, "Q8", x, 8);
}

static void beam_y_drive(R01aAtf22v10 *c) {
    NsEntity *e = &c->base;
    unsigned y = (unsigned)c->y;
    int vs = (c->y >= R01A_VSYNC_START && c->y < R01A_VSYNC_END);

    ns_entity_drive(e, "VSYNC", vs ? NS_LVL_H : NS_LVL_L);
    drive_bit(e, "Q0", y, 0);
    drive_bit(e, "Q1", y, 1);
    drive_bit(e, "Q2", y, 2);
    drive_bit(e, "Q3", y, 3);
    drive_bit(e, "Q4", y, 4);
    drive_bit(e, "Q5", y, 5);
    drive_bit(e, "Q6", y, 6);
    drive_bit(e, "Q7", y, 7);
    drive_bit(e, "Q8", y, 8);
}

static int res_held(NsEntity *e) {
    return ns_level_is_low(ns_entity_sense(e, "RES#"));
}

static void pld_reset(NsEntity *e) {
    R01aAtf22v10 *c = (R01aAtf22v10 *)e;
    c->x = 0;
    c->y = 0;
    c->clk_prev = NS_LVL_L;
    if (c->role == R01A_PLD_BEAM_X) {
        beam_x_drive(c);
    } else {
        beam_y_drive(c);
    }
}

static void pld_eval(NsEntity *e) {
    R01aAtf22v10 *c = (R01aAtf22v10 *)e;
    if (res_held(e)) {
        c->x = 0;
        c->y = 0;
    }
    if (c->role == R01A_PLD_BEAM_X) {
        beam_x_drive(c);
    } else {
        beam_y_drive(c);
    }
}

static void pld_tick(NsEntity *e) {
    R01aAtf22v10 *c = (R01aAtf22v10 *)e;
    NsLevel clk = ns_entity_sense(e, "CLK");
    int rise = (clk == NS_LVL_H && c->clk_prev != NS_LVL_H);
    int fall = (clk == NS_LVL_L && c->clk_prev == NS_LVL_H);

    if (res_held(e)) {
        c->x = 0;
        c->y = 0;
        c->clk_prev = clk;
        if (c->role == R01A_PLD_BEAM_X) {
            beam_x_drive(c);
        } else {
            beam_y_drive(c);
        }
        return;
    }

    if (c->role == R01A_PLD_BEAM_X) {
        if (fall) {
            c->x++;
            if (c->x >= R01A_BEAM_DOTS_X) {
                c->x = 0;
            }
        }
        beam_x_drive(c);
    } else if (rise) {
        c->y++;
        if (c->y >= R01A_BEAM_DOTS_Y) {
            c->y = 0;
        }
        beam_y_drive(c);
    } else {
        beam_y_drive(c);
    }
    c->clk_prev = clk;
}

static void pld_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable PLD_VT = {pld_reset, pld_eval, pld_tick, pld_destroy};

static void add_unused_ins(NsEntity *e) {
    ns_entity_add_pin(e, 3, "I3", NS_PIN_IN);
    ns_entity_add_pin(e, 4, "PD", NS_PIN_IN);
    ns_entity_add_pin(e, 5, "I5", NS_PIN_IN);
    ns_entity_add_pin(e, 6, "I6", NS_PIN_IN);
    ns_entity_add_pin(e, 7, "I7", NS_PIN_IN);
    ns_entity_add_pin(e, 8, "I8", NS_PIN_IN);
    ns_entity_add_pin(e, 9, "I9", NS_PIN_IN);
    ns_entity_add_pin(e, 10, "I10", NS_PIN_IN);
    ns_entity_add_pin(e, 11, "I11", NS_PIN_IN);
    ns_entity_add_pin(e, 13, "I13", NS_PIN_IN);
}

void r01a_atf22v10_init(R01aAtf22v10 *chip, const char *refdes, int role) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &PLD_VT, "ATF22V10", refdes ? refdes : "UPLD");
    chip->base.impl = chip;
    chip->role = role;
    ns_entity_add_pin(&chip->base, 1, "CLK", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "RES#", NS_PIN_IN);
    add_unused_ins(&chip->base);
    ns_entity_add_pin(&chip->base, 12, "GND", NS_PIN_PWR);
    if (role == R01A_PLD_BEAM_X) {
        ns_entity_add_pin(&chip->base, 14, "HSYNC", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 15, "Q3", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 16, "Q2", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 17, "Q4", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 18, "Q7", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 19, "Q6", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 20, "Q5", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 21, "Q0", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 22, "Q8", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 23, "Q1", NS_PIN_OUT);
    } else {
        ns_entity_add_pin(&chip->base, 14, "VSYNC", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 15, "Q2", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 16, "Q3", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 17, "Q5", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 18, "Q7", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 19, "Q6", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 20, "Q4", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 21, "Q1", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 22, "Q8", NS_PIN_OUT);
        ns_entity_add_pin(&chip->base, 23, "Q0", NS_PIN_OUT);
    }
    ns_entity_add_pin(&chip->base, 24, "VCC", NS_PIN_PWR);
    ns_entity_set_dip_mm(&chip->base, 24, 32, 8);
    chip->base.health = NS_HEALTH_OK;
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_atf22v10_entity(R01aAtf22v10 *chip) {
    return chip ? &chip->base : NULL;
}

int r01a_atf22v10_x(const R01aAtf22v10 *chip) {
    return chip ? chip->x : 0;
}

int r01a_atf22v10_y(const R01aAtf22v10 *chip) {
    return chip ? chip->y : 0;
}

int r01a_atf22v10_hblank(const R01aAtf22v10 *chip) {
    return chip ? (chip->x >= R01A_BEAM_VISIBLE_W) : 0;
}

int r01a_atf22v10_vblank(const R01aAtf22v10 *chip) {
    return chip ? (chip->y >= R01A_BEAM_VISIBLE_H) : 0;
}
