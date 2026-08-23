#include "retr01_sim/entity.h"

#include <string.h>

void r01s_entity_init(R01sEntity *e, const R01sEntityVTable *vt, const char *part, const char *refdes) {
    if (!e) {
        return;
    }
    memset(e, 0, sizeof(*e));
    e->vt = vt;
    e->part = part;
    e->refdes = refdes;
    e->body_w = 40;
    e->body_h = 80;
}

int r01s_entity_add_pin(R01sEntity *e, int number, const char *name, R01sPinDir dir) {
    if (!e || e->pin_count >= R01S_MAX_PINS) {
        return -1;
    }
    r01s_pin_init(&e->pins[e->pin_count], number, name, dir);
    e->pin_count++;
    return 0;
}

void r01s_entity_set_dip(R01sEntity *e, int dip_pins, int body_w, int body_h) {
    if (!e) {
        return;
    }
    e->dip_pins = dip_pins > 0 ? dip_pins : 0;
    e->body_w = body_w > 0 ? body_w : 40;
    e->body_h = body_h > 0 ? body_h : (dip_pins / 2) * 12 + 16;
}

void r01s_entity_place(R01sEntity *e, int board_x, int board_y) {
    if (!e) {
        return;
    }
    e->board_x = board_x;
    e->board_y = board_y;
}

void r01s_entity_reset(R01sEntity *e) {
    if (e && e->vt && e->vt->reset) {
        e->vt->reset(e);
    }
}

void r01s_entity_eval(R01sEntity *e) {
    if (e && e->vt && e->vt->eval) {
        e->vt->eval(e);
    }
}

void r01s_entity_tick(R01sEntity *e) {
    if (e && e->vt && e->vt->tick) {
        e->vt->tick(e);
    }
}

void r01s_entity_destroy(R01sEntity *e) {
    if (e && e->vt && e->vt->destroy) {
        e->vt->destroy(e);
    }
    if (e) {
        e->impl = NULL;
    }
}

R01sPin *r01s_entity_pin(R01sEntity *e, int number) {
    int i;
    if (!e) {
        return NULL;
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].number == number) {
            return &e->pins[i];
        }
    }
    return NULL;
}

const R01sPin *r01s_entity_pin_const(const R01sEntity *e, int number) {
    return r01s_entity_pin((R01sEntity *)e, number);
}
