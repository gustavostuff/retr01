#include "sn74hc74.h"

#include "discrete_ic/bus.h"

#include <string.h>

static void hc74_reset(NsEntity *e) {
    R01aSn74hc74 *c = (R01aSn74hc74 *)e;
    c->clk1_prev = NS_LVL_L;
    c->clk2_prev = NS_LVL_L;
    c->q1 = NS_LVL_L;
    c->q2 = NS_LVL_L;
    ns_entity_drive(e, "1Q", NS_LVL_L);
    ns_entity_drive(e, "1/Q", NS_LVL_H);
    ns_entity_drive(e, "2Q", NS_LVL_L);
    ns_entity_drive(e, "2/Q", NS_LVL_H);
}

static void hc74_eval(NsEntity *e) {
    R01aSn74hc74 *c = (R01aSn74hc74 *)e;
    NsLevel vcc = ns_entity_sense(e, "VCC");
    NsLevel gnd = ns_entity_sense(e, "GND");
    NsLevel pre1;
    NsLevel clr1;
    NsLevel pre2;
    NsLevel clr2;

    if (!ns_level_is_high(vcc) || !ns_level_is_low(gnd)) {
        ns_entity_drive(e, "1Q", NS_LVL_Z);
        ns_entity_drive(e, "1/Q", NS_LVL_Z);
        ns_entity_drive(e, "2Q", NS_LVL_Z);
        ns_entity_drive(e, "2/Q", NS_LVL_Z);
        return;
    }

    pre1 = ns_entity_sense(e, "1PRE#");
    clr1 = ns_entity_sense(e, "1CLR#");
    if (ns_level_is_low(pre1) && !ns_level_is_low(clr1)) {
        c->q1 = NS_LVL_H;
        ns_entity_drive(e, "1Q", NS_LVL_H);
        ns_entity_drive(e, "1/Q", NS_LVL_L);
    } else if (!ns_level_is_low(pre1) && ns_level_is_low(clr1)) {
        c->q1 = NS_LVL_L;
        ns_entity_drive(e, "1Q", NS_LVL_L);
        ns_entity_drive(e, "1/Q", NS_LVL_H);
    } else if (ns_level_is_low(pre1) && ns_level_is_low(clr1)) {
        ns_entity_drive(e, "1Q", NS_LVL_H);
        ns_entity_drive(e, "1/Q", NS_LVL_H);
    } else {
        ns_entity_drive(e, "1Q", c->q1);
        ns_entity_drive(e, "1/Q", c->q1 == NS_LVL_H ? NS_LVL_L : NS_LVL_H);
    }

    pre2 = ns_entity_sense(e, "2PRE#");
    clr2 = ns_entity_sense(e, "2CLR#");
    if (ns_level_is_low(pre2) && !ns_level_is_low(clr2)) {
        c->q2 = NS_LVL_H;
        ns_entity_drive(e, "2Q", NS_LVL_H);
        ns_entity_drive(e, "2/Q", NS_LVL_L);
    } else if (!ns_level_is_low(pre2) && ns_level_is_low(clr2)) {
        c->q2 = NS_LVL_L;
        ns_entity_drive(e, "2Q", NS_LVL_L);
        ns_entity_drive(e, "2/Q", NS_LVL_H);
    } else if (ns_level_is_low(pre2) && ns_level_is_low(clr2)) {
        ns_entity_drive(e, "2Q", NS_LVL_H);
        ns_entity_drive(e, "2/Q", NS_LVL_H);
    } else {
        ns_entity_drive(e, "2Q", c->q2);
        ns_entity_drive(e, "2/Q", c->q2 == NS_LVL_H ? NS_LVL_L : NS_LVL_H);
    }
}

static void hc74_tick(NsEntity *e) {
    R01aSn74hc74 *c = (R01aSn74hc74 *)e;
    NsLevel vcc = ns_entity_sense(e, "VCC");
    NsLevel gnd = ns_entity_sense(e, "GND");
    NsLevel pre1;
    NsLevel clr1;
    NsLevel clk1;
    NsLevel pre2;
    NsLevel clr2;
    NsLevel clk2;

    if (!ns_level_is_high(vcc) || !ns_level_is_low(gnd)) {
        return;
    }

    pre1 = ns_entity_sense(e, "1PRE#");
    clr1 = ns_entity_sense(e, "1CLR#");
    clk1 = ns_entity_sense(e, "1CLK");
    if (c->clk1_prev == NS_LVL_L && ns_level_is_high(clk1)) {
        if (!ns_level_is_low(pre1) && !ns_level_is_low(clr1)) {
            NsLevel d1 = ns_entity_sense(e, "1D");
            c->q1 = ns_level_is_high(d1) ? NS_LVL_H : NS_LVL_L;
        }
    }
    c->clk1_prev = clk1;

    pre2 = ns_entity_sense(e, "2PRE#");
    clr2 = ns_entity_sense(e, "2CLR#");
    clk2 = ns_entity_sense(e, "2CLK");
    if (c->clk2_prev == NS_LVL_L && ns_level_is_high(clk2)) {
        if (!ns_level_is_low(pre2) && !ns_level_is_low(clr2)) {
            NsLevel d2 = ns_entity_sense(e, "2D");
            c->q2 = ns_level_is_high(d2) ? NS_LVL_H : NS_LVL_L;
        }
    }
    c->clk2_prev = clk2;

    hc74_eval(e);
}

static void hc74_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable HC74_VT = {hc74_reset, hc74_eval, hc74_tick, hc74_destroy};

void r01a_sn74hc74_init(R01aSn74hc74 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &HC74_VT, "74HC74", refdes ? refdes : "U74");
    chip->base.impl = chip;

    ns_entity_add_pin(&chip->base, 1, "1CLR#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "1D", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 3, "1CLK", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "1PRE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 5, "1Q", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 6, "1/Q", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 7, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 8, "2/Q", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 9, "2Q", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 10, "2PRE#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 11, "2CLK", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 12, "2D", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 13, "2CLR#", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 14, "VCC", NS_PIN_PWR);

    ns_entity_set_dip(&chip->base, 14);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_sn74hc74_entity(R01aSn74hc74 *chip) {
    return chip ? &chip->base : NULL;
}
