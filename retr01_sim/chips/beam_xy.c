#include "beam_xy.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void beam_drive_bus(R01sEntity *e, const char *prefix, int width, int value) {
    int i;
    char name[8];
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, (value & (1 << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void beam_drive_flags(R01sBeamXy *c) {
    R01sEntity *e = &c->base;
    int hb = (c->x >= R01S_BEAM_VISIBLE_W);
    int vb = (c->y >= R01S_BEAM_VISIBLE_H);
    beam_drive_bus(e, "X", 9, c->x);
    beam_drive_bus(e, "Y", 9, c->y);
    r01s_entity_drive(e, "HBLANK", hb ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(e, "VBLANK", vb ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(e, "NMI#", (c->nmi_hold > 0) ? R01S_LVL_L : R01S_LVL_H);
}

static void beam_reset(R01sEntity *e) {
    R01sBeamXy *c = (R01sBeamXy *)e;
    c->x = 0;
    c->y = 0;
    c->dot_prev = R01S_LVL_L;
    c->nmi_hold = 0;
    beam_drive_flags(c);
}

static void beam_eval(R01sEntity *e) {
    R01sBeamXy *c = (R01sBeamXy *)e;
    if (r01s_level_is_low(r01s_entity_sense(e, "RES#"))) {
        c->x = 0;
        c->y = 0;
        c->nmi_hold = 0;
    }
    beam_drive_flags(c);
}

static void beam_tick(R01sEntity *e) {
    R01sBeamXy *c = (R01sBeamXy *)e;
    R01sLevel dot = r01s_entity_sense(e, "DOT");
    int rise = (dot == R01S_LVL_H && c->dot_prev != R01S_LVL_H);
    int entered_vblank = 0;

    if (r01s_level_is_low(r01s_entity_sense(e, "RES#"))) {
        c->x = 0;
        c->y = 0;
        c->nmi_hold = 0;
        c->dot_prev = dot;
        beam_drive_flags(c);
        return;
    }

    if (rise) {
        c->x++;
        if (c->x >= R01S_BEAM_DOTS_X) {
            c->x = 0;
            c->y++;
            if (c->y >= R01S_BEAM_DOTS_Y) {
                c->y = 0;
            }
            if (c->y == R01S_BEAM_VISIBLE_H) {
                entered_vblank = 1;
            }
        }
        if (entered_vblank) {
            c->nmi_hold = 8; /* short stub pulse */
        } else if (c->nmi_hold > 0) {
            c->nmi_hold--;
        }
    }
    c->dot_prev = dot;
    beam_drive_flags(c);
}

static void beam_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable BEAM_VT = {beam_reset, beam_eval, beam_tick, beam_destroy};

void r01s_beam_xy_init(R01sBeamXy *chip, const char *refdes) {
    static const char *const X_NAMES[9] = {"X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7", "X8"};
    static const char *const Y_NAMES[9] = {"Y0", "Y1", "Y2", "Y3", "Y4", "Y5", "Y6", "Y7", "Y8"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &BEAM_VT, "BEAM_XY", refdes ? refdes : "UPLD");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "DOT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "RES#", R01S_PIN_IN);
    for (i = 0; i < 9; i++) {
        r01s_entity_add_pin(&chip->base, 3 + i, X_NAMES[i], R01S_PIN_OUT);
    }
    for (i = 0; i < 9; i++) {
        r01s_entity_add_pin(&chip->base, 12 + i, Y_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 21, "HBLANK", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 22, "VBLANK", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 23, "NMI#", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 24, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 24, 64);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_beam_xy_entity(R01sBeamXy *chip) {
    return chip ? &chip->base : NULL;
}

int r01s_beam_xy_x(const R01sBeamXy *chip) {
    return chip ? chip->x : 0;
}

int r01s_beam_xy_y(const R01sBeamXy *chip) {
    return chip ? chip->y : 0;
}

int r01s_beam_xy_hblank(const R01sBeamXy *chip) {
    return chip ? (chip->x >= R01S_BEAM_VISIBLE_W) : 0;
}

int r01s_beam_xy_vblank(const R01sBeamXy *chip) {
    return chip ? (chip->y >= R01S_BEAM_VISIBLE_H) : 0;
}
