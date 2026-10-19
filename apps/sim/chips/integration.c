#include "integration.h"

#include <string.h>

static void integ_reset(R01sEntity *e) {
    R01sIntegration *c = (R01sIntegration *)e;
    c->nmi_pulses = 0;
    c->last_nmi_low = 0;
}

static void integ_eval(R01sEntity *e) {
    (void)e;
}

static void integ_tick(R01sEntity *e) {
    (void)e;
}

static void integ_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable INTEG_VT = {integ_reset, integ_eval, integ_tick, integ_destroy};

void r01s_integration_init(R01sIntegration *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &INTEG_VT, "INTEGRATION", refdes ? refdes : "UPLDP");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "NMI#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "OK", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 3, "PAD", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "VID", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "BUS", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 7, "NC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "VCC", R01S_PIN_PWR);
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_NONE, 0, 0);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_integration_entity(R01sIntegration *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_integration_note_nmi(R01sIntegration *chip) {
    if (!chip) {
        return;
    }
    chip->nmi_pulses++;
    chip->last_nmi_low = 1;
}

uint32_t r01s_integration_nmi_pulses(const R01sIntegration *chip) {
    return chip ? chip->nmi_pulses : 0;
}
