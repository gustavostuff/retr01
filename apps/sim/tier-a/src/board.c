#include "r01a_board.h"

#include "netlist_sim/bus.h"
#include "r01_kit_palette.h"
#include "r01a_raster.h"

#include <stdio.h>
#include <string.h>

#define R01A_SETTLE_PASSES 2

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

static void board_bind_rails(R01aBoard *b) {
    NsEntity *pwr = r01a_pwr5v_entity(&b->pwr);
    NsLevel vdd;
    ns_entity_drive(pwr, "VIN", NS_LVL_H);
    ns_entity_drive(pwr, "EN", NS_LVL_H);
    ns_entity_eval(pwr);
    vdd = ns_entity_sense(pwr, "VDD");

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
    board_bind_rails(b);
    board_tie_prom_unused_a(b);
    board_wire_index(b);
    drive_copy(r01a_ad724_entity(&b->ad724), "FIN", r01a_osc_fsc_entity(&b->osc_fsc), "FSC");
    drive_copy(r01a_ad724_entity(&b->ad724), "HSYNC", r01a_atf22v10_entity(&b->beam_x), "CSYNC");
    for (p = 0; p < R01A_SETTLE_PASSES; p++) {
        ns_entity_eval(r01a_pwr5v_entity(&b->pwr));
        ns_entity_eval(r01a_osc_dot_entity(&b->osc_dot));
        ns_entity_eval(r01a_osc_fsc_entity(&b->osc_fsc));
        ns_entity_eval(r01a_atf22v10_entity(&b->beam_x));
        ns_entity_eval(r01a_atf22v10_entity(&b->beam_y));
        ns_entity_eval(r01a_at27c256r_entity(&b->prom));
        ns_entity_eval(r01a_ad724_entity(&b->ad724));
    }
}

void r01a_board_step(R01aBoard *board) {
    if (!board) {
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

static void island_video_init(NsIsland *island) {
    R01aBoard *b = (R01aBoard *)island->impl;
    r01a_pwr5v_init(&b->pwr, "PS1");
    r01a_osc_dot_init(&b->osc_dot, "Y2");
    r01a_osc_fsc_init(&b->osc_fsc, "Y3");
    r01a_atf22v10_init(&b->beam_x, "UPLDX", R01A_PLD_BEAM_X);
    r01a_atf22v10_init(&b->beam_y, "UPLDY", R01A_PLD_BEAM_Y);
    r01a_at27c256r_init(&b->prom, "U24");
    r01a_ad724_init(&b->ad724, "UENC");
    ns_video_sink_init(&b->sink, "SCR1");
    ns_video_sink_set_palette(&b->sink, kit_palette);
    ns_video_sink_set_scale_2x(&b->sink, 1);
    ns_island_add_entity(island, r01a_pwr5v_entity(&b->pwr));
    ns_island_add_entity(island, r01a_osc_dot_entity(&b->osc_dot));
    ns_island_add_entity(island, r01a_osc_fsc_entity(&b->osc_fsc));
    ns_island_add_entity(island, r01a_atf22v10_entity(&b->beam_x));
    ns_island_add_entity(island, r01a_atf22v10_entity(&b->beam_y));
    ns_island_add_entity(island, r01a_at27c256r_entity(&b->prom));
    ns_island_add_entity(island, r01a_ad724_entity(&b->ad724));
    ns_island_add_entity(island, ns_video_sink_entity(&b->sink));
}

static const NsIslandVTable ISLAND_VIDEO_VT = {island_video_init, NULL, NULL, NULL, NULL};

static void group_reset(NsIslandGroup *group) {
    R01aBoard *b = (R01aBoard *)group->impl;
    ns_entity_reset(r01a_pwr5v_entity(&b->pwr));
    ns_entity_reset(r01a_osc_dot_entity(&b->osc_dot));
    ns_entity_reset(r01a_osc_fsc_entity(&b->osc_fsc));
    ns_entity_reset(r01a_atf22v10_entity(&b->beam_x));
    ns_entity_reset(r01a_atf22v10_entity(&b->beam_y));
    ns_entity_reset(r01a_at27c256r_entity(&b->prom));
    ns_entity_reset(r01a_ad724_entity(&b->ad724));
    ns_entity_reset(ns_video_sink_entity(&b->sink));
    ns_video_sink_set_scale_2x(&b->sink, 1);
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
    bottom = place_along(r01a_pwr5v_entity(&board->pwr), &x, y, gap);
    if (bottom - y > row0_h) {
        row0_h = bottom - y;
    }
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
    ns_island_builder_mount(b, r01a_pwr5v_entity(&board->pwr), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_osc_dot_entity(&board->osc_dot), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_osc_fsc_entity(&board->osc_fsc), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_atf22v10_entity(&board->beam_x), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_atf22v10_entity(&board->beam_y), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_at27c256r_entity(&board->prom), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, r01a_ad724_entity(&board->ad724), R01A_ISLAND_VIDEO, 0, 0);
    ns_island_builder_mount(b, ns_video_sink_entity(&board->sink), R01A_ISLAND_VIDEO, 0, 0);
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
