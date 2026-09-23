#include "netlist_sim/entity.h"
#include "netlist_sim/passive.h"

#include <string.h>

/*
 * Molded PDIP body LxW (mm). Drawn at NS_PX_PER_MM.
 * Across-body px is snapped to the pin/breadboard pitch grid so opposing pin
 * tip origins are N * NS_DIP_PIN_PITCH_PX apart (see ns_dip_snap_across_px).
 * See hw/md/packages_dip.md (Microchip DS00049 + JEDEC MS-001/MS-011).
 */
typedef struct NsDipPkg {
    int pins;
    int len_mm;
    int wid_mm;
} NsDipPkg;

static const NsDipPkg NS_DIP_PKGS[] = {
    /* 14/16/20: 74HC N-package family (body width ~6.35 -> 6 mm). */
    {8, 9, 6},   {14, 19, 6},  {16, 20, 6},  {20, 25, 6},
    /* 24 default = 300 mil ATF class. 600 mil 24-pin (AT28C16) uses set_dip_mm. */
    {24, 32, 8}, {28, 36, 14}, {32, 42, 14}, {40, 52, 14},
};

void ns_dip_pkg_mm(int dip_pins, int *len_mm, int *wid_mm) {
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
    for (i = 0; i < (int)(sizeof(NS_DIP_PKGS) / sizeof(NS_DIP_PKGS[0])); i++) {
        if (NS_DIP_PKGS[i].pins == pins) {
            if (len_mm) {
                *len_mm = NS_DIP_PKGS[i].len_mm;
            }
            if (wid_mm) {
                *wid_mm = NS_DIP_PKGS[i].wid_mm;
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

int ns_dip_body_along_px(int dip_pins) {
    int len_mm = 0;
    ns_dip_pkg_mm(dip_pins, &len_mm, NULL);
    return len_mm * NS_PX_PER_MM;
}

int ns_dip_snap_across_px(int across_px) {
    int pitch = NS_DIP_PIN_PITCH_PX;
    int tip_span;
    /* Opposing tip origins on the hole lattice (N * pitch).
     * JEDEC dual-row class from molded width, plus one extra breadboard pitch
     * so the body is thick enough for the package label. */
    if (across_px <= 0) {
        across_px = pitch;
    }
    if (across_px <= 18) {
        tip_span = 4 * pitch; /* was 3 (300 mil); +1 pitch for label room */
    } else {
        tip_span = 7 * pitch; /* was 6 (600 mil); +1 pitch for label room */
    }
    return tip_span - pitch;
}

int ns_dip_body_across_px(int dip_pins) {
    int wid_mm = 0;
    ns_dip_pkg_mm(dip_pins, NULL, &wid_mm);
    return ns_dip_snap_across_px(wid_mm * NS_PX_PER_MM);
}

void ns_entity_refresh_body(NsEntity *e) {
    int along;
    int across;
    if (!e) {
        return;
    }
    along = e->pkg_len_mm > 0 ? e->pkg_len_mm * NS_PX_PER_MM : ns_dip_body_along_px(e->dip_pins);
    across = e->pkg_wid_mm > 0 ? e->pkg_wid_mm * NS_PX_PER_MM : ns_dip_body_across_px(e->dip_pins);
    if (along < 1) {
        along = 1;
    }
    across = ns_dip_snap_across_px(across);
    if (!ns_orient_is_horiz(e->orient)) {
        e->body_w = across;
        e->body_h = along;
    } else {
        e->body_w = along;
        e->body_h = across;
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
    return h % NS_PIN_HASH_SIZE;
}

void ns_entity_pin_hash_build(NsEntity *e) {
    int i;
    if (!e || e->pin_hash_built) {
        return;
    }
    for (i = 0; i < NS_PIN_HASH_SIZE; i++) {
        e->pin_hash_idx[i] = -1;
    }
    for (i = 0; i < e->pin_count; i++) {
        unsigned slot = entity_pin_hash_key(e->pins[i].name);
        int probes = 0;
        while (e->pin_hash_idx[slot] >= 0) {
            slot = (slot + 1u) % NS_PIN_HASH_SIZE;
            probes++;
            if (probes >= NS_PIN_HASH_SIZE) {
                break;
            }
        }
        if (probes < NS_PIN_HASH_SIZE) {
            e->pin_hash_idx[slot] = (int8_t)i;
        }
    }
    e->pin_hash_built = 1;
}

void ns_entity_init(NsEntity *e, const NsEntityVTable *vt, const char *part, const char *refdes) {
    if (!e) {
        return;
    }
    memset(e, 0, sizeof(*e));
    e->vt = vt;
    e->part = part;
    e->refdes = refdes;
    e->orient = NS_ORIENT_H;
    e->health = NS_HEALTH_BOOT;
    e->body_w = 40;
    e->body_h = 24;
    {
        int hi;
        for (hi = 0; hi < NS_PIN_HASH_SIZE; hi++) {
            e->pin_hash_idx[hi] = -1;
        }
    }
}

int ns_entity_add_pin(NsEntity *e, int number, const char *name, NsPinDir dir) {
    if (!e || e->pin_count >= NS_MAX_PINS) {
        return -1;
    }
    ns_pin_init(&e->pins[e->pin_count], number, name, dir);
    e->pin_count++;
    e->pin_hash_built = 0;
    return 0;
}

void ns_entity_set_dip(NsEntity *e, int dip_pins) {
    if (!e) {
        return;
    }
    e->visual = NS_ENTITY_VIS_IC;
    e->dip_pins = dip_pins > 0 ? dip_pins : 0;
    ns_dip_pkg_mm(e->dip_pins, &e->pkg_len_mm, &e->pkg_wid_mm);
    e->orient = NS_ORIENT_H;
    ns_entity_refresh_body(e);
}

void ns_entity_set_dip_mm(NsEntity *e, int dip_pins, int len_mm, int wid_mm) {
    if (!e) {
        return;
    }
    e->visual = NS_ENTITY_VIS_IC;
    e->dip_pins = dip_pins > 0 ? dip_pins : 0;
    if (len_mm > 0 && wid_mm > 0) {
        e->pkg_len_mm = len_mm;
        e->pkg_wid_mm = wid_mm;
    } else {
        ns_dip_pkg_mm(e->dip_pins, &e->pkg_len_mm, &e->pkg_wid_mm);
    }
    e->orient = NS_ORIENT_H;
    ns_entity_refresh_body(e);
}

void ns_entity_set_orient(NsEntity *e, NsPkgOrient orient) {
    if (!e) {
        return;
    }
    e->orient = orient;
    if (e->visual == NS_ENTITY_VIS_IC && e->dip_pins > 0) {
        ns_entity_refresh_body(e);
    }
}

void ns_entity_set_glyph(NsEntity *e, NsEntityVisual visual, int body_w, int body_h) {
    if (!e) {
        return;
    }
    e->visual = visual;
    e->dip_pins = 0;
    e->pkg_len_mm = 0;
    e->pkg_wid_mm = 0;
    e->orient = NS_ORIENT_H;
    e->body_w = body_w > 0 ? body_w : 25;
    e->body_h = body_h > 0 ? body_h : 25;
}

void ns_entity_place(NsEntity *e, int board_x, int board_y) {
    if (!e) {
        return;
    }
    e->board_x = board_x;
    e->board_y = board_y;
}

void ns_entity_reset(NsEntity *e) {
    if (e && e->vt && e->vt->reset) {
        e->vt->reset(e);
    }
}

void ns_entity_eval(NsEntity *e) {
    if (e && e->vt && e->vt->eval) {
        e->vt->eval(e);
    }
}

void ns_entity_tick(NsEntity *e) {
    if (e && e->vt && e->vt->tick) {
        e->vt->tick(e);
    }
}

void ns_entity_destroy(NsEntity *e) {
    if (e && e->vt && e->vt->destroy) {
        e->vt->destroy(e);
    }
    if (e) {
        e->impl = NULL;
    }
}

NsPin *ns_entity_pin(NsEntity *e, int number) {
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

const NsPin *ns_entity_pin_const(const NsEntity *e, int number) {
    return ns_entity_pin((NsEntity *)e, number);
}

NsPin *ns_entity_pin_named(NsEntity *e, const char *name) {
    int i;
    if (!e || !name) {
        return NULL;
    }
    if (!e->pin_hash_built) {
        ns_entity_pin_hash_build(e);
    }
    if (e->pin_hash_built) {
        unsigned slot = entity_pin_hash_key(name);
        int probes = 0;
        while (probes < NS_PIN_HASH_SIZE) {
            int idx = e->pin_hash_idx[slot];
            if (idx < 0) {
                break;
            }
            if (e->pins[idx].name && strcmp(e->pins[idx].name, name) == 0) {
                return &e->pins[idx];
            }
            slot = (slot + 1u) % NS_PIN_HASH_SIZE;
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

const NsPin *ns_entity_pin_named_const(const NsEntity *e, const char *name) {
    return ns_entity_pin_named((NsEntity *)e, name);
}

static int pin_tip_reach(void) {
    return 2; /* pin.png: H=3, OY=0, tip is 2 px out from the body edge */
}

static void dip_pin_pos(const NsEntity *e, int pin_num, int *along, int *side_pin1) {
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int half = dip / 2;
    int idx;
    int span;
    int pitch = NS_DIP_PIN_PITCH_PX;
    int row_span;
    int margin;
    int reverse;

    if (dip <= 0 || pin_num <= 0 || pin_num > dip) {
        *side_pin1 = 1;
        *along = ns_orient_is_horiz(e->orient) ? (e->body_w / 2) : (e->body_h / 2);
        return;
    }
    *side_pin1 = pin_num <= half;
    idx = *side_pin1 ? (pin_num - 1) : (dip - pin_num);
    span = ns_orient_is_horiz(e->orient) ? e->body_w : e->body_h;
    row_span = (half > 1) ? (half - 1) * pitch : 0;
    margin = (span - row_span) / 2;
    if (margin < 1) {
        margin = 1;
    }
    reverse = (e->orient == NS_ORIENT_180 || e->orient == NS_ORIENT_270);
    if (reverse) {
        *along = margin + (half > 0 ? (half - 1 - idx) : 0) * pitch;
    } else {
        *along = margin + idx * pitch;
    }
}

static int glyph_pin_bottom(const NsPin *p) {
    if (!p) {
        return 1;
    }
    if (p->dir == NS_PIN_OUT) {
        return 0;
    }
    if (p->dir == NS_PIN_PWR && p->name && (strcmp(p->name, "VDD") == 0 || strcmp(p->name, "VCC") == 0)) {
        return 0;
    }
    return 1;
}

static int glyph_pin_tip_board(const NsEntity *e, int pin_num, int *tbx, int *tby) {
    int i;
    int bi = 0;
    int ti = 0;
    int reach = pin_tip_reach();

    for (i = 0; i < e->pin_count; i++) {
        int bottom;
        int idx;
        int along;
        if (e->pins[i].dir == NS_PIN_NC) {
            continue;
        }
        bottom = glyph_pin_bottom(&e->pins[i]);
        if (bottom) {
            idx = bi++;
        } else {
            idx = ti++;
        }
        if (e->pins[i].number != pin_num) {
            continue;
        }
        along = 5 + idx * NS_DIP_PIN_PITCH_PX;
        if (along > e->body_w - 3) {
            along = e->body_w - 3;
        }
        *tbx = e->board_x + along;
        *tby = bottom ? (e->board_y + e->body_h + reach) : (e->board_y - 1 - reach);
        return 1;
    }
    return 0;
}

int ns_entity_pin_tip_board(const NsEntity *e, int pin_num, int *tbx, int *tby) {
    int along;
    int side_pin1;
    int reach;
    int dip;

    if (!e || !tbx || !tby) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_OSC) {
        return ns_osc4legs_chip_tip(e, pin_num, tbx, tby);
    }
    if (e->visual == NS_ENTITY_VIS_PWR) {
        return glyph_pin_tip_board(e, pin_num, tbx, tby);
    }
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    if (pin_num < 1 || pin_num > dip) {
        return 0;
    }
    reach = pin_tip_reach();
    dip_pin_pos(e, pin_num, &along, &side_pin1);
    switch (e->orient) {
    case NS_ORIENT_90:
        *tby = e->board_y + along;
        *tbx = side_pin1 ? (e->board_x - 1 - reach) : (e->board_x + e->body_w + reach);
        break;
    case NS_ORIENT_180:
        *tbx = e->board_x + along;
        *tby = side_pin1 ? (e->board_y - 1 - reach) : (e->board_y + e->body_h + reach);
        break;
    case NS_ORIENT_270:
        *tby = e->board_y + along;
        *tbx = side_pin1 ? (e->board_x + e->body_w + reach) : (e->board_x - 1 - reach);
        break;
    case NS_ORIENT_0:
    default:
        *tbx = e->board_x + along;
        *tby = side_pin1 ? (e->board_y + e->body_h + reach) : (e->board_y - 1 - reach);
        break;
    }
    return 1;
}
