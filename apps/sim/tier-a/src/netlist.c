#include "r01a_netlist.h"
#include "r01a_rgb_netlist.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/entity.h"
#include "discrete_ic/island.h"
#include "discrete_ic/island_group.h"
#include "discrete_ic/passive.h"

#include <stdio.h>
#include <string.h>

#define R01A_NL_SLOTS 512
#define R01A_NL_BB_MAX (R01A_BB_EXTRA_MAX + 1)
#define R01A_NL_GNET (R01A_NL_BB_MAX * NS_PB_STRIPS)

typedef struct NlSlot {
    NsEntity *e;
    int pi;
} NlSlot;

static NlSlot g_slots[R01A_NL_SLOTS];
static int g_parent[R01A_NL_SLOTS];
static int g_nslot;

static int pin_find(NsEntity *e, int pi) {
    int i;
    for (i = 0; i < g_nslot; i++) {
        if (g_slots[i].e == e && g_slots[i].pi == pi) {
            return i;
        }
    }
    return -1;
}

static int pin_add(NsEntity *e, int pi) {
    int i = pin_find(e, pi);
    if (i >= 0) {
        return i;
    }
    if (!e || pi < 0 || pi >= e->pin_count || g_nslot >= R01A_NL_SLOTS) {
        return -1;
    }
    i = g_nslot++;
    g_slots[i].e = e;
    g_slots[i].pi = pi;
    g_parent[i] = i;
    return i;
}

static int pin_root(int s) {
    while (s >= 0 && s < g_nslot && g_parent[s] != s) {
        g_parent[s] = g_parent[g_parent[s]];
        s = g_parent[s];
    }
    return s;
}

static void pin_union(int a, int b) {
    int ra = pin_root(a);
    int rb = pin_root(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        g_parent[rb] = ra;
    }
}

static int pin_add_name(NsEntity *e, const char *name) {
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
    return pin_add(e, idx);
}

static void link_n(NsEntity *a, const char *an, NsEntity *b, const char *bn) {
    int sa = pin_add_name(a, an);
    int sb = pin_add_name(b, bn);
    if (sa >= 0 && sb >= 0) {
        pin_union(sa, sb);
    }
}

static NsEntity *ent(R01aBoard *b, const char *ref) {
    return r01a_board_entity_by_refdes(b, ref);
}

static void build_auto(R01aBoard *b) {
    NsEntity *y2 = ent(b, "Y2");
    NsEntity *bx = ent(b, "UPLDX");
    NsEntity *by = ent(b, "UPLDY");
    NsEntity *u24 = ent(b, "U24");
    NsEntity *j2 = ent(b, "J2");
    int i;
    char iname[8];
    char aname[4];
    g_nslot = 0;
    if (!y2 || !bx || !by || !u24 || !j2) {
        return;
    }
    link_n(y2, "VDD", y2, "OE#");
    link_n(y2, "VDD", bx, "VCC");
    link_n(y2, "VDD", by, "VCC");
    link_n(y2, "VDD", u24, "VCC");
    link_n(y2, "VDD", u24, "VPP");
    link_n(y2, "VDD", bx, "RES#");
    link_n(y2, "VDD", by, "RES#");
    link_n(y2, "VDD", u24, "PGM#");
    link_n(y2, "GND", bx, "GND");
    link_n(y2, "GND", by, "GND");
    link_n(y2, "GND", u24, "GND");
    link_n(y2, "GND", u24, "CE#");
    link_n(y2, "GND", u24, "OE#");
    link_n(y2, "GND", j2, "GND");
    link_n(y2, "GND", j2, "GND2");
    link_n(y2, "DOT", ent(b, "R12"), "1");
    link_n(ent(b, "R12"), "2", bx, "CLK");
    link_n(bx, "HWRAP", by, "CLK");
    link_n(bx, "CSYNC", j2, "CSYNC");
    for (i = 0; i < 6; i++) {
        snprintf(iname, sizeof(iname), "INDEX%d", i);
        snprintf(aname, sizeof(aname), "A%d", i);
        link_n(bx, iname, u24, aname);
    }
    for (i = 6; i <= 13; i++) {
        snprintf(aname, sizeof(aname), "A%d", i);
        link_n(y2, "GND", u24, aname);
    }
    r01a_netlist_link_dac_rgbs((R01aNetLinkFn)link_n, y2, u24, j2, ent(b, "R1"), ent(b, "R2"),
                                 ent(b, "R3"), ent(b, "R4"), ent(b, "R5"), ent(b, "R6"),
                                 ent(b, "R7"), ent(b, "R8"), ent(b, "R9"), ent(b, "R10"),
                                 ent(b, "R11"));
}

static int name_gnd(const char *n) {
    return n && (strcmp(n, "GND") == 0 || strcmp(n, "AGND") == 0 || strcmp(n, "DGND") == 0);
}

static int name_vdd(const char *n) {
    return n && (strcmp(n, "VDD") == 0 || strcmp(n, "VCC") == 0 || strcmp(n, "APOS") == 0 ||
                 strcmp(n, "DPOS") == 0);
}

static int name_clk(const char *n) {
    return n && (strcmp(n, "DOT") == 0 || strcmp(n, "CLK") == 0 || strcmp(n, "CSYNC") == 0 ||
                 strcmp(n, "HWRAP") == 0);
}

static int auto_kind(int slot) {
    int root = pin_root(slot);
    int i;
    int saw_clk = 0;
    for (i = 0; i < g_nslot; i++) {
        const char *n;
        if (pin_root(i) != root) {
            continue;
        }
        n = g_slots[i].e->pins[g_slots[i].pi].name;
        if (name_gnd(n)) {
            return R01A_NET_KIND_GND;
        }
        if (name_vdd(n)) {
            return R01A_NET_KIND_PWR;
        }
        if (name_clk(n)) {
            saw_clk = 1;
        }
    }
    return saw_clk ? R01A_NET_KIND_CLK : R01A_NET_KIND_DATA;
}

static int entity_tip(const NsEntity *e, int pin_num, int *tx, int *ty) {
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return ns_passive_tip_board((const NsPassive *)e, pin_num, tx, ty);
    }
    return ns_entity_pin_tip_board(e, pin_num, tx, ty);
}

static int sfind(int *p, int s) {
    if (s < 0 || s >= NS_PB_STRIPS) {
        return s;
    }
    while (p[s] != s) {
        p[s] = p[p[s]];
        s = p[s];
    }
    return s;
}

static void sunion(int *p, int a, int b) {
    int ra = sfind(p, a);
    int rb = sfind(p, b);
    if (ra != rb && ra >= 0 && ra < NS_PB_STRIPS) {
        p[rb] = ra;
    }
}

static int gfind(int *p, int s) {
    if (s < 0 || s >= R01A_NL_GNET) {
        return s;
    }
    while (p[s] != s) {
        p[s] = p[p[s]];
        s = p[s];
    }
    return s;
}

static void gunion(int *p, int a, int b) {
    int ra = gfind(p, a);
    int rb = gfind(p, b);
    if (ra != rb && ra >= 0 && ra < R01A_NL_GNET) {
        p[rb] = ra;
    }
}

static int bb_index(NsBreadboard **bbs, int n, const char *ref) {
    int i;
    if (!ref || !ref[0]) {
        ref = "BB1";
    }
    for (i = 0; i < n; i++) {
        if (bbs[i] && bbs[i]->base.refdes && strcmp(bbs[i]->base.refdes, ref) == 0) {
            return i;
        }
    }
    return -1;
}

static int jumper_intra(const R01aJumper *j, const NsBreadboard *bb) {
    return bb && bb->base.refdes && strcmp(r01a_jumper_a_ref(j), bb->base.refdes) == 0 &&
           strcmp(r01a_jumper_b_ref(j), bb->base.refdes) == 0;
}

static void gnet_build(int *parent, R01aBoard *board, NsBreadboard **bbs, int nbb) {
    int i;
    int s;
    int local[NS_PB_STRIPS];
    int n = nbb * NS_PB_STRIPS;
    for (i = 0; i < n && i < R01A_NL_GNET; i++) {
        parent[i] = i;
    }
    for (i = 0; i < nbb; i++) {
        for (s = 0; s < NS_PB_STRIPS; s++) {
            local[s] = s;
        }
        for (s = 0; s < board->jumper_count; s++) {
            if (!jumper_intra(&board->jumpers[s], bbs[i])) {
                continue;
            }
            sunion(local, ns_breadboard_strip_id(board->jumpers[s].a),
                   ns_breadboard_strip_id(board->jumpers[s].b));
        }
        for (s = 0; s < NS_PB_STRIPS; s++) {
            gunion(parent, i * NS_PB_STRIPS + s, i * NS_PB_STRIPS + sfind(local, s));
        }
    }
    for (i = 0; i < board->jumper_count; i++) {
        int ia = bb_index(bbs, nbb, r01a_jumper_a_ref(&board->jumpers[i]));
        int ib = bb_index(bbs, nbb, r01a_jumper_b_ref(&board->jumpers[i]));
        int sa;
        int sb;
        if (ia < 0 || ib < 0) {
            continue;
        }
        sa = ns_breadboard_strip_id(board->jumpers[i].a);
        sb = ns_breadboard_strip_id(board->jumpers[i].b);
        if (sa < 0 || sa >= NS_PB_STRIPS || sb < 0 || sb >= NS_PB_STRIPS) {
            continue;
        }
        gunion(parent, ia * NS_PB_STRIPS + sa, ib * NS_PB_STRIPS + sb);
    }
}

static int pin_on_strip(NsEntity *e, int pi, NsBreadboard **bbs, int nbb, int *bi, int *strip) {
    int tx;
    int ty;
    int i;
    if (!entity_tip(e, e->pins[pi].number, &tx, &ty)) {
        return 0;
    }
    for (i = 0; i < nbb; i++) {
        int s;
        if (ns_breadboard_tip_strip(bbs[i], tx, ty, &s)) {
            *bi = i;
            *strip = s;
            return 1;
        }
    }
    return 0;
}

static const char *pin_label(int slot, char *buf, size_t n) {
    const NsEntity *e;
    const char *ref;
    const char *pn;
    if (slot < 0 || slot >= g_nslot) {
        snprintf(buf, n, "?");
        return buf;
    }
    e = g_slots[slot].e;
    ref = (e && e->refdes) ? e->refdes : "?";
    pn = e->pins[g_slots[slot].pi].name;
    snprintf(buf, n, "%s.%s", ref, pn ? pn : "?");
    return buf;
}

static int add_issue(R01aNetIssue *out, int max_out, int *n, int kind, const char *text) {
    int i;
    if (!out || !n || max_out <= 0) {
        return 0;
    }
    for (i = 0; i < *n; i++) {
        if (out[i].kind == kind && strcmp(out[i].text, text) == 0) {
            return 1;
        }
    }
    if (*n >= max_out) {
        return 0;
    }
    out[*n].kind = kind;
    snprintf(out[*n].text, sizeof(out[*n].text), "%s", text);
    (*n)++;
    return 1;
}

static int short_kind(int a, int b) {
    int lo = a < b ? a : b;
    int hi = a < b ? b : a;
    if (lo == R01A_NET_KIND_PWR && hi == R01A_NET_KIND_GND) {
        return R01A_NET_SHORT_PWR_GND;
    }
    if (lo == R01A_NET_KIND_GND && hi == R01A_NET_KIND_DATA) {
        return R01A_NET_SHORT_DATA_GND;
    }
    if (lo == R01A_NET_KIND_PWR && hi == R01A_NET_KIND_DATA) {
        return R01A_NET_SHORT_DATA_PWR;
    }
    if (lo == R01A_NET_KIND_CLK && hi == R01A_NET_KIND_GND) {
        return R01A_NET_SHORT_CLK_GND;
    }
    if (lo == R01A_NET_KIND_PWR && hi == R01A_NET_KIND_CLK) {
        return R01A_NET_SHORT_CLK_PWR;
    }
    if (lo == R01A_NET_KIND_CLK && hi == R01A_NET_KIND_DATA) {
        return R01A_NET_SHORT_CLK_DATA;
    }
    if (lo == R01A_NET_KIND_DATA && hi == R01A_NET_KIND_DATA) {
        return R01A_NET_SHORT_DATA;
    }
    return R01A_NET_SHORT_CHIP;
}

const char *r01a_netlist_kind_label(int kind) {
    switch (kind) {
    case R01A_NET_UNSEATED:
        return "unseated";
    case R01A_NET_MISSING:
        return "open";
    case R01A_NET_SHORT_PWR_GND:
        return "short 5V-GND";
    case R01A_NET_SHORT_DATA_GND:
        return "short data-GND";
    case R01A_NET_SHORT_DATA_PWR:
        return "short data-5V";
    case R01A_NET_SHORT_CLK_GND:
        return "short clk-GND";
    case R01A_NET_SHORT_CLK_PWR:
        return "short clk-5V";
    case R01A_NET_SHORT_CLK_DATA:
        return "short clk-data";
    case R01A_NET_SHORT_DATA:
        return "short data-data";
    case R01A_NET_SHORT_CHIP:
        return "short";
    default:
        return "ok";
    }
}

int r01a_netlist_check(R01aBoard *board, R01aNetIssue *out, int max_out) {
    NsIsland *island;
    NsBreadboard *bbs[R01A_NL_BB_MAX];
    int parent[R01A_NL_GNET];
    int pin_g[R01A_NL_SLOTS];
    int seated[R01A_NL_SLOTS];
    int nbb = 0;
    int nout = 0;
    int i;
    int bb1 = -1;
    int pwr_g[2];
    int gnd_g[2];
    int seen_root[R01A_NL_SLOTS];

    if (!board) {
        return 0;
    }
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return 0;
    }
    for (i = 0; i < island->entity_count && nbb < R01A_NL_BB_MAX; i++) {
        NsEntity *e = island->entities[i];
        if (e && e->visual == NS_ENTITY_VIS_BREADBOARD) {
            bbs[nbb] = (NsBreadboard *)e;
            if (e->refdes && strcmp(e->refdes, "BB1") == 0) {
                bb1 = nbb;
            }
            nbb++;
        }
    }
    build_auto(board);
    gnet_build(parent, board, bbs, nbb);
    pwr_g[0] = pwr_g[1] = -1;
    gnd_g[0] = gnd_g[1] = -1;
    if (bb1 >= 0) {
        int k;
        for (k = 0; k < 2; k++) {
            NsPbHole hp = {0, k ? NS_PB_LANE_BOT_POS : NS_PB_LANE_TOP_POS};
            NsPbHole hn = {0, k ? NS_PB_LANE_BOT_NEG : NS_PB_LANE_TOP_NEG};
            pwr_g[k] = gfind(parent, bb1 * NS_PB_STRIPS + ns_breadboard_strip_id(hp));
            gnd_g[k] = gfind(parent, bb1 * NS_PB_STRIPS + ns_breadboard_strip_id(hn));
        }
    }
    for (i = 0; i < g_nslot; i++) {
        int bi;
        int st;
        seated[i] = 0;
        pin_g[i] = -1;
        if (pin_on_strip(g_slots[i].e, g_slots[i].pi, bbs, nbb, &bi, &st)) {
            seated[i] = 1;
            pin_g[i] = gfind(parent, bi * NS_PB_STRIPS + st);
        }
    }

    /* Hard shorts: one strip/jumper island holds two Auto nets, or a signal on a rail. */
    for (i = 0; i < g_nslot; i++) {
        int j;
        if (!seated[i]) {
            continue;
        }
        for (j = i + 1; j < g_nslot; j++) {
            int ki;
            int kj;
            char la[32];
            char lb[32];
            char line[R01A_NET_TEXT_LEN];
            int sk;
            if (!seated[j] || pin_g[i] != pin_g[j]) {
                continue;
            }
            if (pin_root(i) == pin_root(j)) {
                continue;
            }
            if (board->wire_mode == R01A_WIRE_MANUAL && g_slots[i].e == g_slots[j].e) {
                continue;
            }
            ki = auto_kind(i);
            kj = auto_kind(j);
            if (ki == kj && ki != R01A_NET_KIND_DATA && ki != R01A_NET_KIND_CLK) {
                continue;
            }
            if (ki == kj && ki == R01A_NET_KIND_CLK && pin_root(i) == pin_root(j)) {
                continue;
            }
            sk = short_kind(ki, kj);
            if (g_slots[i].e == g_slots[j].e && ki != kj) {
                sk = (sk == R01A_NET_SHORT_DATA) ? R01A_NET_SHORT_CHIP : sk;
            }
            pin_label(i, la, sizeof(la));
            pin_label(j, lb, sizeof(lb));
            snprintf(line, sizeof(line), "%s: %s %s", r01a_netlist_kind_label(sk), la, lb);
            add_issue(out, max_out, &nout, sk, line);
        }
    }
    for (i = 0; i < g_nslot; i++) {
        int k;
        int g;
        int on_pwr;
        int on_gnd;
        char la[32];
        char line[R01A_NET_TEXT_LEN];
        int sk;
        if (!seated[i]) {
            continue;
        }
        k = auto_kind(i);
        g = pin_g[i];
        on_pwr = (g == pwr_g[0] || g == pwr_g[1]);
        on_gnd = (g == gnd_g[0] || g == gnd_g[1]);
        if (k == R01A_NET_KIND_PWR && on_gnd) {
            sk = R01A_NET_SHORT_PWR_GND;
            pin_label(i, la, sizeof(la));
            snprintf(line, sizeof(line), "%s: %s on BB1 GND", r01a_netlist_kind_label(sk), la);
            add_issue(out, max_out, &nout, sk, line);
        }
        if (k == R01A_NET_KIND_GND && on_pwr) {
            sk = R01A_NET_SHORT_PWR_GND;
            pin_label(i, la, sizeof(la));
            snprintf(line, sizeof(line), "%s: %s on BB1 5V", r01a_netlist_kind_label(sk), la);
            add_issue(out, max_out, &nout, sk, line);
        }
        if (k == R01A_NET_KIND_DATA && on_gnd) {
            sk = R01A_NET_SHORT_DATA_GND;
            pin_label(i, la, sizeof(la));
            snprintf(line, sizeof(line), "%s: %s on BB1 GND", r01a_netlist_kind_label(sk), la);
            add_issue(out, max_out, &nout, sk, line);
        }
        if (k == R01A_NET_KIND_DATA && on_pwr) {
            sk = R01A_NET_SHORT_DATA_PWR;
            pin_label(i, la, sizeof(la));
            snprintf(line, sizeof(line), "%s: %s on BB1 5V", r01a_netlist_kind_label(sk), la);
            add_issue(out, max_out, &nout, sk, line);
        }
        if (k == R01A_NET_KIND_CLK && on_gnd) {
            sk = R01A_NET_SHORT_CLK_GND;
            pin_label(i, la, sizeof(la));
            snprintf(line, sizeof(line), "%s: %s on BB1 GND", r01a_netlist_kind_label(sk), la);
            add_issue(out, max_out, &nout, sk, line);
        }
        if (k == R01A_NET_KIND_CLK && on_pwr) {
            sk = R01A_NET_SHORT_CLK_PWR;
            pin_label(i, la, sizeof(la));
            snprintf(line, sizeof(line), "%s: %s on BB1 5V", r01a_netlist_kind_label(sk), la);
            add_issue(out, max_out, &nout, sk, line);
        }
    }

    memset(seen_root, 0, sizeof(seen_root));
    for (i = 0; i < g_nslot; i++) {
        int root = pin_root(i);
        int k;
        int ak;
        int n_unseat = 0;
        int comps[R01A_NL_SLOTS];
        int ncomp = 0;
        int first_un = -1;
        int a;
        if (seen_root[root]) {
            continue;
        }
        seen_root[root] = 1;
        ak = auto_kind(root);
        for (k = 0; k < g_nslot; k++) {
            int c;
            int found = 0;
            if (pin_root(k) != root) {
                continue;
            }
            if (!seated[k]) {
                if (first_un < 0) {
                    first_un = k;
                }
                n_unseat++;
                continue;
            }
            for (c = 0; c < ncomp; c++) {
                if (comps[c] == pin_g[k]) {
                    found = 1;
                    break;
                }
            }
            if (!found && ncomp < R01A_NL_SLOTS) {
                comps[ncomp++] = pin_g[k];
            }
        }
        if (n_unseat) {
            char la[32];
            char line[R01A_NET_TEXT_LEN];
            pin_label(first_un, la, sizeof(la));
            if (n_unseat == 1) {
                snprintf(line, sizeof(line), "unseated %s", la);
            } else {
                snprintf(line, sizeof(line), "unseated %s +%d", la, n_unseat - 1);
            }
            add_issue(out, max_out, &nout, R01A_NET_UNSEATED, line);
        }
        if (ak == R01A_NET_KIND_PWR || ak == R01A_NET_KIND_GND) {
            int *rail = (ak == R01A_NET_KIND_PWR) ? pwr_g : gnd_g;
            int open_n = 0;
            for (k = 0; k < g_nslot; k++) {
                int g;
                if (pin_root(k) != root || !seated[k]) {
                    continue;
                }
                g = pin_g[k];
                if (g != rail[0] && g != rail[1]) {
                    open_n++;
                }
            }
            if (open_n) {
                char names[72];
                char line[R01A_NET_TEXT_LEN];
                int listed = 0;
                names[0] = '\0';
                for (k = 0; k < g_nslot; k++) {
                    char la[32];
                    size_t used;
                    if (pin_root(k) != root || !seated[k]) {
                        continue;
                    }
                    if (pin_g[k] == rail[0] || pin_g[k] == rail[1]) {
                        continue;
                    }
                    if (listed >= 3) {
                        continue;
                    }
                    pin_label(k, la, sizeof(la));
                    used = strlen(names);
                    snprintf(names + used, sizeof(names) - used, "%s%s", listed ? " " : "", la);
                    listed++;
                }
                snprintf(line, sizeof(line), "open %s: %s%s (need BB1 %s)",
                         ak == R01A_NET_KIND_PWR ? "5V" : "GND", names, open_n > listed ? " ..." : "",
                         ak == R01A_NET_KIND_PWR ? "+5V" : "GND");
                add_issue(out, max_out, &nout, R01A_NET_MISSING, line);
            }
            continue;
        }
        if (ncomp > 1) {
            for (a = 0; a < g_nslot; a++) {
                int b;
                if (pin_root(a) != root || !seated[a]) {
                    continue;
                }
                for (b = a + 1; b < g_nslot; b++) {
                    char la[32];
                    char lb[32];
                    char line[R01A_NET_TEXT_LEN];
                    if (pin_root(b) != root || !seated[b] || pin_g[a] == pin_g[b]) {
                        continue;
                    }
                    pin_label(a, la, sizeof(la));
                    pin_label(b, lb, sizeof(lb));
                    snprintf(line, sizeof(line), "open %s -- %s", la, lb);
                    add_issue(out, max_out, &nout, R01A_NET_MISSING, line);
                    a = g_nslot;
                    break;
                }
            }
        }
    }
    return nout;
}
