#include "netlist_sim/passive.h"

#include "netlist_sim/ui_passive_assets.h"

#include <stdio.h>
#include <string.h>

typedef struct PassiveSprite {
    const uint8_t *rgba;
    int w;
    int h;
    int piv_x;
    int piv_y;
    int span_px;
} PassiveSprite;

static const PassiveSprite k_sprites[NS_PASSIVE_KIND_COUNT] = {
    {NS_UI_PASSIVE_R_RGBA, NS_UI_PASSIVE_R_W, NS_UI_PASSIVE_R_H, NS_UI_PASSIVE_R_PIV_X,
     NS_UI_PASSIVE_R_PIV_Y, NS_UI_PASSIVE_R_SPAN_PX},
    {NS_UI_PASSIVE_CCAP_RGBA, NS_UI_PASSIVE_CCAP_W, NS_UI_PASSIVE_CCAP_H, NS_UI_PASSIVE_CCAP_PIV_X,
     NS_UI_PASSIVE_CCAP_PIV_Y, NS_UI_PASSIVE_CCAP_SPAN_PX},
    {NS_UI_PASSIVE_ECAP_RGBA, NS_UI_PASSIVE_ECAP_W, NS_UI_PASSIVE_ECAP_H, NS_UI_PASSIVE_ECAP_PIV_X,
     NS_UI_PASSIVE_ECAP_PIV_Y, NS_UI_PASSIVE_ECAP_SPAN_PX},
    {NS_UI_PASSIVE_OSC_RGBA, NS_UI_PASSIVE_OSC_W, NS_UI_PASSIVE_OSC_H, NS_UI_PASSIVE_OSC_PIV_X,
     NS_UI_PASSIVE_OSC_PIV_Y, NS_UI_PASSIVE_OSC_SPAN_PX},
    {NS_UI_PASSIVE_D_RGBA, NS_UI_PASSIVE_D_W, NS_UI_PASSIVE_D_H, NS_UI_PASSIVE_D_PIV_X,
     NS_UI_PASSIVE_D_PIV_Y, NS_UI_PASSIVE_D_SPAN_PX},
};

static const NsEntityVTable k_passive_vt = {NULL, NULL, NULL, NULL};

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
    int rx, ry;
    if (!p || !wx || !wy || pin_num < 1 || pin_num > 2) {
        return 0;
    }
    if (pin_num == 1) {
        *wx = p->pivot_x;
        *wy = p->pivot_y;
        return 1;
    }
    sp = sprite_for(p->kind);
    rot_cw_delta(sp->span_px, 0, (int)p->base.orient, &rx, &ry);
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
    /* Two logical leads for snap (no electrical levels yet). */
    ns_entity_add_pin(&p->base, 1, "1", NS_PIN_IO);
    ns_entity_add_pin(&p->base, 2, "2", NS_PIN_IO);
    ns_passive_set_pivot(p, 0, 0);
    return p;
}

void ns_passive_draw(SDL_Renderer *r, const NsPassive *p, int screen_pivot_x, int screen_pivot_y,
                       int selected) {
    const PassiveSprite *sp;
    int sx, sy;
    int min_x, min_y, max_x, max_y;
    if (!r || !p) {
        return;
    }
    sp = sprite_for(p->kind);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (sy = 0; sy < sp->h; sy++) {
        for (sx = 0; sx < sp->w; sx++) {
            const uint8_t *px = sp->rgba + ((size_t)sy * (size_t)sp->w + (size_t)sx) * 4u;
            int dx, dy, rx, ry;
            if (px[3] == 0) {
                continue;
            }
            dx = sx - sp->piv_x;
            dy = sy - sp->piv_y;
            rot_cw_delta(dx, dy, (int)p->base.orient, &rx, &ry);
            SDL_SetRenderDrawColor(r, px[0], px[1], px[2], px[3]);
            SDL_RenderDrawPoint(r, screen_pivot_x + rx, screen_pivot_y + ry);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    if (selected) {
        passive_aabb_about_pivot(sp, p->base.orient, &min_x, &min_y, &max_x, &max_y);
        SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
        {
            SDL_Rect rc = {screen_pivot_x + min_x - 1, screen_pivot_y + min_y - 1, max_x - min_x + 3,
                           max_y - min_y + 3};
            SDL_RenderDrawRect(r, &rc);
        }
    }
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

    /* Ordered: OSC, CCAP, ECAP, R . No diodes on this BOM. */
    add_n(bank, NS_PASSIVE_OSC, "Y", &y_seq, "8.000MHz", 1);
    add_n(bank, NS_PASSIVE_OSC, "Y", &y_seq, "5.369318MHz", 1);
    add_n(bank, NS_PASSIVE_OSC, "Y", &y_seq, "14.31818MHz", 1);

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
