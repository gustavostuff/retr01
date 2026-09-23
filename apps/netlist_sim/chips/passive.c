#include "netlist_sim/passive.h"

#include "netlist_sim/ui_passive_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PassiveSprite {
    const uint8_t *rgba;
    int w;
    int h;
    int piv_x;
    int piv_y;
    int span_px;
    int span_y;
} PassiveSprite;

static const PassiveSprite k_sprites[NS_PASSIVE_KIND_COUNT] = {
    {NS_UI_PASSIVE_R_RGBA, NS_UI_PASSIVE_R_W, NS_UI_PASSIVE_R_H, NS_UI_PASSIVE_R_PIV_X,
     NS_UI_PASSIVE_R_PIV_Y, NS_UI_PASSIVE_R_SPAN_PX, NS_UI_PASSIVE_R_SPAN_Y},
    {NS_UI_PASSIVE_CCAP_RGBA, NS_UI_PASSIVE_CCAP_W, NS_UI_PASSIVE_CCAP_H, NS_UI_PASSIVE_CCAP_PIV_X,
     NS_UI_PASSIVE_CCAP_PIV_Y, NS_UI_PASSIVE_CCAP_SPAN_PX, NS_UI_PASSIVE_CCAP_SPAN_Y},
    {NS_UI_PASSIVE_ECAP_RGBA, NS_UI_PASSIVE_ECAP_W, NS_UI_PASSIVE_ECAP_H, NS_UI_PASSIVE_ECAP_PIV_X,
     NS_UI_PASSIVE_ECAP_PIV_Y, NS_UI_PASSIVE_ECAP_SPAN_PX, NS_UI_PASSIVE_ECAP_SPAN_Y},
    {NS_UI_PASSIVE_OSC_RGBA, NS_UI_PASSIVE_OSC_W, NS_UI_PASSIVE_OSC_H, NS_UI_PASSIVE_OSC_PIV_X,
     NS_UI_PASSIVE_OSC_PIV_Y, NS_UI_PASSIVE_OSC_SPAN_PX, NS_UI_PASSIVE_OSC_SPAN_Y},
    {NS_UI_PASSIVE_OSC4LEGS_RGBA, NS_UI_PASSIVE_OSC4LEGS_W, NS_UI_PASSIVE_OSC4LEGS_H,
     NS_UI_PASSIVE_OSC4LEGS_PIV_X, NS_UI_PASSIVE_OSC4LEGS_PIV_Y, NS_UI_PASSIVE_OSC4LEGS_SPAN_PX,
     NS_UI_PASSIVE_OSC4LEGS_SPAN_Y},
    {NS_UI_PASSIVE_D_RGBA, NS_UI_PASSIVE_D_W, NS_UI_PASSIVE_D_H, NS_UI_PASSIVE_D_PIV_X,
     NS_UI_PASSIVE_D_PIV_Y, NS_UI_PASSIVE_D_SPAN_PX, NS_UI_PASSIVE_D_SPAN_Y},
};

static const NsEntityVTable k_passive_vt = {NULL, NULL, NULL, NULL};

static int osc4_delta(int pin_num, int *dx, int *dy);

static void rot_cw_delta(int dx, int dy, int steps, int *ox, int *oy) {
    int x = dx;
    int y = dy;
    int i;
    int k = steps & 3;
    for (i = 0; i < k; i++) {
        int t = x;
        x = y;
        y = -t;
    }
    *ox = x;
    *oy = y;
}

#define R_BAND_GOLD 10
#define R_BAND_SILVER 11

static int r_parse_ohms(const char *s, double *out) {
    char *end = NULL;
    double v;
    if (!s || !s[0] || !out) {
        return 0;
    }
    v = strtod(s, &end);
    if (end == s || v <= 0.0) {
        return 0;
    }
    while (*end == ' ') {
        end++;
    }
    if (*end == 'k' || *end == 'K') {
        v *= 1000.0;
        end++;
    } else if (*end == 'M') {
        v *= 1000000.0;
        end++;
    } else if (*end == 'R' || *end == 'r') {
        end++;
    }
    *out = v;
    return 1;
}

static int r_eia_bands(const char *value, int *b) {
    double ohms;
    double sig;
    int mult = 0;
    if (!b || !r_parse_ohms(value, &ohms)) {
        return 0;
    }
    sig = ohms;
    while (sig >= 99.999) {
        sig /= 10.0;
        mult++;
    }
    while (sig < 10.0 && mult > -2) {
        sig *= 10.0;
        mult--;
    }
    if (sig < 10.0) {
        return 0;
    }
    b[0] = (int)(sig / 10.0 + 1e-6);
    b[1] = ((int)(sig + 1e-6)) % 10;
    if (b[0] < 0 || b[0] > 9 || b[1] < 0 || b[1] > 9) {
        return 0;
    }
    if (mult >= 0 && mult <= 9) {
        b[2] = mult;
    } else if (mult == -1) {
        b[2] = R_BAND_GOLD;
    } else if (mult == -2) {
        b[2] = R_BAND_SILVER;
    } else {
        return 0;
    }
    b[3] = R_BAND_GOLD;
    return 1;
}

static void r_band_rgb(int code, uint8_t *r, uint8_t *g, uint8_t *b) {
    static const uint8_t k[12][3] = {
        {18, 18, 18},    {112, 64, 30},  {198, 38, 38},  {232, 120, 28},
        {236, 204, 32},  {38, 150, 50},  {42, 74, 198},  {132, 50, 176},
        {132, 132, 136}, {238, 238, 238}, {206, 158, 32}, {178, 178, 186},
    };
    if (code < 0 || code > 11) {
        code = 0;
    }
    if (r) {
        *r = k[code][0];
    }
    if (g) {
        *g = k[code][1];
    }
    if (b) {
        *b = k[code][2];
    }
}

static int r_band_at_x(int sx) {
    static const int xs[4] = {5, 7, 9, 14};
    int i;
    for (i = 0; i < 4; i++) {
        if (sx == xs[i]) {
            return i;
        }
    }
    return -1;
}

static const PassiveSprite *sprite_for(NsPassiveKind kind) {
    if (kind < 0 || kind >= NS_PASSIVE_KIND_COUNT) {
        return &k_sprites[NS_PASSIVE_R];
    }
    return &k_sprites[kind];
}

const char *ns_passive_kind_name(NsPassiveKind kind) {
    switch (kind) {
    case NS_PASSIVE_R:
        return "R";
    case NS_PASSIVE_CCAP:
        return "CCAP";
    case NS_PASSIVE_ECAP:
        return "ECAP";
    case NS_PASSIVE_OSC:
        return "OSC";
    case NS_PASSIVE_OSC4LEGS:
        return "OSC4LEGS";
    case NS_PASSIVE_D:
        return "D";
    default:
        return "?";
    }
}

void ns_passive_bank_clear(NsPassiveBank *bank) {
    if (bank) {
        memset(bank, 0, sizeof(*bank));
    }
}

static void passive_aabb_about_pivot(const PassiveSprite *sp, NsPkgOrient orient, int *min_x, int *min_y,
                                     int *max_x, int *max_y) {
    int sx, sy;
    int first = 1;
    *min_x = *min_y = 0;
    *max_x = *max_y = 0;
    for (sy = 0; sy < sp->h; sy++) {
        for (sx = 0; sx < sp->w; sx++) {
            const uint8_t *px = sp->rgba + ((size_t)sy * (size_t)sp->w + (size_t)sx) * 4u;
            int dx, dy, rx, ry;
            if (px[3] == 0) {
                continue;
            }
            dx = sx - sp->piv_x;
            dy = sy - sp->piv_y;
            rot_cw_delta(dx, dy, (int)orient, &rx, &ry);
            if (first) {
                *min_x = *max_x = rx;
                *min_y = *max_y = ry;
                first = 0;
            } else {
                if (rx < *min_x) {
                    *min_x = rx;
                }
                if (ry < *min_y) {
                    *min_y = ry;
                }
                if (rx > *max_x) {
                    *max_x = rx;
                }
                if (ry > *max_y) {
                    *max_y = ry;
                }
            }
        }
    }
    if (first) {
        *min_x = *min_y = 0;
        *max_x = *max_y = 0;
    }
}

void ns_passive_sync_aabb(NsPassive *p) {
    const PassiveSprite *sp;
    int min_x, min_y, max_x, max_y;
    if (!p) {
        return;
    }
    sp = sprite_for(p->kind);
    passive_aabb_about_pivot(sp, p->base.orient, &min_x, &min_y, &max_x, &max_y);
    p->base.board_x = p->pivot_x + min_x;
    p->base.board_y = p->pivot_y + min_y;
    p->base.body_w = max_x - min_x + 1;
    p->base.body_h = max_y - min_y + 1;
    if (p->base.body_w < 1) {
        p->base.body_w = 1;
    }
    if (p->base.body_h < 1) {
        p->base.body_h = 1;
    }
}

void ns_passive_set_pivot(NsPassive *p, int pivot_x, int pivot_y) {
    if (!p) {
        return;
    }
    p->pivot_x = pivot_x;
    p->pivot_y = pivot_y;
    ns_passive_sync_aabb(p);
}

void ns_passive_set_orient(NsPassive *p, NsPkgOrient orient) {
    if (!p) {
        return;
    }
    p->base.orient = orient;
    ns_passive_sync_aabb(p);
}

int ns_passive_tip_board(const NsPassive *p, int pin_num, int *wx, int *wy) {
    const PassiveSprite *sp;
    int dx;
    int dy;
    int rx;
    int ry;
    if (!p || !wx || !wy || pin_num < 1) {
        return 0;
    }
    sp = sprite_for(p->kind);
    if (sp->span_y != 0) {
        if (!osc4_delta(pin_num, &dx, &dy)) {
            return 0;
        }
        rot_cw_delta(dx, dy, (int)p->base.orient, &rx, &ry);
        *wx = p->pivot_x + rx;
        *wy = p->pivot_y + ry;
        return 1;
    }
    if (pin_num == 1) {
        *wx = p->pivot_x;
        *wy = p->pivot_y;
        return 1;
    }
    if (pin_num != 2 || sp->span_px == 0) {
        return 0;
    }
    dx = sp->span_px;
    dy = 0;
    rot_cw_delta(dx, dy, (int)p->base.orient, &rx, &ry);
    *wx = p->pivot_x + rx;
    *wy = p->pivot_y + ry;
    return 1;
}

NsPassive *ns_passive_bank_add(NsPassiveBank *bank, NsPassiveKind kind, const char *refdes,
                                   const char *value) {
    NsPassive *p;
    const char *part;
    if (!bank || bank->count >= NS_PASSIVE_MAX) {
        return NULL;
    }
    p = &bank->parts[bank->count++];
    memset(p, 0, sizeof(*p));
    part = ns_passive_kind_name(kind);
    snprintf(p->refdes_buf, sizeof(p->refdes_buf), "%s", refdes ? refdes : part);
    snprintf(p->value, sizeof(p->value), "%s", value ? value : "");
    ns_entity_init(&p->base, &k_passive_vt, part, p->refdes_buf);
    ns_entity_set_glyph(&p->base, NS_ENTITY_VIS_PASSIVE, 1, 1);
    p->kind = kind;
    p->polarized = (kind == NS_PASSIVE_ECAP || kind == NS_PASSIVE_D) ? 1 : 0;
    p->base.orient = NS_ORIENT_0;
    if (kind == NS_PASSIVE_OSC4LEGS) {
        ns_entity_add_pin(&p->base, 1, "OE#", NS_PIN_IN);
        ns_entity_add_pin(&p->base, 7, "GND", NS_PIN_PWR);
        ns_entity_add_pin(&p->base, 8, "OUT", NS_PIN_OUT);
        ns_entity_add_pin(&p->base, 14, "VDD", NS_PIN_IN);
    } else if (kind == NS_PASSIVE_ECAP) {
        ns_entity_add_pin(&p->base, 1, "-", NS_PIN_IO);
        ns_entity_add_pin(&p->base, 2, "+", NS_PIN_IO);
    } else if (kind == NS_PASSIVE_D) {
        ns_entity_add_pin(&p->base, 1, "A", NS_PIN_IO);
        ns_entity_add_pin(&p->base, 2, "K", NS_PIN_IO);
    } else {
        ns_entity_add_pin(&p->base, 1, "1", NS_PIN_IO);
        ns_entity_add_pin(&p->base, 2, "2", NS_PIN_IO);
    }
    ns_passive_set_pivot(p, 0, 0);
    return p;
}

static void ns_passive_draw_ex(SDL_Renderer *r, NsPassiveKind kind, NsPkgOrient orient, const char *value,
                              int screen_pivot_x, int screen_pivot_y, int selected) {
    const PassiveSprite *sp;
    int sx, sy;
    int min_x, min_y, max_x, max_y;
    int bands[4];
    int have_bands = 0;
    if (!r) {
        return;
    }
    sp = sprite_for(kind);
    if (kind == NS_PASSIVE_R) {
        have_bands = r_eia_bands(value, bands);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (sy = 0; sy < sp->h; sy++) {
        for (sx = 0; sx < sp->w; sx++) {
            const uint8_t *px = sp->rgba + ((size_t)sy * (size_t)sp->w + (size_t)sx) * 4u;
            int dx, dy, rx, ry;
            uint8_t cr = px[0];
            uint8_t cg = px[1];
            uint8_t cb = px[2];
            if (px[3] == 0) {
                continue;
            }
            if (kind == NS_PASSIVE_R && cr > 200 && cg > 200 && cb > 200) {
                int bi = r_band_at_x(sx);
                cr = 214;
                cg = 186;
                cb = 132;
                if (have_bands && bi >= 0) {
                    r_band_rgb(bands[bi], &cr, &cg, &cb);
                }
            }
            dx = sx - sp->piv_x;
            dy = sy - sp->piv_y;
            rot_cw_delta(dx, dy, (int)orient, &rx, &ry);
            SDL_SetRenderDrawColor(r, cr, cg, cb, px[3]);
            SDL_RenderDrawPoint(r, screen_pivot_x + rx, screen_pivot_y + ry);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    if (selected) {
        passive_aabb_about_pivot(sp, orient, &min_x, &min_y, &max_x, &max_y);
        SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
        {
            SDL_Rect rc = {screen_pivot_x + min_x - 1, screen_pivot_y + min_y - 1, max_x - min_x + 3,
                           max_y - min_y + 3};
            SDL_RenderDrawRect(r, &rc);
        }
    }
}

void ns_passive_draw_kind(SDL_Renderer *r, NsPassiveKind kind, NsPkgOrient orient, int screen_pivot_x,
                         int screen_pivot_y, int selected) {
    ns_passive_draw_ex(r, kind, orient, NULL, screen_pivot_x, screen_pivot_y, selected);
}

void ns_passive_draw(SDL_Renderer *r, const NsPassive *p, int screen_pivot_x, int screen_pivot_y,
                       int selected) {
    if (!p) {
        return;
    }
    ns_passive_draw_ex(r, p->kind, p->base.orient, p->value, screen_pivot_x, screen_pivot_y, selected);
}

static int osc4_delta(int pin_num, int *dx, int *dy) {
    const PassiveSprite *sp = sprite_for(NS_PASSIVE_OSC4LEGS);
    if (!dx || !dy) {
        return 0;
    }
    /* Filename pivot is pin 14 (top-left, VDD). Top-right 8, bottom-left 1, bottom-right 7. */
    if (pin_num == 14) {
        *dx = 0;
        *dy = 0;
        return 1;
    }
    if (pin_num == 8) {
        *dx = sp->span_px;
        *dy = 0;
        return 1;
    }
    if (pin_num == 1) {
        *dx = 0;
        *dy = sp->span_y;
        return 1;
    }
    if (pin_num == 7) {
        *dx = sp->span_px;
        *dy = sp->span_y;
        return 1;
    }
    return 0;
}

static void osc4_aabb(NsPkgOrient orient, int *min_x, int *min_y, int *max_x, int *max_y) {
    passive_aabb_about_pivot(sprite_for(NS_PASSIVE_OSC4LEGS), orient, min_x, min_y, max_x, max_y);
}

int ns_osc4legs_chip_tip(const NsEntity *e, int pin_num, int *wx, int *wy) {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int dx;
    int dy;
    int rx;
    int ry;
    if (!e || !wx || !wy || !osc4_delta(pin_num, &dx, &dy)) {
        return 0;
    }
    osc4_aabb(e->orient, &min_x, &min_y, &max_x, &max_y);
    rot_cw_delta(dx, dy, (int)e->orient, &rx, &ry);
    *wx = e->board_x - min_x + rx;
    *wy = e->board_y - min_y + ry;
    return 1;
}

void ns_osc4legs_sync_aabb(NsEntity *e) {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    if (!e) {
        return;
    }
    osc4_aabb(e->orient, &min_x, &min_y, &max_x, &max_y);
    e->body_w = max_x - min_x + 1;
    e->body_h = max_y - min_y + 1;
    if (e->body_w < 1) {
        e->body_w = 1;
    }
    if (e->body_h < 1) {
        e->body_h = 1;
    }
}

void ns_osc4legs_set_orient(NsEntity *e, NsPkgOrient orient) {
    int p1x;
    int p1y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    if (!e) {
        return;
    }
    if (!ns_osc4legs_chip_tip(e, 14, &p1x, &p1y)) {
        p1x = e->board_x;
        p1y = e->board_y;
    }
    e->orient = orient;
    osc4_aabb(e->orient, &min_x, &min_y, &max_x, &max_y);
    e->board_x = p1x + min_x;
    e->board_y = p1y + min_y;
    ns_osc4legs_sync_aabb(e);
}

static void add_n(NsPassiveBank *bank, NsPassiveKind kind, const char *prefix, int *seq,
                  const char *value, int n) {
    int i;
    for (i = 0; i < n; i++) {
        char ref[NS_PASSIVE_REF_LEN];
        snprintf(ref, sizeof(ref), "%s%d", prefix, (*seq)++);
        if (!ns_passive_bank_add(bank, kind, ref, value)) {
            return;
        }
    }
}

int ns_passive_bank_spawn_bom(NsPassiveBank *bank) {
    int r_seq = 1;
    int c_seq = 1;
    int e_seq = 1;
    int y_seq = 1;
    if (!bank) {
        return -1;
    }
    ns_passive_bank_clear(bank);

    /* Ordered: OSC, CCAP, ECAP, R. No diodes on this BOM. */
    add_n(bank, NS_PASSIVE_OSC4LEGS, "Y", &y_seq, "8.000MHz", 1);
    add_n(bank, NS_PASSIVE_OSC4LEGS, "Y", &y_seq, "5.369318MHz", 1);
    add_n(bank, NS_PASSIVE_OSC4LEGS, "Y", &y_seq, "14.31818MHz", 1);

    add_n(bank, NS_PASSIVE_CCAP, "C", &c_seq, "100nF", 21);
    add_n(bank, NS_PASSIVE_CCAP, "C", &c_seq, "22pF", 6);

    add_n(bank, NS_PASSIVE_ECAP, "E", &e_seq, "220uF", 1);

    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "4.00k", 2);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "2.00k", 3);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "1.00k", 3);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "75.0", 3);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "33", 14);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "4.7k", 4);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "10k", 1);

    return bank->count;
}

void ns_passive_bank_layout_grid(NsPassiveBank *bank, int origin_x, int origin_y, int row_gap,
                                   int col_gap) {
    int i;
    int x;
    int y;
    int row_h;
    int max_row_w = 420;
    if (!bank) {
        return;
    }
    if (row_gap < 4) {
        row_gap = 4;
    }
    if (col_gap < 4) {
        col_gap = 4;
    }
    x = origin_x;
    y = origin_y;
    row_h = 0;
    for (i = 0; i < bank->count; i++) {
        NsPassive *p = &bank->parts[i];
        int cell_w;
        int cell_h;
        int start_new_row = 0;
        ns_passive_set_orient(p, NS_ORIENT_0);
        cell_w = p->base.body_w + col_gap;
        cell_h = p->base.body_h + row_gap;
        if (i > 0 && bank->parts[i - 1].kind != p->kind) {
            start_new_row = 1;
        }
        if (start_new_row || (x > origin_x && x + cell_w > origin_x + max_row_w)) {
            x = origin_x;
            y += row_h;
            row_h = 0;
        }
        /* Place so AABB top-left is (x,y): pivot = top-left - aabb_min. */
        {
            const PassiveSprite *sp = sprite_for(p->kind);
            int min_x, min_y, max_x, max_y;
            passive_aabb_about_pivot(sp, p->base.orient, &min_x, &min_y, &max_x, &max_y);
            ns_passive_set_pivot(p, x - min_x, y - min_y);
        }
        if (cell_h > row_h) {
            row_h = cell_h;
        }
        x += cell_w;
    }
}
