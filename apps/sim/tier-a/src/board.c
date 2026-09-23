#include "r01a_board.h"

#include "netlist_sim/breadboard.h"
#include "netlist_sim/bus.h"
#include "netlist_sim/entity.h"
#include "netlist_sim/island.h"
#include "netlist_sim/passive.h"
#include "r01_kit_palette.h"
#include "r01a_raster.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R01A_SETTLE_PASSES 2
#define R01A_ROUTE_BB_MAX (R01A_BB_EXTRA_MAX + 1)
#define R01A_ROUTE_PIN_MAX 1024

static void kit_palette(uint8_t master_index, uint8_t *r, uint8_t *g, uint8_t *b) {
    r01_kit_rgb((int)master_index, r, g, b);
}

static void drive_copy(NsEntity *dst, const char *dn, const NsEntity *src, const char *sn) {
    ns_entity_drive(dst, dn, ns_entity_sense(src, sn));
}

static void drive_vdd(NsEntity *e, const char *name, NsLevel vdd) {
    if (ns_entity_pin_named(e, name)) {
        ns_entity_drive(e, name, vdd);
    }
}

static int bb_is_powered(const NsBreadboard *bb) {
    return bb && bb->base.refdes && strcmp(bb->base.refdes, "BB1") == 0;
}

static int strip_parent[NS_PB_STRIPS];

static int strip_find(int s) {
    while (s >= 0 && s < NS_PB_STRIPS && strip_parent[s] != s) {
        strip_parent[s] = strip_parent[strip_parent[s]];
        s = strip_parent[s];
    }
    return s;
}

static void strip_union(int a, int b) {
    int ra = strip_find(a);
    int rb = strip_find(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        strip_parent[rb] = ra;
    }
}

typedef struct R01aRoutePin {
    NsEntity *e;
    int pi;
    int bb_i;
    int root;
} R01aRoutePin;

static const R01aBoard *route_cache_board;
static uint64_t route_cache_geom;
static int route_cache_ok;
static int route_bb_n;
static R01aRoutePin route_pins[R01A_ROUTE_PIN_MAX];
static int route_pin_n;
static int route_pwr_bb_i;
static int route_vdd_root[2];
static int route_gnd_root[2];

static uint64_t route_mix(uint64_t h, uint64_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

static uint64_t route_geom_hash(const R01aBoard *b, const NsIsland *island) {
    uint64_t h = 0xcbf29ce484222325ULL;
    int i;
    int pi;
    if (!b || !island) {
        return 0;
    }
    h = route_mix(h, (uint64_t)b->jumper_count);
    h = route_mix(h, (uint64_t)b->extra_bb_count);
    h = route_mix(h, (uint64_t)b->passives.count);
    for (i = 0; i < b->jumper_count; i++) {
        const char *s = b->jumpers[i].bb_ref;
        h = route_mix(h, (uint64_t)b->jumpers[i].a.col);
        h = route_mix(h, (uint64_t)b->jumpers[i].a.lane);
        h = route_mix(h, (uint64_t)b->jumpers[i].b.col);
        h = route_mix(h, (uint64_t)b->jumpers[i].b.lane);
        for (; s && *s; s++) {
            h = route_mix(h, (unsigned char)*s);
        }
    }
    for (i = 0; i < island->entity_count; i++) {
        const NsEntity *e = island->entities[i];
        if (!e) {
            continue;
        }
        h = route_mix(h, (uint64_t)(uintptr_t)e);
        h = route_mix(h, (uint64_t)e->board_x);
        h = route_mix(h, (uint64_t)e->board_y);
        h = route_mix(h, (uint64_t)e->orient);
        h = route_mix(h, (uint64_t)e->pin_count);
        if (e->visual == NS_ENTITY_VIS_PASSIVE) {
            const NsPassive *p = (const NsPassive *)e;
            h = route_mix(h, (uint64_t)p->pivot_x);
            h = route_mix(h, (uint64_t)p->pivot_y);
            h = route_mix(h, (uint64_t)p->kind);
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            h = route_mix(h, (uint64_t)e->pins[pi].number);
        }
    }
    return h;
}

static int pin_name_is_gnd(const char *n) {
    return n && (strcmp(n, "GND") == 0 || strcmp(n, "AGND") == 0 || strcmp(n, "DGND") == 0);
}

static int pin_skip_route(const NsEntity *e, const NsPin *p) {
    (void)e;
    (void)p;
    return 0;
}

static int pin_is_driver(const NsPin *p) {
    if (!p) {
        return 0;
    }
    if (p->dir == NS_PIN_OUT || p->dir == NS_PIN_IO) {
        return 1;
    }
    return p->dir == NS_PIN_PWR && pin_name_is_gnd(p->name);
}

static NsLevel pin_drive_level(const NsPin *p) {
    if (pin_name_is_gnd(p->name)) {
        return NS_LVL_L;
    }
    return p->level;
}

static int entity_tip_board(const NsEntity *e, int pin_num, int *tx, int *ty) {
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return ns_passive_tip_board((const NsPassive *)e, pin_num, tx, ty);
    }
    return ns_entity_pin_tip_board(e, pin_num, tx, ty);
}

static int entity_is_route_skip(const NsEntity *e) {
    return !e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY;
}

static int jumper_on_bb(const R01aJumper *j, const NsBreadboard *bb) {
    const char *ref;
    if (!j || !bb || !bb->base.refdes) {
        return 0;
    }
    ref = j->bb_ref[0] ? j->bb_ref : "BB1";
    return strcmp(ref, bb->base.refdes) == 0;
}

static void board_bb_rebuild(R01aBoard *b, NsIsland *island) {
    int i;
    int s;
    int bb_i;
    route_pin_n = 0;
    route_bb_n = 0;
    route_pwr_bb_i = -1;
    if (!b || !island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        NsEntity *be = island->entities[i];
        NsBreadboard *bb;
        if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (route_bb_n >= R01A_ROUTE_BB_MAX) {
            break;
        }
        bb = (NsBreadboard *)be;
        bb_i = route_bb_n;
        route_bb_n++;
        for (s = 0; s < NS_PB_STRIPS; s++) {
            strip_parent[s] = s;
        }
        for (s = 0; s < b->jumper_count; s++) {
            if (!jumper_on_bb(&b->jumpers[s], bb)) {
                continue;
            }
            strip_union(ns_breadboard_strip_id(b->jumpers[s].a), ns_breadboard_strip_id(b->jumpers[s].b));
        }
        {
            int ri;
            for (ri = 0; ri < b->passives.count; ri++) {
                NsPassive *p = &b->passives.parts[ri];
                int t1x;
                int t1y;
                int t2x;
                int t2y;
                int s1;
                int s2;
                if (p->kind != NS_PASSIVE_R) {
                    continue;
                }
                if (!ns_passive_tip_board(p, 1, &t1x, &t1y) || !ns_passive_tip_board(p, 2, &t2x, &t2y)) {
                    continue;
                }
                if (!ns_breadboard_tip_strip(bb, t1x, t1y, &s1) || !ns_breadboard_tip_strip(bb, t2x, t2y, &s2)) {
                    continue;
                }
                strip_union(s1, s2);
            }
        }
        if (bb_is_powered(bb)) {
            int half;
            route_pwr_bb_i = bb_i;
            for (half = 0; half < 2; half++) {
                NsPbHole hp = {half ? NS_PB_RAIL_GAP_END : 0, NS_PB_LANE_TOP_POS};
                NsPbHole hn = {half ? NS_PB_RAIL_GAP_END : 0, NS_PB_LANE_TOP_NEG};
                route_vdd_root[half] = strip_find(ns_breadboard_strip_id(hp));
                route_gnd_root[half] = strip_find(ns_breadboard_strip_id(hn));
            }
        }
        for (s = 0; s < island->entity_count; s++) {
            NsEntity *e = island->entities[s];
            int pi;
            if (entity_is_route_skip(e)) {
                continue;
            }
            for (pi = 0; pi < e->pin_count; pi++) {
                int tx;
                int ty;
                int strip;
                int root;
                if (pin_skip_route(e, &e->pins[pi])) {
                    continue;
                }
                if (!entity_tip_board(e, e->pins[pi].number, &tx, &ty)) {
                    continue;
                }
                if (!ns_breadboard_tip_strip(bb, tx, ty, &strip)) {
                    continue;
                }
                root = strip_find(strip);
                if (root < 0 || root >= NS_PB_STRIPS || route_pin_n >= R01A_ROUTE_PIN_MAX) {
                    continue;
                }
                route_pins[route_pin_n].e = e;
                route_pins[route_pin_n].pi = pi;
                route_pins[route_pin_n].bb_i = bb_i;
                route_pins[route_pin_n].root = root;
                route_pin_n++;
            }
        }
    }
}

static void board_bb_propagate(void) {
    NsLevel net_lvl[R01A_ROUTE_BB_MAX][NS_PB_STRIPS];
    uint8_t net_used[R01A_ROUTE_BB_MAX][NS_PB_STRIPS];
    int i;
    memset(net_lvl, 0, sizeof(net_lvl));
    memset(net_used, 0, sizeof(net_used));
    for (i = 0; i < route_pin_n; i++) {
        R01aRoutePin *rp = &route_pins[i];
        const NsPin *p = &rp->e->pins[rp->pi];
        int bi = rp->bb_i;
        int root = rp->root;
        if (bi < 0 || bi >= route_bb_n || root < 0 || root >= NS_PB_STRIPS) {
            continue;
        }
        net_used[bi][root] = 1;
        if (rp->e->visual != NS_ENTITY_VIS_PASSIVE && pin_is_driver(p)) {
            net_lvl[bi][root] = ns_level_merge(net_lvl[bi][root], pin_drive_level(p));
        }
    }
    if (route_pwr_bb_i >= 0 && route_pwr_bb_i < route_bb_n) {
        int bi = route_pwr_bb_i;
        int k;
        for (k = 0; k < 2; k++) {
            int r = route_vdd_root[k];
            if (r >= 0 && r < NS_PB_STRIPS) {
                net_used[bi][r] = 1;
                net_lvl[bi][r] = ns_level_merge(net_lvl[bi][r], NS_LVL_H);
            }
            r = route_gnd_root[k];
            if (r >= 0 && r < NS_PB_STRIPS) {
                net_used[bi][r] = 1;
                net_lvl[bi][r] = ns_level_merge(net_lvl[bi][r], NS_LVL_L);
            }
        }
    }
    for (i = 0; i < route_pin_n; i++) {
        R01aRoutePin *rp = &route_pins[i];
        int bi = rp->bb_i;
        int root = rp->root;
        if (bi < 0 || bi >= route_bb_n || root < 0 || root >= NS_PB_STRIPS || !net_used[bi][root]) {
            continue;
        }
        rp->e->pins[rp->pi].level = net_lvl[bi][root];
    }
}

static void board_bb_route(R01aBoard *b) {
    NsIsland *island;
    uint64_t geom;
    int prev_fatal;

    if (!b) {
        return;
    }
    island = ns_island_group_at_mut(r01a_board_group(b), 0);
    if (!island) {
        return;
    }
    geom = route_geom_hash(b, island);
    if (!route_cache_ok || route_cache_board != b || route_cache_geom != geom) {
        board_bb_rebuild(b, island);
        route_cache_board = b;
        route_cache_geom = geom;
        route_cache_ok = 1;
    }
    prev_fatal = ns_bus_fatal_conflicts();
    ns_bus_set_fatal_conflicts(0);
    board_bb_propagate();
    ns_bus_set_fatal_conflicts(prev_fatal);
}

static void board_float_external(R01aBoard *b) {
    NsIsland *island;
    int i;
    if (!b) {
        return;
    }
    island = ns_island_group_at_mut(r01a_board_group(b), 0);
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count; i++) {
        NsEntity *e = island->entities[i];
        int pi;
        if (entity_is_route_skip(e)) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            if (pin_skip_route(e, &e->pins[pi])) {
                continue;
            }
            if (e->pins[pi].dir == NS_PIN_OUT || e->pins[pi].dir == NS_PIN_IO) {
                continue;
            }
            e->pins[pi].level = NS_LVL_Z;
        }
    }
}

static void board_eval_chips(R01aBoard *b) {
    ns_entity_eval(r01a_osc_dot_entity(&b->osc_dot));
    ns_entity_eval(r01a_osc_fsc_entity(&b->osc_fsc));
    ns_entity_eval(r01a_atf22v10_entity(&b->beam_x));
    ns_entity_eval(r01a_atf22v10_entity(&b->beam_y));
    ns_entity_eval(r01a_at27c256r_entity(&b->prom));
    ns_entity_eval(r01a_ad724_entity(&b->ad724));
}

static void board_bind_rails(R01aBoard *b) {
    NsLevel vdd = NS_LVL_H;

    drive_vdd(r01a_osc_dot_entity(&b->osc_dot), "VDD", vdd);
    drive_vdd(r01a_osc_dot_entity(&b->osc_dot), "OE#", NS_LVL_H);
    drive_vdd(r01a_osc_fsc_entity(&b->osc_fsc), "VDD", vdd);
    drive_vdd(r01a_osc_fsc_entity(&b->osc_fsc), "OE#", NS_LVL_H);
    drive_vdd(r01a_atf22v10_entity(&b->beam_x), "VCC", vdd);
    drive_vdd(r01a_atf22v10_entity(&b->beam_x), "RES#", NS_LVL_H);
    drive_vdd(r01a_atf22v10_entity(&b->beam_y), "VCC", vdd);
    drive_vdd(r01a_atf22v10_entity(&b->beam_y), "RES#", NS_LVL_H);
    drive_vdd(r01a_at27c256r_entity(&b->prom), "VCC", vdd);
    drive_vdd(r01a_at27c256r_entity(&b->prom), "VPP", vdd);
    drive_vdd(r01a_at27c256r_entity(&b->prom), "PGM#", NS_LVL_H);
    drive_vdd(r01a_ad724_entity(&b->ad724), "APOS", vdd);
    drive_vdd(r01a_ad724_entity(&b->ad724), "DPOS", vdd);
    drive_vdd(r01a_ad724_entity(&b->ad724), "ENCD", NS_LVL_H);
    drive_vdd(r01a_ad724_entity(&b->ad724), "STND", NS_LVL_H);
    drive_vdd(r01a_ad724_entity(&b->ad724), "SELECT", NS_LVL_L);
    drive_vdd(r01a_ad724_entity(&b->ad724), "VSYNC", NS_LVL_H);
}

static void board_tie_prom_unused_a(R01aBoard *b) {
    NsEntity *prom = r01a_at27c256r_entity(&b->prom);
    static const char *const unused[] = {"A6", "A7", "A8", "A9", "A10", "A11", "A12", "A13"};
    size_t i;
    for (i = 0; i < sizeof(unused) / sizeof(unused[0]); i++) {
        ns_entity_drive(prom, unused[i], NS_LVL_L);
    }
    ns_entity_drive(prom, "CE#", NS_LVL_L);
    ns_entity_drive(prom, "OE#", NS_LVL_L);
}

static void board_wire_index(R01aBoard *b) {
    NsEntity *bx = r01a_atf22v10_entity(&b->beam_x);
    NsEntity *prom = r01a_at27c256r_entity(&b->prom);
    int i;
    char iname[8];
    char aname[4];
    for (i = 0; i < 6; i++) {
        snprintf(iname, sizeof(iname), "INDEX%d", i);
        snprintf(aname, sizeof(aname), "A%d", i);
        drive_copy(prom, aname, bx, iname);
    }
}

static void board_plot(R01aBoard *b) {
    int x = r01a_atf22v10_x(&b->beam_x);
    int y = r01a_atf22v10_y(&b->beam_y);
    int vis;
    uint8_t idx;

    if (y != b->prev_y && y == R01A_BEAM_VISIBLE_H) {
        ns_video_sink_on_vblank(&b->sink);
    }
    b->prev_y = y;

    vis = (x >= 0 && x < R01A_BEAM_VISIBLE_W && y >= 0 && y < R01A_BEAM_VISIBLE_H);
    if (!vis) {
        return;
    }
    if (!ns_rgbs_beam_to_logical(ns_video_sink_scale_2x(&b->sink), x, y, NULL, NULL)) {
        return;
    }
    if (!r01a_ad724_encode_ok(&b->ad724)) {
        return;
    }
    if (ns_entity_sense(r01a_osc_dot_entity(&b->osc_dot), "DOT") != NS_LVL_H) {
        return;
    }
    idx = r01a_atf22v10_index(&b->beam_x);
    ns_video_sink_plot(&b->sink, x, y, idx);
}

static void board_settle(R01aBoard *b) {
    int p;
    if (b->wire_mode == R01A_WIRE_MANUAL) {
        board_float_external(b);
        for (p = 0; p < R01A_SETTLE_PASSES; p++) {
            board_bb_route(b);
            board_eval_chips(b);
        }
        return;
    }
    board_bind_rails(b);
    board_tie_prom_unused_a(b);
    board_wire_index(b);
    drive_copy(r01a_ad724_entity(&b->ad724), "FIN", r01a_osc_fsc_entity(&b->osc_fsc), "FSC");
    drive_copy(r01a_ad724_entity(&b->ad724), "HSYNC", r01a_atf22v10_entity(&b->beam_x), "CSYNC");
    for (p = 0; p < R01A_SETTLE_PASSES; p++) {
        board_eval_chips(b);
    }
}

void r01a_board_step(R01aBoard *board) {
    if (!board) {
        return;
    }
    if (board->wire_mode == R01A_WIRE_MANUAL) {
        board_float_external(board);
        board_bb_route(board);
        ns_entity_tick(r01a_osc_dot_entity(&board->osc_dot));
        ns_entity_tick(r01a_osc_fsc_entity(&board->osc_fsc));
        board_bb_route(board);
        ns_entity_tick(r01a_atf22v10_entity(&board->beam_x));
        board_bb_route(board);
        ns_entity_tick(r01a_atf22v10_entity(&board->beam_y));
        board_settle(board);
        board_plot(board);
        return;
    }
    board_bind_rails(board);
    ns_entity_tick(r01a_osc_dot_entity(&board->osc_dot));
    ns_entity_tick(r01a_osc_fsc_entity(&board->osc_fsc));
    drive_copy(r01a_atf22v10_entity(&board->beam_x), "CLK", r01a_osc_dot_entity(&board->osc_dot), "DOT");
    ns_entity_tick(r01a_atf22v10_entity(&board->beam_x));
    drive_copy(r01a_atf22v10_entity(&board->beam_y), "CLK", r01a_atf22v10_entity(&board->beam_x), "HWRAP");
    ns_entity_tick(r01a_atf22v10_entity(&board->beam_y));
    board_settle(board);
    board_plot(board);
}

void r01a_board_step_dots(R01aBoard *board, uint32_t dots) {
    uint32_t i;
    for (i = 0; i < dots; i++) {
        r01a_board_step(board);
        r01a_board_step(board);
    }
}

static void spawn_tier_a_passives(R01aBoard *b);

static void island_video_init(NsIsland *island) {
    R01aBoard *b = (R01aBoard *)island->impl;
    int i;
    r01a_osc_dot_init(&b->osc_dot, "Y2");
    r01a_osc_fsc_init(&b->osc_fsc, "Y3");
    r01a_atf22v10_init(&b->beam_x, "UPLDX", R01A_PLD_BEAM_X);
    r01a_atf22v10_init(&b->beam_y, "UPLDY", R01A_PLD_BEAM_Y);
    r01a_at27c256r_init(&b->prom, "U24");
    r01a_ad724_init(&b->ad724, "UENC");
    ns_breadboard_init(&b->breadboard, "BB1");
    ns_video_sink_init(&b->sink, "SCR1");
    ns_video_sink_set_palette(&b->sink, kit_palette);
    ns_island_add_entity(island, ns_breadboard_entity(&b->breadboard));
    ns_island_add_entity(island, r01a_osc_dot_entity(&b->osc_dot));
    ns_island_add_entity(island, r01a_osc_fsc_entity(&b->osc_fsc));
    ns_island_add_entity(island, r01a_atf22v10_entity(&b->beam_x));
    ns_island_add_entity(island, r01a_atf22v10_entity(&b->beam_y));
    ns_island_add_entity(island, r01a_at27c256r_entity(&b->prom));
    ns_island_add_entity(island, r01a_ad724_entity(&b->ad724));
    ns_island_add_entity(island, ns_video_sink_entity(&b->sink));
    spawn_tier_a_passives(b);
    for (i = 0; i < b->passives.count; i++) {
        ns_island_add_entity(island, &b->passives.parts[i].base);
    }
}

static const NsIslandVTable ISLAND_VIDEO_VT = {island_video_init, NULL, NULL, NULL, NULL};

static void group_reset(NsIslandGroup *group) {
    R01aBoard *b = (R01aBoard *)group->impl;
    ns_entity_reset(r01a_osc_dot_entity(&b->osc_dot));
    ns_entity_reset(r01a_osc_fsc_entity(&b->osc_fsc));
    ns_entity_reset(r01a_atf22v10_entity(&b->beam_x));
    ns_entity_reset(r01a_atf22v10_entity(&b->beam_y));
    ns_entity_reset(r01a_at27c256r_entity(&b->prom));
    ns_entity_reset(r01a_ad724_entity(&b->ad724));
    ns_entity_reset(ns_video_sink_entity(&b->sink));
    ns_video_sink_clear(&b->sink);
    b->prev_y = 0;
    board_settle(b);
}

static void group_step(NsIslandGroup *group) {
    r01a_board_step((R01aBoard *)group->impl);
}

static void group_eval_idle(NsIslandGroup *group) {
    board_settle((R01aBoard *)group->impl);
}

static void group_status(NsIslandGroup *group, char *buf, size_t buf_len) {
    R01aBoard *b = (R01aBoard *)group->impl;
    snprintf(buf, buf_len, "X=%d Y=%d HB=%d VB=%d enc=%d idx=%u", r01a_atf22v10_x(&b->beam_x),
             r01a_atf22v10_y(&b->beam_y), r01a_atf22v10_hblank(&b->beam_x),
             r01a_atf22v10_vblank(&b->beam_y), r01a_ad724_encode_ok(&b->ad724),
             (unsigned)r01a_atf22v10_index(&b->beam_x));
}

static const NsIslandGroupVTable BOARD_GROUP_VT = {
    NULL, group_reset, NULL, group_step, group_eval_idle, group_status, NULL, NULL};

static void add_passives(NsPassiveBank *bank, NsPassiveKind kind, const char *prefix, int *seq,
                         const char *value, int n) {
    int i;
    for (i = 0; i < n; i++) {
        char ref[NS_PASSIVE_REF_LEN];
        snprintf(ref, sizeof(ref), "%s%d", prefix, (*seq)++);
        ns_passive_bank_add(bank, kind, ref, value);
    }
}

static void spawn_tier_a_passives(R01aBoard *b) {
    int c_seq = 1;
    int e_seq = 1;
    int r_seq = 1;
    ns_passive_bank_clear(&b->passives);
    add_passives(&b->passives, NS_PASSIVE_CCAP, "C", &c_seq, "100nF", 7);
    add_passives(&b->passives, NS_PASSIVE_ECAP, "E", &e_seq, "220uF", 1);
    /* DAC R then G then B, then 75 ohm loads, then DOT/FSC series. */
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "4.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "2.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "1.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "4.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "2.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "1.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "2.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "1.00k", 1);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "75.0", 3);
    add_passives(&b->passives, NS_PASSIVE_R, "R", &r_seq, "33", 2);
}

static int place_along(NsEntity *e, int *x, int y, int gap) {
    int bottom;
    if (!e) {
        return y;
    }
    ns_entity_place(e, *x, y);
    bottom = y + e->body_h;
    *x += e->body_w + gap;
    return bottom;
}

static void board_place_free(R01aBoard *board) {
    const int origin_x = 40;
    const int origin_y = 40;
    const int gap = 12;
    int x;
    int y;
    int row0_h = 0;
    int row0_right;
    int row1_right;
    int cluster_right;
    int bottom;

    x = origin_x;
    y = origin_y;
    bottom = place_along(r01a_osc_dot_entity(&board->osc_dot), &x, y, gap);
    if (bottom - y > row0_h) {
        row0_h = bottom - y;
    }
    bottom = place_along(r01a_osc_fsc_entity(&board->osc_fsc), &x, y, gap);
    if (bottom - y > row0_h) {
        row0_h = bottom - y;
    }
    row0_right = x - gap;

    x = origin_x;
    y = origin_y + row0_h + 16;
    (void)place_along(r01a_atf22v10_entity(&board->beam_x), &x, y, gap);
    (void)place_along(r01a_atf22v10_entity(&board->beam_y), &x, y, gap);
    (void)place_along(r01a_at27c256r_entity(&board->prom), &x, y, gap);
    (void)place_along(r01a_ad724_entity(&board->ad724), &x, y, gap);
    row1_right = x - gap;
    cluster_right = row0_right > row1_right ? row0_right : row1_right;
    ns_entity_place(ns_video_sink_entity(&board->sink), cluster_right + 28, origin_y);
    ns_entity_place(ns_breadboard_entity(&board->breadboard), origin_x, y + 72);
    {
        NsEntity *bb = ns_breadboard_entity(&board->breadboard);
        ns_passive_bank_layout_grid(&board->passives, origin_x, bb->board_y + bb->body_h + 16, 6, 6);
    }
}

void r01a_board_init(R01aBoard *board) {
    NsIslandBuilder *b;
    if (!board) {
        return;
    }
    memset(board, 0, sizeof(*board));
    board->running = 1;
    ns_island_builder_init(&board->builder);
    b = &board->builder;
    ns_island_builder_bind(b, &BOARD_GROUP_VT, board);
    if (ns_island_builder_add(b, &ISLAND_VIDEO_VT, "ISLAND A  VIDEO LAB", 0, 0, 1, 1, board) < 0) {
        return;
    }
    ns_island_builder_mount(b, ns_breadboard_entity(&board->breadboard), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_osc_dot_entity(&board->osc_dot), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_osc_fsc_entity(&board->osc_fsc), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_atf22v10_entity(&board->beam_x), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_atf22v10_entity(&board->beam_y), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_at27c256r_entity(&board->prom), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_ad724_entity(&board->ad724), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, ns_video_sink_entity(&board->sink), R01A_ISLAND_VIDEO, 0, 0);
    {
        int i;
        for (i = 0; i < board->passives.count; i++) {
            ns_island_builder_mount(b, &board->passives.parts[i].base, R01A_ISLAND_VIDEO, 0, 0);
        }
    }
    ns_island_builder_finish(b);
    board_place_free(board);
}

void r01a_board_shutdown(R01aBoard *board) {
    if (!board) {
        return;
    }
    ns_island_builder_shutdown(&board->builder);
}

void r01a_board_reset(R01aBoard *board) {
    if (!board) {
        return;
    }
    ns_island_group_reset(ns_island_builder_group(&board->builder));
}

NsIslandGroup *r01a_board_group(R01aBoard *board) {
    return board ? ns_island_builder_group(&board->builder) : NULL;
}

void r01a_board_set_wire_mode(R01aBoard *board, int mode) {
    int next;
    if (!board) {
        return;
    }
    next = (mode == R01A_WIRE_MANUAL) ? R01A_WIRE_MANUAL : R01A_WIRE_AUTO;
    if (board->wire_mode == next) {
        return;
    }
    board->wire_mode = next;
    if (next == R01A_WIRE_MANUAL) {
        ns_video_sink_clear(&board->sink);
    }
    board_settle(board);
}

int r01a_board_wire_mode(const R01aBoard *board) {
    return board ? board->wire_mode : R01A_WIRE_AUTO;
}

static int hole_same(NsPbHole a, NsPbHole b) {
    return a.col == b.col && a.lane == b.lane;
}

int r01a_board_jumper_add_on(R01aBoard *board, NsBreadboard *bb, NsPbHole a, NsPbHole b, uint8_t cr,
                            uint8_t cg, uint8_t cb) {
    int i;
    const char *ref;
    if (!board || !bb || !ns_breadboard_hole_exists(a) || !ns_breadboard_hole_exists(b) || hole_same(a, b)) {
        return 0;
    }
    ref = bb->base.refdes ? bb->base.refdes : "BB1";
    for (i = 0; i < board->jumper_count; i++) {
        if (!jumper_on_bb(&board->jumpers[i], bb)) {
            continue;
        }
        if ((hole_same(board->jumpers[i].a, a) && hole_same(board->jumpers[i].b, b)) ||
            (hole_same(board->jumpers[i].a, b) && hole_same(board->jumpers[i].b, a))) {
            return 1;
        }
    }
    if (board->jumper_count >= R01A_JUMPER_MAX) {
        return 0;
    }
    board->jumpers[board->jumper_count].a = a;
    board->jumpers[board->jumper_count].b = b;
    snprintf(board->jumpers[board->jumper_count].bb_ref, sizeof(board->jumpers[board->jumper_count].bb_ref),
             "%s", ref);
    board->jumpers[board->jumper_count].r = cr;
    board->jumpers[board->jumper_count].g = cg;
    board->jumpers[board->jumper_count].bcol = cb;
    board->jumper_count++;
    return 1;
}

int r01a_board_jumper_add(R01aBoard *board, NsPbHole a, NsPbHole b) {
    if (!board) {
        return 0;
    }
    return r01a_board_jumper_add_on(board, &board->breadboard, a, b, 220, 160, 40);
}

void r01a_board_jumper_remove(R01aBoard *board, int index) {
    int i;
    if (!board || index < 0 || index >= board->jumper_count) {
        return;
    }
    for (i = index; i < board->jumper_count - 1; i++) {
        board->jumpers[i] = board->jumpers[i + 1];
    }
    board->jumper_count--;
}

int r01a_board_jumper_set_end(R01aBoard *board, int index, int end_b, NsPbHole hole) {
    NsPbHole other;
    if (!board || index < 0 || index >= board->jumper_count || !ns_breadboard_hole_exists(hole)) {
        return 0;
    }
    other = end_b ? board->jumpers[index].a : board->jumpers[index].b;
    if (hole_same(other, hole)) {
        return 0;
    }
    if (end_b) {
        board->jumpers[index].b = hole;
    } else {
        board->jumpers[index].a = hole;
    }
    return 1;
}

void r01a_board_jumper_clear(R01aBoard *board) {
    if (!board) {
        return;
    }
    board->jumper_count = 0;
}

NsEntity *r01a_board_entity_by_refdes(R01aBoard *board, const char *refdes) {
    NsIsland *island;
    int i;
    if (!board || !refdes || !refdes[0]) {
        return NULL;
    }
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return NULL;
    }
    for (i = 0; i < island->entity_count; i++) {
        NsEntity *e = island->entities[i];
        if (e && e->refdes && strcmp(e->refdes, refdes) == 0) {
            return e;
        }
    }
    return NULL;
}

static const char *passive_ref_prefix(NsPassiveKind kind) {
    switch (kind) {
    case NS_PASSIVE_R:
        return "R";
    case NS_PASSIVE_CCAP:
        return "C";
    case NS_PASSIVE_ECAP:
        return "E";
    case NS_PASSIVE_OSC:
    case NS_PASSIVE_OSC4LEGS:
        return "Y";
    case NS_PASSIVE_D:
        return "D";
    default:
        return "P";
    }
}

static int next_ref_seq(R01aBoard *board, const char *prefix) {
    NsIsland *island;
    int i;
    int maxn = 0;
    size_t plen;
    if (!board || !prefix || !prefix[0]) {
        return 1;
    }
    plen = strlen(prefix);
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return 1;
    }
    for (i = 0; i < island->entity_count; i++) {
        const char *ref;
        char *end;
        long n;
        if (!island->entities[i] || !island->entities[i]->refdes) {
            continue;
        }
        ref = island->entities[i]->refdes;
        if (strncmp(ref, prefix, plen) != 0) {
            continue;
        }
        n = strtol(ref + plen, &end, 10);
        if (end != ref + plen && *end == '\0' && n > maxn) {
            maxn = (int)n;
        }
    }
    return maxn + 1;
}

NsPassive *r01a_board_add_passive(R01aBoard *board, NsPassiveKind kind, const char *value, int x, int y) {
    NsIsland *island;
    NsPassive *p;
    char ref[NS_PASSIVE_REF_LEN];
    int seq;
    if (!board || kind < 0 || kind >= NS_PASSIVE_KIND_COUNT) {
        return NULL;
    }
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return NULL;
    }
    seq = next_ref_seq(board, passive_ref_prefix(kind));
    if (seq < 1) {
        seq = 1;
    }
    if (seq > 999) {
        seq = 999;
    }
    snprintf(ref, sizeof(ref), "%s%d", passive_ref_prefix(kind), seq);
    p = ns_passive_bank_add(&board->passives, kind, ref, value);
    if (!p) {
        return NULL;
    }
    ns_passive_set_orient(p, NS_ORIENT_0);
    ns_passive_set_pivot(p, x, y);
    if (ns_island_add_entity(island, &p->base) != 0) {
        board->passives.count--;
        return NULL;
    }
    return p;
}

static int entity_on_island(const NsIsland *island, const NsEntity *e) {
    int i;
    if (!island || !e) {
        return 0;
    }
    for (i = 0; i < island->entity_count; i++) {
        if (island->entities[i] == e) {
            return 1;
        }
    }
    return 0;
}

NsBreadboard *r01a_board_add_breadboard(R01aBoard *board, int x, int y) {
    NsIsland *island;
    NsBreadboard *bb;
    char ref[R01A_BB_REF_LEN];
    int seq;
    if (!board) {
        return NULL;
    }
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return NULL;
    }
    if (!entity_on_island(island, ns_breadboard_entity(&board->breadboard))) {
        ns_entity_place(&board->breadboard.base, x, y);
        if (ns_island_add_entity(island, ns_breadboard_entity(&board->breadboard)) != 0) {
            return NULL;
        }
        return &board->breadboard;
    }
    if (board->extra_bb_count >= R01A_BB_EXTRA_MAX) {
        return NULL;
    }
    seq = next_ref_seq(board, "BB");
    if (seq < 2) {
        seq = 2;
    }
    if (seq > 99) {
        seq = 99;
    }
    snprintf(ref, sizeof(ref), "BB%d", seq);
    bb = &board->extra_bb[board->extra_bb_count];
    ns_breadboard_init(bb, ref);
    ns_entity_place(&bb->base, x, y);
    if (ns_island_add_entity(island, ns_breadboard_entity(bb)) != 0) {
        return NULL;
    }
    board->extra_bb_count++;
    return bb;
}

static void extra_bb_fix_aliases(NsBreadboard *bb) {
    if (!bb) {
        return;
    }
    bb->base.refdes = bb->refdes_buf;
    bb->base.impl = bb;
}

int r01a_board_remove_breadboard(R01aBoard *board, NsBreadboard *bb) {
    NsIsland *island;
    int extra_i = -1;
    int i;
    if (!board || !bb) {
        return 0;
    }
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return 0;
    }
    for (i = 0; i < board->extra_bb_count; i++) {
        if (&board->extra_bb[i] == bb) {
            extra_i = i;
            break;
        }
    }
    if (extra_i < 0 && bb != &board->breadboard) {
        return 0;
    }
    for (i = board->jumper_count - 1; i >= 0; i--) {
        if (jumper_on_bb(&board->jumpers[i], bb)) {
            r01a_board_jumper_remove(board, i);
        }
    }
    ns_island_remove_entity(island, ns_breadboard_entity(bb));
    if (extra_i < 0) {
        return 1;
    }
    for (i = extra_i + 1; i < board->extra_bb_count; i++) {
        ns_island_remove_entity(island, ns_breadboard_entity(&board->extra_bb[i]));
    }
    for (i = extra_i; i < board->extra_bb_count - 1; i++) {
        board->extra_bb[i] = board->extra_bb[i + 1];
        extra_bb_fix_aliases(&board->extra_bb[i]);
    }
    board->extra_bb_count--;
    for (i = extra_i; i < board->extra_bb_count; i++) {
        ns_island_add_entity(island, ns_breadboard_entity(&board->extra_bb[i]));
    }
    return 1;
}
