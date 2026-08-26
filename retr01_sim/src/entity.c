#include "retr01_sim/entity.h"

#include "retr01_sim/board_layout.h"

#include <string.h>

/*
 * Molded PDIP body L×W (mm), rounded for a clean px/mm grid.
 * See hw/md/packages_dip.md (Microchip DS00049 + JEDEC MS-001/MS-011).
 */
typedef struct R01sDipPkg {
    int pins;
    int len_mm;
    int wid_mm;
} R01sDipPkg;

static const R01sDipPkg R01S_DIP_PKGS[] = {
    /* 14/16/20: 74HC N-package family (body width ~6.35 → 6 mm). */
    {8, 9, 6},   {14, 19, 6},  {16, 20, 6},  {20, 25, 6},
    /* Wider / longer defaults — prefer r01s_entity_set_dip_mm per part when known. */
    {24, 32, 14}, {28, 36, 14}, {32, 42, 14}, {40, 52, 14},
};

void r01s_dip_pkg_mm(int dip_pins, int *len_mm, int *wid_mm) {
    int i;
    int pins = dip_pins;
    int half;
    int len;
    int wid;

    if (pins < 2) {
        pins = 2;
    }
    if (pins & 1) {
        pins++;
    }
    for (i = 0; i < (int)(sizeof(R01S_DIP_PKGS) / sizeof(R01S_DIP_PKGS[0])); i++) {
        if (R01S_DIP_PKGS[i].pins == pins) {
            if (len_mm) {
                *len_mm = R01S_DIP_PKGS[i].len_mm;
            }
            if (wid_mm) {
                *wid_mm = R01S_DIP_PKGS[i].wid_mm;
            }
            return;
        }
    }
    /* Estimate: (rows-1)*2.54 mm + ~4 mm end overhang; width by row class. */
    half = pins / 2;
    len = (int)((half > 1 ? (half - 1) * 254 : 0) / 100) + 4;
    wid = (pins >= 24) ? 14 : 6;
    if (len_mm) {
        *len_mm = len;
    }
    if (wid_mm) {
        *wid_mm = wid;
    }
}

int r01s_dip_body_along_px(int dip_pins) {
    int len_mm = 0;
    r01s_dip_pkg_mm(dip_pins, &len_mm, NULL);
    return len_mm * R01S_PX_PER_MM;
}

int r01s_dip_body_across_px(int dip_pins) {
    int wid_mm = 0;
    r01s_dip_pkg_mm(dip_pins, NULL, &wid_mm);
    return wid_mm * R01S_PX_PER_MM;
}

void r01s_entity_refresh_body(R01sEntity *e) {
    int along;
    int across;
    if (!e) {
        return;
    }
    along = e->pkg_len_mm > 0 ? e->pkg_len_mm * R01S_PX_PER_MM : r01s_dip_body_along_px(e->dip_pins);
    across = e->pkg_wid_mm > 0 ? e->pkg_wid_mm * R01S_PX_PER_MM : r01s_dip_body_across_px(e->dip_pins);
    if (e->orient == R01S_ORIENT_V) {
        e->body_w = r01s_snap5_up(across);
        e->body_h = r01s_snap5_up(along);
    } else {
        e->body_w = r01s_snap5_up(along);
        e->body_h = r01s_snap5_up(across);
    }
}

static unsigned entity_pin_hash_key(const char *name) {
    unsigned h = 2166136261u;
    if (!name) {
        return 0;
    }
    while (*name) {
        h ^= (unsigned char)*name++;
        h *= 16777619u;
    }
    return h % R01S_PIN_HASH_SIZE;
}

void r01s_entity_pin_hash_build(R01sEntity *e) {
    int i;
    if (!e || e->pin_hash_built) {
        return;
    }
    for (i = 0; i < R01S_PIN_HASH_SIZE; i++) {
        e->pin_hash_idx[i] = -1;
    }
    for (i = 0; i < e->pin_count; i++) {
        unsigned slot = entity_pin_hash_key(e->pins[i].name);
        int probes = 0;
        while (e->pin_hash_idx[slot] >= 0) {
            slot = (slot + 1u) % R01S_PIN_HASH_SIZE;
            probes++;
            if (probes >= R01S_PIN_HASH_SIZE) {
                break;
            }
        }
        if (probes < R01S_PIN_HASH_SIZE) {
            e->pin_hash_idx[slot] = (int8_t)i;
        }
    }
    e->pin_hash_built = 1;
}

void r01s_entity_init(R01sEntity *e, const R01sEntityVTable *vt, const char *part, const char *refdes) {
    if (!e) {
        return;
    }
    memset(e, 0, sizeof(*e));
    e->vt = vt;
    e->part = part;
    e->refdes = refdes;
    e->orient = R01S_ORIENT_H;
    e->body_w = 40;
    e->body_h = 24;
    {
        int hi;
        for (hi = 0; hi < R01S_PIN_HASH_SIZE; hi++) {
            e->pin_hash_idx[hi] = -1;
        }
    }
}

int r01s_entity_add_pin(R01sEntity *e, int number, const char *name, R01sPinDir dir) {
    if (!e || e->pin_count >= R01S_MAX_PINS) {
        return -1;
    }
    r01s_pin_init(&e->pins[e->pin_count], number, name, dir);
    e->pin_count++;
    e->pin_hash_built = 0;
    return 0;
}

void r01s_entity_set_dip(R01sEntity *e, int dip_pins) {
    if (!e) {
        return;
    }
    e->visual = R01S_ENTITY_VIS_IC;
    e->dip_pins = dip_pins > 0 ? dip_pins : 0;
    r01s_dip_pkg_mm(e->dip_pins, &e->pkg_len_mm, &e->pkg_wid_mm);
    e->orient = R01S_ORIENT_H;
    r01s_entity_refresh_body(e);
}

void r01s_entity_set_dip_mm(R01sEntity *e, int dip_pins, int len_mm, int wid_mm) {
    if (!e) {
        return;
    }
    e->visual = R01S_ENTITY_VIS_IC;
    e->dip_pins = dip_pins > 0 ? dip_pins : 0;
    if (len_mm > 0 && wid_mm > 0) {
        e->pkg_len_mm = len_mm;
        e->pkg_wid_mm = wid_mm;
    } else {
        r01s_dip_pkg_mm(e->dip_pins, &e->pkg_len_mm, &e->pkg_wid_mm);
    }
    e->orient = R01S_ORIENT_H;
    r01s_entity_refresh_body(e);
}

void r01s_entity_set_orient(R01sEntity *e, R01sPkgOrient orient) {
    if (!e) {
        return;
    }
    e->orient = orient;
    if (e->visual == R01S_ENTITY_VIS_IC && e->dip_pins > 0) {
        r01s_entity_refresh_body(e);
    }
}

void r01s_entity_set_glyph(R01sEntity *e, R01sEntityVisual visual, int body_w, int body_h) {
    if (!e) {
        return;
    }
    e->visual = visual;
    e->dip_pins = 0;
    e->pkg_len_mm = 0;
    e->pkg_wid_mm = 0;
    e->orient = R01S_ORIENT_H;
    e->body_w = body_w > 0 ? r01s_snap5_up(body_w) : 25;
    e->body_h = body_h > 0 ? r01s_snap5_up(body_h) : 25;
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

R01sPin *r01s_entity_pin_named(R01sEntity *e, const char *name) {
    int i;
    if (!e || !name) {
        return NULL;
    }
    if (!e->pin_hash_built) {
        r01s_entity_pin_hash_build(e);
    }
    if (e->pin_hash_built) {
        unsigned slot = entity_pin_hash_key(name);
        int probes = 0;
        while (probes < R01S_PIN_HASH_SIZE) {
            int idx = e->pin_hash_idx[slot];
            if (idx < 0) {
                break;
            }
            if (e->pins[idx].name && strcmp(e->pins[idx].name, name) == 0) {
                return &e->pins[idx];
            }
            slot = (slot + 1u) % R01S_PIN_HASH_SIZE;
            probes++;
        }
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].name && strcmp(e->pins[i].name, name) == 0) {
            return &e->pins[i];
        }
    }
    return NULL;
}

const R01sPin *r01s_entity_pin_named_const(const R01sEntity *e, const char *name) {
    return r01s_entity_pin_named((R01sEntity *)e, name);
}
