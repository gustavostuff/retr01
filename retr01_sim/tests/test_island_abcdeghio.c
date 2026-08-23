#include "as6c62256.h"
#include "at28c16.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "retr01_sim/board.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/island_builder.h"
#include "sn74hc573.h"
#include "sn74hc688.h"
#include "test_common.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdio.h>

/*
 * Layer-2 islands A–E + G + H + I + O smoke:
 *   D $FE02 / $FE04 latches, E pads, G VRAM, H beam, I BG fetch, O RGBS sink.
 */
int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;
    R01sIslandGroup *group;
    R01sBoard *b;
    int i;
    int saw_latch = 0;
    int saw_pad = 0;
    int saw_vram = 0;
    int saw_vram_read = 0;
    int saw_beam_hblank = 0;
    int saw_beam_line = 0;
    int saw_raster_hit = 0;
    int saw_bg_tile = 0;
    int saw_bg_attr = 0;
    int saw_video = 0;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    group = r01s_island_builder_group(&builder);
    expect_true(group != NULL, "group");
    expect_true(r01s_island_group_count(group) == 9, "9 islands A-E+G+H+I+O");

    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");
    r01s_pads_set(&b->pads, 0, 0xA5);
    {
        R01sEntity *raster = r01s_sn74hc573_entity(&b->raster_latch);
        int bi;
        char dn[8];
        for (bi = 0; bi < 8; bi++) {
            snprintf(dn, sizeof(dn), "%dD", bi + 1);
            r01s_entity_drive(raster, dn, R01S_LVL_L);
        }
        r01s_entity_drive(raster, "OE", R01S_LVL_L);
        r01s_entity_drive(raster, "LE", R01S_LVL_H);
        r01s_entity_eval(raster);
        r01s_entity_drive(raster, "LE", R01S_LVL_L);
    }

    for (i = 0; i < 12000; i++) {
        r01s_island_group_step(group);
        if (r01s_sn74hc573_peek_q(&b->latch) == 0x55) {
            saw_latch = 1;
        }
        if (r01s_as6c62256_peek(&b->vram, 0) == 0xAA) {
            saw_vram = 1;
        }
        if (r01s_w65c02s_a(&b->cpu) == 0xAA) {
            saw_vram_read = 1;
        }
        if (r01s_w65c02s_a(&b->cpu) == 0xA5) {
            saw_pad = 1;
        }
        if (r01s_beam_xy_hblank(&b->beam)) {
            saw_beam_hblank = 1;
        }
        if (r01s_beam_xy_y(&b->beam) >= 1) {
            saw_beam_line = 1;
        }
        if (r01s_entity_sense(r01s_sn74hc688_entity(&b->raster_cmp), "EQ#") == R01S_LVL_L) {
            saw_raster_hit = 1;
        }
        if (r01s_bg_fetch_last_tile(&b->bg_fetch) == 0x42) {
            saw_bg_tile = 1;
        }
        if (r01s_bg_fetch_last_attr(&b->bg_fetch) == 0x07) {
            saw_bg_attr = 1;
        }
        if (r01s_video_sink_lit_pixels(&b->video_sink) > 64) {
            saw_video = 1;
        }
        if (saw_latch && saw_vram && saw_vram_read && saw_pad && saw_beam_hblank && saw_beam_line &&
            saw_raster_hit && saw_bg_tile && saw_bg_attr && saw_video) {
            break;
        }
    }

    expect_true(saw_latch, "STA $FE02 hit island D latch");
    expect_true(saw_vram, "STA $FE12 wrote $AA to VRAM[0]");
    expect_true(saw_vram_read, "LDA $FE12 read VRAM back into A");
    expect_true(saw_pad, "LDA $FE60 read island E pads");
    expect_true(saw_beam_hblank, "island H HBlank");
    expect_true(saw_beam_line, "island H advanced past line 0");
    expect_true(saw_raster_hit, "HC688 EQ# on Y vs $FE04");
    expect_true(saw_bg_tile, "island I latched nametable tile $42");
    expect_true(saw_bg_attr, "island I latched nametable attr $07");
    expect_true(saw_video, "island O lit logical pixels from PROM");
    expect_true(r01s_as6c62256_peek(&b->vram, 0) == 0x42, "VRAM[0] final tile");
    expect_true(r01s_as6c62256_peek(&b->vram, 0xF0) == 0x07, "VRAM[0xF0] attr");
    expect_true(r01s_at28c16_peek(&b->color_prom, 0x02) == 0x20, "PROM index2 kit");
    expect_true(r01s_video_sink_pixel_packed(&b->video_sink, 0, 0) == 0x20,
                "pixel(0,0) via tile $42 -> index $02");
    expect_true(b->cycles > 0, "CPU cycles advanced");
    expect_true(r01s_bus_conflict_count() == 0, "no bus fight");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_island_abcdeghio");
}
