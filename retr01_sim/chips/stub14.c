#include "stub14.h"

#include <string.h>

static void stub14_reset(R01sEntity *e) {
    R01sStub14 *c = (R01sStub14 *)e;
    int i;
    c->eval_count = 0;
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].dir == R01S_PIN_PWR) {
            continue;
        }
        e->pins[i].level = R01S_LVL_Z;
    }
}

static void stub14_eval(R01sEntity *e) {
    R01sStub14 *c = (R01sStub14 *)e;
    c->eval_count++;
}

static void stub14_tick(R01sEntity *e) {
    (void)e;
}

static void stub14_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable STUB14_VT = {
    stub14_reset,
    stub14_eval,
    stub14_tick,
    stub14_destroy,
};

static const struct {
    int num;
    const char *name;
    R01sPinDir dir;
} STUB14_PINS[] = {
    {1, "1A", R01S_PIN_IN},   {2, "1Y", R01S_PIN_OUT}, {3, "2A", R01S_PIN_IN},  {4, "2Y", R01S_PIN_OUT},
    {5, "3A", R01S_PIN_IN},   {6, "3Y", R01S_PIN_OUT}, {7, "GND", R01S_PIN_PWR}, {8, "4Y", R01S_PIN_OUT},
    {9, "4A", R01S_PIN_IN},   {10, "5Y", R01S_PIN_OUT}, {11, "5A", R01S_PIN_IN}, {12, "6Y", R01S_PIN_OUT},
    {13, "6A", R01S_PIN_IN},  {14, "VCC", R01S_PIN_PWR},
};

void r01s_stub14_init(R01sStub14 *chip, const char *refdes) {
    size_t i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &STUB14_VT, "STUB14", refdes ? refdes : "U?");
    chip->base.impl = chip;
    for (i = 0; i < sizeof(STUB14_PINS) / sizeof(STUB14_PINS[0]); i++) {
        r01s_entity_add_pin(&chip->base, STUB14_PINS[i].num, STUB14_PINS[i].name, STUB14_PINS[i].dir);
    }
    r01s_entity_set_dip(&chip->base, 14, 48, 100);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_stub14_entity(R01sStub14 *chip) {
    return chip ? &chip->base : NULL;
}
