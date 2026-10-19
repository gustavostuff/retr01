#include "beam_xy.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void drive_q(R01sEntity *e, int count) {
    int bit;
    char name[4];
    for (bit = 0; bit < 9; bit++) {
        snprintf(name, sizeof(name), "Q%d", bit);
        r01s_entity_drive(e, name, (count & (1 << bit)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void beam_drive(R01sBeamXy *c) {
    R01sEntity *e = &c->base;
    int hs = c->x >= R01S_HSYNC_FIRST && c->x <= R01S_HSYNC_LAST;
    drive_q(e, c->x);
    r01s_entity_drive(e, "HSYNC", hs ? R01S_LVL_H : R01S_LVL_L);
}

static int res_low(R01sEntity *e) {
    return r01s_level_is_low(r01s_entity_sense(e, "RES"));
}

static void beam_reset(R01sEntity *e) {
    R01sBeamXy *c = (R01sBeamXy *)e;
    c->x = 0;
    c->clk_prev = R01S_LVL_L;
    beam_drive(c);
}

static void beam_eval(R01sEntity *e) {
    R01sBeamXy *c = (R01sBeamXy *)e;
    if (res_low(e)) {
        c->x = 0;
    }
    beam_drive(c);
}

static void beam_tick(R01sEntity *e) {
    R01sBeamXy *c = (R01sBeamXy *)e;
    R01sLevel clk = r01s_entity_sense(e, "CLK");
    int rise = (clk == R01S_LVL_H && c->clk_prev != R01S_LVL_H);

    if (res_low(e)) {
        c->x = 0;
        c->clk_prev = clk;
        beam_drive(c);
        return;
    }
    if (rise) {
        c->x++;
        if (c->x >= R01S_BEAM_DOTS_X) {
            c->x = 0;
        }
    }
    c->clk_prev = clk;
    beam_drive(c);
}

static void beam_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable BEAM_VT = {beam_reset, beam_eval, beam_tick, beam_destroy};

static void add_outs(R01sEntity *e, const int *pins, const char *const *names, int n) {
    int i;
    for (i = 0; i < n; i++) {
        r01s_entity_add_pin(e, pins[i], names[i], R01S_PIN_OUT);
    }
}

void r01s_beam_xy_init(R01sBeamXy *chip, const char *refdes) {
    static const int q_pins[9] = {21, 23, 16, 15, 17, 20, 19, 18, 22};
    static const char *const q_names[9] = {"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7", "Q8"};
    int n;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &BEAM_VT, "ATF22V10", refdes ? refdes : "UPLDX");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "RES", R01S_PIN_IN);
    {
        static const char *const in_names[9] = {"IN3", "IN4", "IN5", "IN6", "IN7", "IN8", "IN9", "IN10", "IN11"};
        for (n = 0; n < 9; n++) {
            r01s_entity_add_pin(&chip->base, 3 + n, in_names[n], R01S_PIN_IN);
        }
    }
    r01s_entity_add_pin(&chip->base, 12, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 13, "IN13", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 14, "HSYNC", R01S_PIN_OUT);
    add_outs(&chip->base, q_pins, q_names, 9);
    r01s_entity_add_pin(&chip->base, 24, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip_mm(&chip->base, 24, 32, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_beam_xy_entity(R01sBeamXy *chip) {
    return chip ? &chip->base : NULL;
}

int r01s_beam_xy_x(const R01sBeamXy *chip) {
    return chip ? chip->x : 0;
}

int r01s_beam_xy_hblank(const R01sBeamXy *chip) {
    return chip ? (chip->x >= R01S_BEAM_VISIBLE_W) : 0;
}

int r01s_beam_xy_hsync(const R01sBeamXy *chip) {
    return chip && chip->x >= R01S_HSYNC_FIRST && chip->x <= R01S_HSYNC_LAST;
}

void r01s_beam_xy_rewind(R01sBeamXy *chip) {
    if (!chip) {
        return;
    }
    chip->x = 0;
    chip->clk_prev = R01S_LVL_L;
    beam_drive(chip);
}
