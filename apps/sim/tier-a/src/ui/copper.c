#include "r01a_ui.h"

#include "r01a_netlist.h"
#include "r01a_rgb_netlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct R01aPinNetSlot {
    NsEntity *entity;
    int pin_index;
} R01aPinNetSlot;

static R01aPinNetSlot g_pin_slots[R01A_PIN_NET_SLOTS];
static int g_pin_parent[R01A_PIN_NET_SLOTS];
static int g_cu_parent[R01A_PIN_NET_SLOTS];
static int g_pin_slot_count;

static int pin_net_find(NsEntity *e, int pin_index) {
    int i;
    for (i = 0; i < g_pin_slot_count; i++) {
        if (g_pin_slots[i].entity == e && g_pin_slots[i].pin_index == pin_index) {
            return i;
        }
    }
    return -1;
}

static int pin_net_add(NsEntity *e, int pin_index) {
    int i = pin_net_find(e, pin_index);
    if (i >= 0) {
        return i;
    }
    if (!e || pin_index < 0 || g_pin_slot_count >= R01A_PIN_NET_SLOTS) {
        return -1;
    }
    i = g_pin_slot_count++;
    g_pin_slots[i].entity = e;
    g_pin_slots[i].pin_index = pin_index;
    g_pin_parent[i] = i;
    return i;
}

static int pin_net_add_name(NsEntity *e, const char *name) {
    const NsPin *pin;
    int idx;
    if (!e || !name) {
        return -1;
    }
    pin = ns_entity_pin_named_const(e, name);
    if (!pin) {
        return -1;
    }
    idx = (int)(pin - e->pins);
    if (idx < 0 || idx >= e->pin_count) {
        return -1;
    }
    return pin_net_add(e, idx);
}

static int pin_net_root(int s) {
    while (s >= 0 && s < g_pin_slot_count && g_pin_parent[s] != s) {
        g_pin_parent[s] = g_pin_parent[g_pin_parent[s]];
        s = g_pin_parent[s];
    }
    return s;
}

static void pin_net_union(int a, int b) {
    int ra = pin_net_root(a);
    int rb = pin_net_root(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        g_pin_parent[rb] = ra;
    }
}

static void pin_net_link(NsEntity *a, const char *an, NsEntity *b, const char *bn) {
    int sa = pin_net_add_name(a, an);
    int sb = pin_net_add_name(b, bn);
    if (sa >= 0 && sb >= 0) {
        pin_net_union(sa, sb);
    }
}

static void pin_net_link_fn(NsEntity *a, const char *an, NsEntity *b, const char *bn) {
    pin_net_link(a, an, b, bn);
}

void pin_net_build(R01aUi *ui) {
    NsEntity *u04 = ui_ent(ui, "U04");
    NsEntity *u74 = ui_ent(ui, "U74");
    NsEntity *y2 = ui_ent(ui, "Y2");
    NsEntity *bx = ui_ent(ui, "UPLDX");
    NsEntity *by = ui_ent(ui, "UPLDY");
    NsEntity *u24 = ui_ent(ui, "U24");
    NsEntity *j2 = ui_ent(ui, "J2");
    NsEntity *r12 = ui_ent(ui, "R12");
    NsEntity *r13 = ui_ent(ui, "R13");
    NsEntity *c6 = ui_ent(ui, "C6");
    NsEntity *c7 = ui_ent(ui, "C7");
    NsEntity *e1 = ui_ent(ui, "E1");
    int i;
    char iname[8];
    char aname[4];

    g_pin_slot_count = 0;
    if (!u04 || !u74 || !y2 || !bx || !by || !u24 || !j2) {
        return;
    }

    pin_net_link(u04, "VCC", u74, "VCC");
    pin_net_link(u04, "VCC", bx, "VCC");
    pin_net_link(u04, "VCC", by, "VCC");
    pin_net_link(u04, "VCC", u24, "VCC");
    pin_net_link(u04, "VCC", u24, "VPP");
    pin_net_link(u04, "VCC", bx, "RES#");
    pin_net_link(u04, "VCC", by, "RES#");
    pin_net_link(u04, "VCC", u24, "PGM#");
    pin_net_link(u74, "VCC", u74, "1PRE#");
    pin_net_link(u74, "VCC", u74, "1CLR#");
    pin_net_link(u74, "VCC", u74, "2PRE#");
    pin_net_link(u74, "VCC", u74, "2CLR#");
    if (e1) {
        pin_net_link(u04, "VCC", e1, "+");
    }
    for (i = 0; i < R01A_BYPASS_N; i++) {
        NsEntity *cap = ui_ent(ui, r01a_bypass_pairs[i].cap);
        NsEntity *ic = ui_ent(ui, r01a_bypass_pairs[i].ic);
        if (cap && ic) {
            pin_net_link(cap, "1", ic, "VCC");
        }
    }

    pin_net_link(u04, "GND", u74, "GND");
    pin_net_link(u04, "GND", bx, "GND");
    pin_net_link(u04, "GND", by, "GND");
    pin_net_link(u04, "GND", u24, "GND");
    pin_net_link(u04, "GND", u24, "CE#");
    pin_net_link(u04, "GND", u24, "OE#");
    pin_net_link(u04, "GND", j2, "GND");
    pin_net_link(u04, "GND", j2, "GND2");
    pin_net_link(u04, "GND", u04, "3A");
    pin_net_link(u04, "GND", u04, "4A");
    pin_net_link(u04, "GND", u04, "5A");
    pin_net_link(u04, "GND", u04, "6A");
    if (e1) {
        pin_net_link(u04, "GND", e1, "-");
    }
    for (i = 0; i < R01A_BYPASS_N; i++) {
        NsEntity *cap = ui_ent(ui, r01a_bypass_pairs[i].cap);
        NsEntity *ic = ui_ent(ui, r01a_bypass_pairs[i].ic);
        if (cap && ic) {
            pin_net_link(cap, "2", ic, "GND");
        }
    }
    if (c6) {
        pin_net_link(u04, "GND", c6, "2");
    }
    if (c7) {
        pin_net_link(u04, "GND", c7, "2");
    }
    for (i = 6; i <= 13; i++) {
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(u04, "GND", u24, aname);
    }

    pin_net_link(u04, "1A", y2, "1");
    if (r13) {
        pin_net_link(u04, "1A", r13, "1");
    }
    if (c6) {
        pin_net_link(u04, "1A", c6, "1");
    }
    pin_net_link(u04, "1Y", y2, "2");
    if (r13) {
        pin_net_link(u04, "1Y", r13, "2");
    }
    if (c7) {
        pin_net_link(u04, "1Y", c7, "1");
    }
    pin_net_link(u04, "1Y", u04, "2A");
    pin_net_link(u04, "2Y", u74, "1CLK");
    pin_net_link(u74, "1/Q", u74, "1D");
    pin_net_link(u74, "1Q", u74, "2CLK");
    pin_net_link(u74, "2/Q", u74, "2D");
    if (r12) {
        pin_net_link(u74, "2Q", r12, "1");
        pin_net_link(r12, "2", bx, "CLK");
    } else {
        pin_net_link(u74, "2Q", bx, "CLK");
    }
    pin_net_link(bx, "HWRAP", by, "CLK");
    pin_net_link(bx, "CSYNC", j2, "CSYNC");
    for (i = 0; i < 6; i++) {
        snprintf(iname, sizeof(iname), "INDEX%d", i);
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(bx, iname, u24, aname);
    }
    r01a_netlist_link_dac_rgbs(pin_net_link_fn, u04, u24, j2, ui_ent(ui, "R1"), ui_ent(ui, "R2"),
                               ui_ent(ui, "R3"), ui_ent(ui, "R4"), ui_ent(ui, "R5"), ui_ent(ui, "R6"),
                               ui_ent(ui, "R7"), ui_ent(ui, "R8"), ui_ent(ui, "R9"), ui_ent(ui, "R10"),
                               ui_ent(ui, "R11"));
}

static int pins_share_net(NsEntity *a, int ap, NsEntity *b, int bp) {
    int sa = pin_net_find(a, ap);
    int sb = pin_net_find(b, bp);
    if (sa < 0 || sb < 0) {
        return 0;
    }
    return pin_net_root(sa) == pin_net_root(sb);
}

static int cu_root(int s) {
    while (s >= 0 && s < g_pin_slot_count && g_cu_parent[s] != s) {
        g_cu_parent[s] = g_cu_parent[g_cu_parent[s]];
        s = g_cu_parent[s];
    }
    return s;
}

static void cu_union(int a, int b) {
    int ra = cu_root(a);
    int rb = cu_root(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        g_cu_parent[rb] = ra;
    }
}

static int slot_at_xy(const R01aUi *ui, int x, int y) {
    int ci = -1;
    int pi = -1;
    if (!hit_pin_at(ui, x, y, &ci, &pi)) {
        return -1;
    }
    return pin_net_find(ui->chips[ci], pi);
}

static int traces_share_point(const R01aTrace *a, const R01aTrace *b) {
    int i;
    int j;
    for (i = 0; i < a->n; i++) {
        for (j = 0; j < b->n; j++) {
            if (a->x[i] == b->x[j] && a->y[i] == b->y[j]) {
                return 1;
            }
        }
    }
    return 0;
}

static int trace_first_slot(const R01aUi *ui, const R01aTrace *t) {
    int i;
    for (i = 0; i < t->n; i++) {
        int s = slot_at_xy(ui, t->x[i], t->y[i]);
        if (s >= 0) {
            return s;
        }
    }
    return -1;
}

void copper_rebuild(const R01aUi *ui) {
    int i;
    int j;
    for (i = 0; i < g_pin_slot_count; i++) {
        g_cu_parent[i] = i;
    }
    for (i = 0; i < ui->trace_n; i++) {
        int first = -1;
        for (j = 0; j < ui->traces[i].n; j++) {
            int s = slot_at_xy(ui, ui->traces[i].x[j], ui->traces[i].y[j]);
            if (s < 0) {
                continue;
            }
            if (first >= 0) {
                cu_union(first, s);
            } else {
                first = s;
            }
        }
    }
    for (i = 0; i < ui->trace_n; i++) {
        int sa = trace_first_slot(ui, &ui->traces[i]);
        if (sa < 0) {
            continue;
        }
        for (j = i + 1; j < ui->trace_n; j++) {
            int sb;
            if (!traces_share_point(&ui->traces[i], &ui->traces[j])) {
                continue;
            }
            sb = trace_first_slot(ui, &ui->traces[j]);
            if (sb >= 0) {
                cu_union(sa, sb);
            }
        }
    }
    /* Package ties (VCC to PRE#, unused inputs to GND, …) are already on the die/leadframe. */
    for (i = 0; i < g_pin_slot_count; i++) {
        for (j = i + 1; j < g_pin_slot_count; j++) {
            if (g_pin_slots[i].entity == g_pin_slots[j].entity && pin_net_root(i) == pin_net_root(j)) {
                cu_union(i, j);
            }
        }
    }
}

static int copper_connected(NsEntity *a, int ap, NsEntity *b, int bp) {
    int sa = pin_net_find(a, ap);
    int sb = pin_net_find(b, bp);
    if (sa < 0 || sb < 0) {
        return 0;
    }
    return cu_root(sa) == cu_root(sb);
}

/* 1 if every schematic net is one copper island (no open airs). */

static int copper_reproduces_netlist(void) {
    int i;
    int seen[R01A_PIN_NET_SLOTS];
    int cu[R01A_PIN_NET_SLOTS];
    if (g_pin_slot_count < 1) {
        return 0;
    }
    for (i = 0; i < g_pin_slot_count; i++) {
        seen[i] = 0;
    }
    for (i = 0; i < g_pin_slot_count; i++) {
        int r = pin_net_root(i);
        int c = cu_root(i);
        if (r < 0 || r >= R01A_PIN_NET_SLOTS) {
            return 0;
        }
        if (!seen[r]) {
            seen[r] = 1;
            cu[r] = c;
        } else if (cu[r] != c) {
            return 0;
        }
    }
    return 1;
}

int screen_picture_live(const R01aUi *ui) {
#if !R01A_COPPER_TRACES
    (void)ui;
    return 1;
#else
    if (!ui->show_nets) {
        return 1;
    }
    return copper_reproduces_netlist();
#endif
}

void nearest_open_partner(const R01aUi *ui, int chip, int pin, int *oc, int *op) {
    NsEntity *src = ui->chips[chip];
    int best = -1;
    int i;
    int pi;
    *oc = -1;
    *op = -1;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int ax;
            int ay;
            int bx;
            int by;
            int d;
            if (e == src) {
                continue;
            }
            if (!pins_share_net(src, pin, e, pi) || copper_connected(src, pin, e, pi)) {
                continue;
            }
            if (!pin_center(src, pin, &ax, &ay) || !pin_center(e, pi, &bx, &by)) {
                continue;
            }
            d = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
            if (best < 0 || d < best) {
                best = d;
                *oc = i;
                *op = pi;
            }
        }
    }
}

static Uint8 air_alpha(void) {
    Uint32 t = SDL_GetTicks() % 1000u;
    if (t < 500u) {
        return (Uint8)(t * 255u / 500u);
    }
    return (Uint8)((1000u - t) * 255u / 500u);
}

static int warn_on(void) {
    return (int)((SDL_GetTicks() / 100u) & 1u);
}

static const char *slot_pin_name(const NsEntity *e, int pi) {
    if (!e || pi < 0 || pi >= e->pin_count) {
        return NULL;
    }
    return e->pins[pi].name;
}

static int name_is_vcc(const char *n) {
    return n && (strcmp(n, "VCC") == 0 || strcmp(n, "VDD") == 0 || strcmp(n, "VPP") == 0 ||
                 strcmp(n, "+") == 0);
}

static int name_is_gnd(const char *n) {
    return n && (strcmp(n, "GND") == 0 || strcmp(n, "GND2") == 0 || strcmp(n, "-") == 0);
}

static int is_bypass_cap(const NsEntity *e) {
    return e && e->refdes && r01a_netlist_bypass_ic(e->refdes) != NULL;
}

static int is_bypass_hop(const NsEntity *a, int ap, const NsEntity *b, int bp) {
    const char *an = slot_pin_name(a, ap);
    const char *bn = slot_pin_name(b, bp);
    if (is_bypass_cap(a)) {
        if (an && strcmp(an, "1") == 0 && name_is_vcc(bn)) {
            return 1;
        }
        if (an && strcmp(an, "2") == 0 && name_is_gnd(bn)) {
            return 1;
        }
    }
    if (is_bypass_cap(b)) {
        if (bn && strcmp(bn, "1") == 0 && name_is_vcc(an)) {
            return 1;
        }
        if (bn && strcmp(bn, "2") == 0 && name_is_gnd(an)) {
            return 1;
        }
    }
    return 0;
}

static int is_pierce_part(const NsEntity *e) {
    const char *r = e ? e->refdes : NULL;
    return r && (strcmp(r, "Y2") == 0 || strcmp(r, "C6") == 0 || strcmp(r, "C7") == 0 ||
                 strcmp(r, "R13") == 0 || strcmp(r, "U04") == 0);
}

static int is_pierce_pin(const NsEntity *e, int pi) {
    const char *n = slot_pin_name(e, pi);
    const char *r = e ? e->refdes : NULL;
    if (!n || !is_pierce_part(e)) {
        return 0;
    }
    if (strcmp(r, "U04") == 0) {
        return strcmp(n, "1A") == 0 || strcmp(n, "1Y") == 0 || strcmp(n, "2A") == 0;
    }
    if ((strcmp(r, "C6") == 0 || strcmp(r, "C7") == 0) && strcmp(n, "2") == 0) {
        return 0;
    }
    return 1;
}

static int is_clk_pin(const NsEntity *e, int pi) {
    const char *n = slot_pin_name(e, pi);
    const char *r = e ? e->refdes : NULL;
    if (r && strcmp(r, "R12") == 0) {
        return 1;
    }
    return n && (strcmp(n, "CLK") == 0 || strcmp(n, "DOT") == 0 || strcmp(n, "CSYNC") == 0 ||
                 strcmp(n, "HWRAP") == 0 || strcmp(n, "1CLK") == 0 || strcmp(n, "2CLK") == 0 ||
                 strcmp(n, "1Q") == 0 || strcmp(n, "2Q") == 0 || strcmp(n, "2Y") == 0);
}

static int is_video_pin(const NsEntity *e, int pi) {
    const char *n = slot_pin_name(e, pi);
    const char *r = e ? e->refdes : NULL;
    int rn;
    if (!n) {
        return 0;
    }
    if (r && strcmp(r, "J2") == 0 &&
        (strcmp(n, "R") == 0 || strcmp(n, "G") == 0 || strcmp(n, "B") == 0)) {
        return 1;
    }
    if (!r || r[0] != 'R') {
        return 0;
    }
    rn = atoi(r + 1);
    return rn >= 1 && rn <= 11;
}

static int hop_limit_mm(const NsEntity *a, int ap, const NsEntity *b, int bp) {
    if (is_bypass_hop(a, ap, b, bp)) {
        return R01A_HOP_BYPASS_MM;
    }
    if (is_pierce_pin(a, ap) && is_pierce_pin(b, bp)) {
        return R01A_HOP_FAST_MM;
    }
    if (is_clk_pin(a, ap) && is_clk_pin(b, bp)) {
        return R01A_HOP_FAST_MM;
    }
    if (is_video_pin(a, ap) && is_video_pin(b, bp)) {
        return R01A_HOP_FAST_MM;
    }
    return 0;
}

static int hop_too_long_xy(int x0, int y0, int x1, int y1, int limit_mm) {
    long dx;
    long dy;
    long lim;
    if (limit_mm <= 0) {
        return 0;
    }
    dx = (long)x0 - x1;
    dy = (long)y0 - y1;
    lim = (long)limit_mm * (long)NS_PX_PER_MM;
    return dx * dx + dy * dy > lim * lim;
}

static int hop_too_long(const NsEntity *a, int ap, const NsEntity *b, int bp) {
    int ax;
    int ay;
    int bx;
    int by;
    int lim = hop_limit_mm(a, ap, b, bp);
    if (lim <= 0 || !pin_center(a, ap, &ax, &ay) || !pin_center(b, bp, &bx, &by)) {
        return 0;
    }
    return hop_too_long_xy(ax, ay, bx, by, lim);
}

static int seg_len_px(int x0, int y0, int x1, int y1) {
    int dx = x0 - x1;
    int dy = y0 - y1;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}

static int pin_tight_limit_mm(const NsEntity *e, int pi) {
    if (is_bypass_cap(e)) {
        return R01A_HOP_BYPASS_MM;
    }
    if (is_pierce_pin(e, pi) || is_clk_pin(e, pi) || is_video_pin(e, pi)) {
        return R01A_HOP_FAST_MM;
    }
    return 0;
}

static int trace_too_long(const R01aUi *ui, const R01aTrace *t) {
    int slots[8];
    int n = 0;
    int i;
    int a;
    int b;
    int len_px = 0;
    for (i = 0; i + 1 < t->n; i++) {
        len_px += seg_len_px(t->x[i], t->y[i], t->x[i + 1], t->y[i + 1]);
    }
    for (i = 0; i < t->n && n < 8; i++) {
        int s = slot_at_xy(ui, t->x[i], t->y[i]);
        int k;
        int dup = 0;
        if (s < 0) {
            continue;
        }
        for (k = 0; k < n; k++) {
            if (slots[k] == s) {
                dup = 1;
            }
        }
        if (!dup) {
            slots[n++] = s;
        }
    }
    if (n == 1) {
        int lim = pin_tight_limit_mm(g_pin_slots[slots[0]].entity, g_pin_slots[slots[0]].pin_index);
        return lim > 0 && len_px > lim * NS_PX_PER_MM;
    }
    for (a = 0; a < n; a++) {
        for (b = a + 1; b < n; b++) {
            int lim = hop_limit_mm(g_pin_slots[slots[a]].entity, g_pin_slots[slots[a]].pin_index,
                                   g_pin_slots[slots[b]].entity, g_pin_slots[slots[b]].pin_index);
            if (lim > 0 && (len_px > lim * NS_PX_PER_MM ||
                            hop_too_long(g_pin_slots[slots[a]].entity, g_pin_slots[slots[a]].pin_index,
                                         g_pin_slots[slots[b]].entity, g_pin_slots[slots[b]].pin_index))) {
                return 1;
            }
        }
    }
    return 0;
}

void draw_air_wires(SDL_Renderer *r, const R01aUi *ui) {
    int i;
    int j;
    Uint8 a = air_alpha();
    for (i = 0; i < g_pin_slot_count; i++) {
        NsEntity *ea = g_pin_slots[i].entity;
        int pa = g_pin_slots[i].pin_index;
        int ax;
        int ay;
        int best = -1;
        int best_j = -1;
        int bx = 0;
        int by = 0;
        if (!pin_center(ea, pa, &ax, &ay)) {
            continue;
        }
        for (j = i + 1; j < g_pin_slot_count; j++) {
            NsEntity *eb = g_pin_slots[j].entity;
            int pb = g_pin_slots[j].pin_index;
            int tx;
            int ty;
            int d;
            if (ea == eb || pin_net_root(i) != pin_net_root(j) || copper_connected(ea, pa, eb, pb)) {
                continue;
            }
            if (!pin_center(eb, pb, &tx, &ty)) {
                continue;
            }
            d = (ax - tx) * (ax - tx) + (ay - ty) * (ay - ty);
            if (best < 0 || d < best) {
                best = d;
                best_j = j;
                bx = tx;
                by = ty;
            }
        }
        if (best_j >= 0) {
            NsEntity *eb = g_pin_slots[best_j].entity;
            int pb = g_pin_slots[best_j].pin_index;
            int x0 = board_sx(ui, ax);
            int y0 = board_sy(ui, ay);
            int x1 = board_sx(ui, bx);
            int y1 = board_sy(ui, by);
            int warn = hop_too_long(ea, pa, eb, pb);
            Uint8 cr = warn ? 220 : 40;
            Uint8 cg = warn ? 40 : 220;
            Uint8 cb = warn ? 40 : 80;
            Uint8 ca = warn || ui->air_hard ? 255 : a;
            if (warn && !warn_on()) {
                continue;
            }
            if (ui->air_hard) {
                draw_hard_line(r, x0, y0, x1, y1, cr, cg, cb);
            } else {
                draw_soft_line(r, x0, y0, x1, y1, cr, cg, cb, ca);
            }
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            plot_a(r, x0, y0, cr, cg, cb, ca);
            plot_a(r, x1, y1, cr, cg, cb, ca);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
    }
}

void draw_traces(SDL_Renderer *r, const R01aUi *ui) {
    int i;
    int s;
    for (i = 0; i < ui->trace_n; i++) {
        const R01aTrace *t = &ui->traces[i];
        int warn = trace_too_long(ui, t);
        Uint8 cr = 20;
        Uint8 cg = 200;
        Uint8 cb = 70;
        if (warn) {
            if (!warn_on()) {
                continue;
            }
            cr = 220;
            cg = 40;
            cb = 40;
        }
        for (s = 0; s + 1 < t->n; s++) {
            draw_hard_line(r, board_sx(ui, t->x[s]), board_sy(ui, t->y[s]), board_sx(ui, t->x[s + 1]),
                           board_sy(ui, t->y[s + 1]), cr, cg, cb);
        }
    }
}

void draw_arm(SDL_Renderer *r, const R01aUi *ui, int mx, int my) {
    int s;
    int lx;
    int ly;
    int sx;
    int sy;
    Uint32 now;
    if (!ui->arm || ui->arm_n < 1) {
        return;
    }
    for (s = 0; s + 1 < ui->arm_n; s++) {
        draw_hard_line(r, board_sx(ui, ui->arm_x[s]), board_sy(ui, ui->arm_y[s]),
                       board_sx(ui, ui->arm_x[s + 1]), board_sy(ui, ui->arm_y[s + 1]), 20, 200, 70);
    }
    lx = ui->arm_x[ui->arm_n - 1];
    ly = ui->arm_y[ui->arm_n - 1];
    snap_octant(lx, ly, snap_pin(mx), snap_pin(my), &sx, &sy);
    sx = snap_pin(sx);
    sy = snap_pin(sy);
    draw_hard_line(r, board_sx(ui, lx), board_sy(ui, ly), board_sx(ui, sx), board_sy(ui, sy), 40, 140, 50);
    if (ui->dest_chip >= 0 && ui->dest_pin >= 0) {
        int dx;
        int dy;
        if (pin_center(ui->chips[ui->dest_chip], ui->dest_pin, &dx, &dy)) {
            now = SDL_GetTicks();
            draw_ants_line(r, board_sx(ui, sx), board_sy(ui, sy), board_sx(ui, dx), board_sy(ui, dy), now);
        }
    }
}

void arm_begin(R01aUi *ui, int chip, int pin) {
    int cx;
    int cy;
    if (!pin_center(ui->chips[chip], pin, &cx, &cy)) {
        return;
    }
    ui->arm = 1;
    ui->arm_n = 1;
    ui->arm_x[0] = (int16_t)cx;
    ui->arm_y[0] = (int16_t)cy;
    nearest_open_partner(ui, chip, pin, &ui->dest_chip, &ui->dest_pin);
}

void arm_cancel(R01aUi *ui) {
    ui->arm = 0;
    ui->arm_n = 0;
    ui->dest_chip = -1;
    ui->dest_pin = -1;
}

void arm_commit(R01aUi *ui) {
    R01aTrace *t;
    int i;
    if (!ui->arm || ui->arm_n < 2 || ui->trace_n >= R01A_TRACE_MAX) {
        arm_cancel(ui);
        return;
    }
    t = &ui->traces[ui->trace_n++];
    t->n = ui->arm_n;
    for (i = 0; i < ui->arm_n; i++) {
        t->x[i] = (int16_t)ui->arm_x[i];
        t->y[i] = (int16_t)ui->arm_y[i];
    }
    arm_cancel(ui);
    hist_after(ui);
}

void arm_add_point(R01aUi *ui, int x, int y) {
    int lx;
    int ly;
    int sx;
    int sy;
    if (!ui->arm || ui->arm_n < 1 || ui->arm_n >= R01A_TRACE_PTS) {
        return;
    }
    lx = ui->arm_x[ui->arm_n - 1];
    ly = ui->arm_y[ui->arm_n - 1];
    snap_octant(lx, ly, snap_pin(x), snap_pin(y), &sx, &sy);
    sx = snap_pin(sx);
    sy = snap_pin(sy);
    if (sx == lx && sy == ly) {
        return;
    }
    ui->arm_x[ui->arm_n] = sx;
    ui->arm_y[ui->arm_n] = sy;
    ui->arm_n++;
}

void arm_add_to_pad(R01aUi *ui, int px, int py) {
    int lx;
    int ly;
    if (!ui->arm || ui->arm_n < 1) {
        return;
    }
    arm_add_point(ui, px, py);
    lx = ui->arm_x[ui->arm_n - 1];
    ly = ui->arm_y[ui->arm_n - 1];
    if (lx == px && ly == py) {
        return;
    }
    if (ui->arm_n >= R01A_TRACE_PTS) {
        return;
    }
    if (lx != px && ly != py) {
        ui->arm_x[ui->arm_n] = px;
        ui->arm_y[ui->arm_n] = ly;
        ui->arm_n++;
        if (ui->arm_n >= R01A_TRACE_PTS) {
            return;
        }
    }
    ui->arm_x[ui->arm_n] = px;
    ui->arm_y[ui->arm_n] = py;
    ui->arm_n++;
}
