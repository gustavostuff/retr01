#include "discrete_ic/passive.h"

#include "discrete_ic/ui_passive_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pin pixels from tools/discrete_ic/assets/passives/pin_map.json.
 * pins[0] is the rotation pivot. negative (JSON) is index 1 for ECAP and D. */
typedef struct PassivePinDef {
    int number;
    int x;
    int y;
    const char *name;
} PassivePinDef;

typedef struct PassiveSprite {
    const uint8_t *rgba;
    int w;
    int h;
    int piv_x;
    int piv_y;
    int pin_count;
    PassivePinDef pins[4];
} PassiveSprite;

static const PassiveSprite *sprite_for(NsPassiveKind kind);

static const PassiveSprite k_sprites[NS_PASSIVE_KIND_COUNT] = {
    {NS_UI_PASSIVE_R_RGBA, NS_UI_PASSIVE_R_W, NS_UI_PASSIVE_R_H, 0, 2, 2,
     {{1, 0, 2, "1"}, {2, 20, 2, "2"}}},
    {NS_UI_PASSIVE_CCAP_RGBA, NS_UI_PASSIVE_CCAP_W, NS_UI_PASSIVE_CCAP_H, 2, 12, 2,
     {{1, 2, 12, "1"}, {2, 7, 12, "2"}}},
    {NS_UI_PASSIVE_ECAP_RGBA, NS_UI_PASSIVE_ECAP_W, NS_UI_PASSIVE_ECAP_H, 2, 19, 2,
     {{1, 2, 19, "+"}, {2, 7, 19, "-"}}},
    {NS_UI_PASSIVE_OSC_RGBA, NS_UI_PASSIVE_OSC_W, NS_UI_PASSIVE_OSC_H, 4, 11, 2,
     {{1, 4, 11, "1"}, {2, 9, 11, "2"}}},
    {NS_UI_PASSIVE_OSC4LEGS_RGBA, NS_UI_PASSIVE_OSC4LEGS_W, NS_UI_PASSIVE_OSC4LEGS_H, 4, 0, 4,
     {{14, 4, 0, "VDD"}, {8, 9, 0, "OUT"}, {1, 4, 15, "OE#"}, {7, 9, 15, "GND"}}},
    {NS_UI_PASSIVE_D_RGBA, NS_UI_PASSIVE_D_W, NS_UI_PASSIVE_D_H, 0, 2, 2,
     {{1, 0, 2, "A"}, {2, 15, 2, "K"}}},
    {NS_UI_PASSIVE_OSC_RGBA, NS_UI_PASSIVE_OSC_W, NS_UI_PASSIVE_OSC_H, 4, 11, 2,
     {{1, 4, 11, "1"}, {2, 9, 11, "2"}}},
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

static const PassivePinDef *pin_by_number(const PassiveSprite *sp, int number) {
    int i;
    if (!sp) {
        return NULL;
    }
    for (i = 0; i < sp->pin_count; i++) {
        if (sp->pins[i].number == number) {
            return &sp->pins[i];
        }
    }
    return NULL;
}

/* Whole PNG rectangle after rotation about pins[0]. Transparent pixels count. */
static int sprite_rect_hit(const PassiveSprite *sp, NsPkgOrient orient, int piv_x, int piv_y, int bx, int by) {
    int dx;
    int dy;
    int rx;
    int ry;
    int sx;
    int sy;
    if (!sp || sp->w < 1 || sp->h < 1) {
        return 0;
    }
    dx = bx - piv_x;
    dy = by - piv_y;
    rot_cw_delta(dx, dy, (4 - ((int)orient & 3)) & 3, &rx, &ry);
    sx = rx + sp->piv_x;
    sy = ry + sp->piv_y;
    return sx >= 0 && sy >= 0 && sx < sp->w && sy < sp->h;
}

/* Unit step from pin 1 toward pin 2 in unrotated PNG space. */
static void lead_axis_local(const PassiveSprite *sp, int *ux, int *uy) {
    int dx = 1;
    int dy = 0;
    if (sp && sp->pin_count >= 2) {
        dx = sp->pins[1].x - sp->pins[0].x;
        dy = sp->pins[1].y - sp->pins[0].y;
    }
    if (dy == 0 && dx != 0) {
        *ux = dx > 0 ? 1 : -1;
        *uy = 0;
        return;
    }
    if (dx == 0 && dy != 0) {
        *ux = 0;
        *uy = dy > 0 ? 1 : -1;
        return;
    }
    *ux = 1;
    *uy = 0;
}

static int chebyshev_to_seg(int px, int py, int ax, int ay, int bx, int by, int half) {
    int x0 = ax < bx ? ax : bx;
    int x1 = ax > bx ? ax : bx;
    int y0 = ay < by ? ay : by;
    int y1 = ay > by ? ay : by;
    int dx;
    int dy;
    if (px < x0) {
        dx = x0 - px;
    } else if (px > x1) {
        dx = px - x1;
    } else {
        dx = 0;
    }
    if (py < y0) {
        dy = y0 - py;
    } else if (py > y1) {
        dy = py - y1;
    } else {
        dy = 0;
    }
    return (dx > dy ? dx : dy) <= half;
}

static void r_axis(const NsPassive *p, int *ux, int *uy) {
    int lx;
    int ly;
    lead_axis_local(sprite_for(p->kind), &lx, &ly);
    rot_cw_delta(lx, ly, (int)p->base.orient, ux, uy);
}

static void r_default_tip(const NsPassive *p, int pin_num, int *wx, int *wy) {
    const PassiveSprite *sp = sprite_for(p->kind);
    const PassivePinDef *pin = pin_by_number(sp, pin_num);
    int rx;
    int ry;
    if (!pin) {
        *wx = p->pivot_x;
        *wy = p->pivot_y;
        return;
    }
    rot_cw_delta(pin->x - sp->piv_x, pin->y - sp->piv_y, (int)p->base.orient, &rx, &ry);
    *wx = p->pivot_x + rx;
    *wy = p->pivot_y + ry;
}

int ns_passive_hit(const NsPassive *p, int bx, int by) {
    int ux;
    int uy;
    int x0;
    int y0;
    int x1;
    int y1;
    if (!p) {
        return 0;
    }
    if (sprite_rect_hit(sprite_for(p->kind), p->base.orient, p->pivot_x, p->pivot_y, bx, by)) {
        return 1;
    }
    if (p->kind == NS_PASSIVE_OSC4LEGS || (p->leg_ext[0] <= 0 && p->leg_ext[1] <= 0)) {
        return 0;
    }
    r_axis(p, &ux, &uy);
    if (p->leg_ext[0] > 0) {
        r_default_tip(p, 1, &x0, &y0);
        x1 = x0 - ux * p->leg_ext[0];
        y1 = y0 - uy * p->leg_ext[0];
        if (chebyshev_to_seg(bx, by, x0, y0, x1, y1, 1)) {
            return 1;
        }
    }
    if (p->leg_ext[1] > 0) {
        r_default_tip(p, 2, &x0, &y0);
        x1 = x0 + ux * p->leg_ext[1];
        y1 = y0 + uy * p->leg_ext[1];
        if (chebyshev_to_seg(bx, by, x0, y0, x1, y1, 1)) {
            return 1;
        }
    }
    return 0;
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
    case NS_PASSIVE_XTAL:
        return "XTAL";
    default:
        return "?";
    }
}

void ns_passive_bank_clear(NsPassiveBank *bank) {
    if (bank) {
        memset(bank, 0, sizeof(*bank));
    }
}

static void expand_aabb(int rx, int ry, int *min_x, int *min_y, int *max_x, int *max_y, int *first) {
    if (*first) {
        *min_x = *max_x = rx;
        *min_y = *max_y = ry;
        *first = 0;
        return;
    }
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

/* Axis-aligned bounds of the full PNG rectangle, rotated about pins[0]. */
static void passive_aabb_about_pivot(const PassiveSprite *sp, NsPkgOrient orient, int *min_x, int *min_y,
                                     int *max_x, int *max_y) {
    int corners[4][2];
    int i;
    int first = 1;
    *min_x = *min_y = 0;
    *max_x = *max_y = 0;
    if (!sp || sp->w < 1 || sp->h < 1) {
        return;
    }
    corners[0][0] = 0;
    corners[0][1] = 0;
    corners[1][0] = sp->w - 1;
    corners[1][1] = 0;
    corners[2][0] = 0;
    corners[2][1] = sp->h - 1;
    corners[3][0] = sp->w - 1;
    corners[3][1] = sp->h - 1;
    for (i = 0; i < 4; i++) {
        int rx;
        int ry;
        rot_cw_delta(corners[i][0] - sp->piv_x, corners[i][1] - sp->piv_y, (int)orient, &rx, &ry);
        expand_aabb(rx, ry, min_x, min_y, max_x, max_y, &first);
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
    if (p->kind != NS_PASSIVE_OSC4LEGS && sp->pin_count >= 2 &&
        (p->leg_ext[0] > 0 || p->leg_ext[1] > 0)) {
        int lx;
        int ly;
        int rx;
        int ry;
        int first = 0;
        lead_axis_local(sp, &lx, &ly);
        rot_cw_delta(-lx * p->leg_ext[0], -ly * p->leg_ext[0], (int)p->base.orient, &rx, &ry);
        expand_aabb(rx, ry, &min_x, &min_y, &max_x, &max_y, &first);
        rot_cw_delta((sp->pins[1].x - sp->piv_x) + lx * p->leg_ext[1],
                     (sp->pins[1].y - sp->piv_y) + ly * p->leg_ext[1], (int)p->base.orient, &rx, &ry);
        expand_aabb(rx, ry, &min_x, &min_y, &max_x, &max_y, &first);
    }
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
    const PassivePinDef *pin;
    int dx;
    int dy;
    int lx;
    int ly;
    int rx;
    int ry;
    if (!p || !wx || !wy || pin_num < 1) {
        return 0;
    }
    sp = sprite_for(p->kind);
    pin = pin_by_number(sp, pin_num);
    if (!pin) {
        return 0;
    }
    dx = pin->x - sp->piv_x;
    dy = pin->y - sp->piv_y;
    if (p->kind != NS_PASSIVE_OSC4LEGS && sp->pin_count >= 2) {
        lead_axis_local(sp, &lx, &ly);
        if (pin_num == sp->pins[0].number) {
            dx -= lx * p->leg_ext[0];
            dy -= ly * p->leg_ext[0];
        } else if (pin_num == sp->pins[1].number) {
            dx += lx * p->leg_ext[1];
            dy += ly * p->leg_ext[1];
        }
    }
    rot_cw_delta(dx, dy, (int)p->base.orient, &rx, &ry);
    *wx = p->pivot_x + rx;
    *wy = p->pivot_y + ry;
    return 1;
}

void ns_passive_set_leg_ext(NsPassive *p, int pin_num, int extra) {
    if (!p || p->kind == NS_PASSIVE_OSC4LEGS) {
        return;
    }
    if (extra < 0) {
        extra = 0;
    }
    if (extra > 400) {
        extra = 400;
    }
    if (pin_num == 1) {
        p->leg_ext[0] = extra;
    } else if (pin_num == 2) {
        p->leg_ext[1] = extra;
    } else {
        return;
    }
    ns_passive_sync_aabb(p);
}

void ns_passive_set_leg_to(NsPassive *p, int pin_num, int wx, int wy) {
    int ux;
    int uy;
    int dx;
    int dy;
    int extra;
    int defx;
    int defy;
    if (!p || p->kind == NS_PASSIVE_OSC4LEGS || (pin_num != 1 && pin_num != 2)) {
        return;
    }
    r_axis(p, &ux, &uy);
    r_default_tip(p, pin_num, &defx, &defy);
    dx = wx - defx;
    dy = wy - defy;
    if (pin_num == 1) {
        extra = -dx * ux - dy * uy;
    } else {
        extra = dx * ux + dy * uy;
    }
    ns_passive_set_leg_ext(p, pin_num, extra);
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
        static const int order[] = {1, 7, 8, 14};
        static const NsPinDir dirs[] = {NS_PIN_IN, NS_PIN_PWR, NS_PIN_OUT, NS_PIN_IN};
        const PassiveSprite *sp = sprite_for(kind);
        int i;
        for (i = 0; i < 4; i++) {
            const PassivePinDef *pin = pin_by_number(sp, order[i]);
            ns_entity_add_pin(&p->base, order[i], pin ? pin->name : "?", dirs[i]);
        }
    } else {
        const PassiveSprite *sp = sprite_for(kind);
        int i;
        for (i = 0; i < sp->pin_count; i++) {
            ns_entity_add_pin(&p->base, sp->pins[i].number, sp->pins[i].name, NS_PIN_IO);
        }
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
    const PassiveSprite *sp;
    int ux;
    int uy;
    if (!p) {
        return;
    }
    ns_passive_draw_ex(r, p->kind, p->base.orient, p->value, screen_pivot_x, screen_pivot_y, 0);
    if (p->kind == NS_PASSIVE_R && (p->leg_ext[0] > 0 || p->leg_ext[1] > 0)) {
        sp = sprite_for(NS_PASSIVE_R);
        r_axis(p, &ux, &uy);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        if (p->leg_ext[0] > 0) {
            SDL_RenderDrawLine(r, screen_pivot_x, screen_pivot_y, screen_pivot_x - ux * p->leg_ext[0],
                               screen_pivot_y - uy * p->leg_ext[0]);
        }
        if (p->leg_ext[1] > 0 && sp->pin_count >= 2) {
            int rx;
            int ry;
            int x0;
            int y0;
            rot_cw_delta(sp->pins[1].x - sp->piv_x, sp->pins[1].y - sp->piv_y, (int)p->base.orient, &rx, &ry);
            x0 = screen_pivot_x + rx;
            y0 = screen_pivot_y + ry;
            SDL_RenderDrawLine(r, x0, y0, x0 + ux * p->leg_ext[1], y0 + uy * p->leg_ext[1]);
        }
    }
    if (selected) {
        int sx = screen_pivot_x + (p->base.board_x - p->pivot_x);
        int sy = screen_pivot_y + (p->base.board_y - p->pivot_y);
        SDL_Rect rc = {sx - 1, sy - 1, p->base.body_w + 2, p->base.body_h + 2};
        SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
        SDL_RenderDrawRect(r, &rc);
    }
}

static int osc4_delta(int pin_num, int *dx, int *dy) {
    const PassiveSprite *sp = sprite_for(NS_PASSIVE_OSC4LEGS);
    const PassivePinDef *pin = pin_by_number(sp, pin_num);
    if (!dx || !dy || !pin) {
        return 0;
    }
    *dx = pin->x - sp->piv_x;
    *dy = pin->y - sp->piv_y;
    return 1;
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

int ns_osc4legs_hit(const NsEntity *e, int bx, int by) {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int p1x;
    int p1y;
    if (!e) {
        return 0;
    }
    osc4_aabb(e->orient, &min_x, &min_y, &max_x, &max_y);
    p1x = e->board_x - min_x;
    p1y = e->board_y - min_y;
    return sprite_rect_hit(sprite_for(NS_PASSIVE_OSC4LEGS), e->orient, p1x, p1y, bx, by);
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

    /* Ordered: XTAL, CCAP, ECAP, R. No diodes on this BOM. */
    add_n(bank, NS_PASSIVE_XTAL, "Y", &y_seq, "8.000MHz", 1);
    add_n(bank, NS_PASSIVE_XTAL, "Y", &y_seq, "21.47727MHz", 1);
    add_n(bank, NS_PASSIVE_XTAL, "Y", &y_seq, "3.579545MHz", 1);

    /* C1-C20 are motherboard bypass. Cart and pad 100 nF stay on those boards. */
    add_n(bank, NS_PASSIVE_CCAP, "C", &c_seq, "100nF", 20);
    c_seq = 22;
    add_n(bank, NS_PASSIVE_CCAP, "C", &c_seq, "22pF", 6);

    add_n(bank, NS_PASSIVE_ECAP, "E", &e_seq, "220uF", 1);

    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "4.00k", 2);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "2.00k", 3);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "1.00k", 3);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "75.0", 3);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "33", 14);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "4.7k", 4);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "10k", 1);
    add_n(bank, NS_PASSIVE_R, "R", &r_seq, "1M", 2);

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
