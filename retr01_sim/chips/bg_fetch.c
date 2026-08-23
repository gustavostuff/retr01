#include "bg_fetch.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

static void bg_drive_va(R01sBgFetch *c) {
    R01sEntity *e = &c->base;
    int i;
    char name[8];
    for (i = 0; i < 15; i++) {
        snprintf(name, sizeof(name), "VA%d", i);
        if (c->fetching) {
            r01s_entity_drive(e, name, (c->va & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
        } else {
            r01s_entity_drive(e, name, R01S_LVL_Z);
        }
    }
    r01s_entity_drive(e, "FETCH#", c->fetching ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(e, "ATTR#", c->attr_cycle ? R01S_LVL_L : R01S_LVL_H);
}

static void bg_drive_latches(R01sBgFetch *c) {
    R01sEntity *e = &c->base;
    int i;
    char name[8];
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "TQ%d", i);
        r01s_entity_drive(e, name, (c->last_tile & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
        snprintf(name, sizeof(name), "AQ%d", i);
        r01s_entity_drive(e, name, (c->last_attr & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static uint16_t bg_compute_va(const R01sBgFetch *c, int *attr_cycle_out) {
    int lx = c->beam_x / 2;
    int ly = c->beam_y / 2;
    int sx;
    int sy;
    int slot_x;
    int slot_y;
    int slot;
    int local_x;
    int local_y;
    int tx;
    int ty;
    int cell;
    int attr;

    sx = (int)(c->scroll_x & 127u) + lx;
    sy = (int)(c->scroll_y < 120u ? c->scroll_y : 119u) + ly;
    slot_x = (sx / R01S_BG_SCREEN_PX_W) & 1;
    slot_y = (sy / R01S_BG_SCREEN_PX_H) & 1;
    slot = slot_y * 2 + slot_x;
    local_x = sx - slot_x * R01S_BG_SCREEN_PX_W;
    local_y = sy - slot_y * R01S_BG_SCREEN_PX_H;
    if (local_x < 0) {
        local_x = 0;
    }
    if (local_y < 0) {
        local_y = 0;
    }
    tx = local_x / 8;
    ty = local_y / 8;
    if (tx >= R01S_BG_SCREEN_TILES_X) {
        tx = R01S_BG_SCREEN_TILES_X - 1;
    }
    if (ty > 14) {
        ty = 14;
    }
    cell = ty * R01S_BG_SCREEN_TILES_X + tx;
    attr = (c->beam_x & 1) != 0;
    if (attr_cycle_out) {
        *attr_cycle_out = attr;
    }
    if (attr) {
        return (uint16_t)(slot * R01S_BG_SLOT_BYTES + R01S_BG_ATTR_OFF + cell);
    }
    return (uint16_t)(slot * R01S_BG_SLOT_BYTES + cell);
}

static void bg_recompute(R01sBgFetch *c) {
    int attr = 0;
    int visible;

    visible = !c->hblank && !c->vblank && c->beam_x < 256 && c->beam_y < 240;
    if (c->cpu_phase || !visible) {
        c->fetching = 0;
        c->attr_cycle = 0;
        c->va = 0;
    } else {
        c->fetching = 1;
        c->va = bg_compute_va(c, &attr);
        c->attr_cycle = attr;
    }
    bg_drive_va(c);
    bg_drive_latches(c);
}

static void bg_reset(R01sEntity *e) {
    R01sBgFetch *c = (R01sBgFetch *)e;
    c->beam_x = 0;
    c->beam_y = 0;
    c->hblank = 0;
    c->vblank = 0;
    c->cpu_phase = 1;
    c->scroll_x = 0;
    c->scroll_y = 0;
    c->va = 0;
    c->fetching = 0;
    c->attr_cycle = 0;
    c->last_tile = 0;
    c->last_attr = 0;
    c->fetch_count = 0;
    bg_drive_va(c);
    bg_drive_latches(c);
}

static void bg_eval(R01sEntity *e) {
    bg_recompute((R01sBgFetch *)e);
}

static void bg_tick(R01sEntity *e) {
    (void)e; /* combinatorial / board-driven */
}

static void bg_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable BG_FETCH_VT = {bg_reset, bg_eval, bg_tick, bg_destroy};

void r01s_bg_fetch_init(R01sBgFetch *chip, const char *refdes) {
    static const char *const VA_NAMES[15] = {"VA0",  "VA1",  "VA2",  "VA3",  "VA4",  "VA5",  "VA6", "VA7",
                                             "VA8",  "VA9",  "VA10", "VA11", "VA12", "VA13", "VA14"};
    static const char *const DQ_NAMES[8] = {"DQ0", "DQ1", "DQ2", "DQ3", "DQ4", "DQ5", "DQ6", "DQ7"};
    static const char *const TQ_NAMES[8] = {"TQ0", "TQ1", "TQ2", "TQ3", "TQ4", "TQ5", "TQ6", "TQ7"};
    static const char *const AQ_NAMES[8] = {"AQ0", "AQ1", "AQ2", "AQ3", "AQ4", "AQ5", "AQ6", "AQ7"};
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &BG_FETCH_VT, "BG_FETCH", refdes ? refdes : "UPLDI");
    chip->base.impl = chip;
    for (i = 0; i < 15; i++) {
        r01s_entity_add_pin(&chip->base, 1 + i, VA_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 16, "FETCH#", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 17, "ATTR#", R01S_PIN_OUT);
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 18 + i, DQ_NAMES[i], R01S_PIN_IN);
    }
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 26 + i, TQ_NAMES[i], R01S_PIN_OUT);
    }
    for (i = 0; i < 8; i++) {
        r01s_entity_add_pin(&chip->base, 34 + i, AQ_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 42, "VCC", R01S_PIN_PWR);
    r01s_entity_set_dip(&chip->base, 42, 72);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_bg_fetch_entity(R01sBgFetch *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_bg_fetch_set_beam(R01sBgFetch *chip, int x, int y, int hblank, int vblank) {
    if (!chip) {
        return;
    }
    chip->beam_x = x;
    chip->beam_y = y;
    chip->hblank = hblank ? 1 : 0;
    chip->vblank = vblank ? 1 : 0;
}

void r01s_bg_fetch_set_scroll(R01sBgFetch *chip, uint8_t sx, uint8_t sy) {
    if (!chip) {
        return;
    }
    chip->scroll_x = (uint8_t)(sx & 127u);
    chip->scroll_y = (uint8_t)(sy < 120u ? sy : 119u);
}

void r01s_bg_fetch_set_cpu_phase(R01sBgFetch *chip, int cpu_phase) {
    if (!chip) {
        return;
    }
    chip->cpu_phase = cpu_phase ? 1 : 0;
}

void r01s_bg_fetch_capture_dq(R01sBgFetch *chip, uint8_t data) {
    if (!chip || !chip->fetching) {
        return;
    }
    if (chip->attr_cycle) {
        chip->last_attr = data;
    } else {
        chip->last_tile = data;
    }
    chip->fetch_count++;
    bg_drive_latches(chip);
}

uint16_t r01s_bg_fetch_va(const R01sBgFetch *chip) {
    return chip ? chip->va : 0;
}

int r01s_bg_fetch_active(const R01sBgFetch *chip) {
    return chip ? chip->fetching : 0;
}

int r01s_bg_fetch_attr_cycle(const R01sBgFetch *chip) {
    return chip ? chip->attr_cycle : 0;
}

uint8_t r01s_bg_fetch_last_tile(const R01sBgFetch *chip) {
    return chip ? chip->last_tile : 0;
}

uint8_t r01s_bg_fetch_last_attr(const R01sBgFetch *chip) {
    return chip ? chip->last_attr : 0;
}

uint32_t r01s_bg_fetch_count(const R01sBgFetch *chip) {
    return chip ? chip->fetch_count : 0;
}
