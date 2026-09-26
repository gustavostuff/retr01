#include "ad724.h"

#include "discrete_ic/bus.h"

#include <string.h>

static int ad724_encode(R01aAd724 *c) {
    NsEntity *e = &c->base;
    NsLevel fin = ns_entity_sense(e, "FIN");
    int encd = ns_level_is_high(ns_entity_sense(e, "ENCD"));
    int apos = ns_level_is_high(ns_entity_sense(e, "APOS"));
    int dpos = ns_level_is_high(ns_entity_sense(e, "DPOS"));
    int stnd = ns_level_is_high(ns_entity_sense(e, "STND"));
    int select_fsc = ns_level_is_low(ns_entity_sense(e, "SELECT"));
    int fin_driven = (fin == NS_LVL_H || fin == NS_LVL_L);

    if (fin_driven && fin != c->fin_prev && c->fin_prev != NS_LVL_Z) {
        c->saw_fin_edge = 1;
    }
    c->fin_prev = fin;
    return encd && apos && dpos && stnd && select_fsc && fin_driven && c->saw_fin_edge;
}

static void ad724_reset(NsEntity *e) {
    R01aAd724 *c = (R01aAd724 *)e;
    c->fin_prev = NS_LVL_Z;
    c->saw_fin_edge = 0;
    c->encode_ok = 0;
    ns_entity_drive(e, "COMP", NS_LVL_L);
    ns_entity_drive(e, "LUMA", NS_LVL_L);
    ns_entity_drive(e, "CRMA", NS_LVL_L);
}

static void ad724_eval(NsEntity *e) {
    R01aAd724 *c = (R01aAd724 *)e;
    c->encode_ok = ad724_encode(c);
    ns_entity_drive(e, "COMP", c->encode_ok ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "LUMA", c->encode_ok ? NS_LVL_H : NS_LVL_L);
    ns_entity_drive(e, "CRMA", c->encode_ok ? NS_LVL_H : NS_LVL_L);
}

static void ad724_tick(NsEntity *e) {
    ad724_eval(e);
}

static void ad724_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable AD724_VT = {ad724_reset, ad724_eval, ad724_tick, ad724_destroy};

void r01a_ad724_init(R01aAd724 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &AD724_VT, "AD724", refdes ? refdes : "UENC");
    chip->base.impl = chip;
    ns_entity_add_pin(&chip->base, 1, "STND", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "AGND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 3, "FIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "APOS", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 5, "ENCD", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 6, "RIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 7, "GIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 8, "BIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 9, "CRMA", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 10, "COMP", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 11, "LUMA", NS_PIN_OUT);
    ns_entity_add_pin(&chip->base, 12, "SELECT", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 13, "DGND", NS_PIN_PWR);
    ns_entity_add_pin(&chip->base, 14, "DPOS", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 15, "VSYNC", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 16, "HSYNC", NS_PIN_IN);
    ns_entity_set_dip(&chip->base, 16);
    ns_entity_reset(&chip->base);
}

NsEntity *r01a_ad724_entity(R01aAd724 *chip) {
    return chip ? &chip->base : NULL;
}

int r01a_ad724_encode_ok(const R01aAd724 *chip) {
    return chip ? chip->encode_ok : 0;
}
