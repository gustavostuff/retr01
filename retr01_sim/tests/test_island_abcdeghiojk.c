#include "atmega328p.h"
#include "as6c62256.h"
#include "at28c16.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "retr01_sim/board.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/island_builder.h"
#include "sn74hc573.h"
#include "sn74hc688.h"
#include "sst39sf040.h"
#include "test_common.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdio.h>

/*
 * Layer-2 islands A–E + G + H + I + O + J + K smoke:
 *   prior milestones + MAP $FE93 'R' + APU $FE4x tone PWM edges.
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
    int saw_map = 0;
    int saw_apu = 0;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    group = r01s_island_builder_group(&builder);
    expect_true(group != NULL, "group");
    expect_true(r01s_island_group_count(group) == 11, "11 islands A-E+G+H+I+O+J+K");

    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");
    expect_true(b->cart_loaded, "cart loaded");
    expect_true(r01s_sst39sf040_peek(&b->cart_flash, 0) == 'R', "flash magic R");
    expect_true(r01s_sst39sf040_peek(&b->cart_flash, 1) == 'E', "flash magic E");

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

    for (i = 0; i < 14000; i++) {
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
        if (b->health_saw_map) {
            saw_map = 1;
        }
        if (b->health_saw_apu || r01s_atmega328p_pwm_edges(&b->apu) >= 2) {
            saw_apu = 1;
        }
        if (saw_latch && saw_vram && saw_vram_read && saw_pad && saw_beam_hblank && saw_beam_line &&
            saw_raster_hit && saw_bg_tile && saw_bg_attr && saw_video && saw_map && saw_apu) {
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
    expect_true(saw_map, "island J MAP $FE93 read cart magic R");
    expect_true(saw_apu, "island K APU PWM tone edges");
    expect_true(r01s_atmega328p_peek(&b->apu, 0) == 0x8F, "APU $FE40 enable+vol");
    expect_true(r01s_atmega328p_period(&b->apu) == 0x10, "APU period $10");
    expect_true(r01s_as6c62256_peek(&b->vram, 0) == 0x42, "VRAM[0] final tile");
    expect_true(b->cycles > 0, "CPU cycles advanced");
    expect_true(r01s_bus_conflict_count() == 0, "no bus fight");

    /* Load Studio cart (if present) and confirm magic + bring-up overlay still boots. */
    {
        const char *paths[] = {"../retr01_studio/project.retr01", "../../retr01_studio/project.retr01",
                               "retr01_studio/project.retr01", NULL};
        int pi;
        for (pi = 0; paths[pi]; pi++) {
            if (r01s_board_load_cart(b, paths[pi]) == 0) {
                expect_true(r01s_sst39sf040_peek(&b->cart_flash, 0) == 'R', "studio cart magic");
                expect_true(r01s_sst39sf040_peek(&b->cart_flash, b->cart_off_prg) == 0xA9, "bring-up LDA overlay");
                break;
            }
        }
    }

    r01s_island_builder_shutdown(&builder);
    return test_done("test_island_abcdeghiojk");
}
