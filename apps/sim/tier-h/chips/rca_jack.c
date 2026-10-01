#include "rca_jack.h"

#include <string.h>

static void rca_reset(R01sEntity *e) {
    (void)e;
}

static void rca_eval(R01sEntity *e) {
    (void)e;
}

static void rca_tick(R01sEntity *e) {
    (void)e;
}

static void rca_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable RCA_VT = {rca_reset, rca_eval, rca_tick, rca_destroy};

void r01s_rca_jack_init(R01sRcaJack *jack, const char *refdes, const char *part) {
    if (!jack) {
        return;
    }
    memset(jack, 0, sizeof(*jack));
    r01s_entity_init(&jack->base, &RCA_VT, part ? part : "RCJ-014", refdes ? refdes : "J9");
    jack->base.impl = jack;
    r01s_entity_add_pin(&jack->base, 11, "1A", R01S_PIN_PWR);
    r01s_entity_add_pin(&jack->base, 12, "1B", R01S_PIN_PWR);
    r01s_entity_add_pin(&jack->base, 13, "1C", R01S_PIN_PWR);
    r01s_entity_add_pin(&jack->base, 2, "2", R01S_PIN_IO);
    r01s_entity_set_glyph(&jack->base, R01S_ENTITY_VIS_PANEL, 24, 16);
    r01s_entity_reset(&jack->base);
}

R01sEntity *r01s_rca_jack_entity(R01sRcaJack *jack) {
    return jack ? &jack->base : NULL;
}
