#include "atmega1284p.h"
#include "atmega328p.h"
#include "as6c62256.h"
#include "at28c16.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "retr01_sim/board.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/island_builder.h"
#include "atf22v10.h"
#include "sn74hc573.h"
#include "sst39sf040.h"
#include "test_common.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdio.h>

/*
 * Layer-2 islands A–E + G + H + I + O + J + K + L + M + N + P smoke:
 *   prior milestones + sprites + VBlank NMI (~60 Hz class) + no bus fight (Island P).
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
    int saw_oam = 0;
    int saw_linebuf = 0;
    int saw_sprites = 0;
    int saw_nmi = 0;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    group = r01s_island_builder_group(&builder);
    expect_true(group != NULL, "group");
    expect_true(r01s_island_group_count(group) == 16, "16 islands A-E+G+H+I+O+J+K+L+M+N+P+Q");

    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");
    expect_true(b->cart_loaded, "cart loaded");
    expect_true(r01s_sst39sf040_peek(&b->cart_flash, 0) == 'R', "flash magic R");
    expect_true(r01s_sst39sf040_peek(&b->cart_flash, 1) == 'E', "flash magic E");

    r01s_pads_set(&b->pads, 0, 0xA5);
    {
        R01sEntity *raster = r01s_sn74hc573_entity(&b->latch573[R01S_LATCH_FE04]);
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

    for (i = 0; i < 250000; i++) {
        r01s_island_group_step(group);
        if (r01s_sn74hc573_peek_q(&b->latch573[R01S_LATCH_FE02]) == 0x55) {
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
        if (r01s_beam_xy_hblank(&b->pld_beam_x)) {
            saw_beam_hblank = 1;
        }
        if (r01s_beam_xy_y(&b->pld_beam_x) >= 1) {
            saw_beam_line = 1;
        }
        if (r01s_entity_sense(r01s_atf22v10_entity(&b->pld_beam_y), "EQ#") == R01S_LVL_L) {
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
        if (b->health_saw_oam || (r01s_atmega1284p_oam_peek(&b->mcu1284, 0) == 0x10 &&
                                  r01s_atmega1284p_alive(&b->mcu1284))) {
            saw_oam = 1;
        }
        if (b->health_saw_linebuf || (b->linebuf_saw_mux_mcu && b->linebuf_saw_mux_beam)) {
            saw_linebuf = 1;
        }
        if (b->health_saw_sprites ||
            r01s_as6c62256_peek(&b->linebuf, (uint16_t)(((b->linebuf_show_half & 1u) << 7) | 0x20)) == 0x01 ||
            r01s_as6c62256_peek(&b->linebuf, 0x20) == 0x01 ||
            r01s_as6c62256_peek(&b->linebuf, 0xA0) == 0x01) {
            saw_sprites = 1;
        }
        if (b->health_saw_nmi || b->nmi_pulses >= 1) {
            saw_nmi = 1;
        }
        if (saw_latch && saw_vram && saw_vram_read && saw_pad && saw_beam_hblank && saw_beam_line &&
            saw_raster_hit && saw_bg_tile && saw_bg_attr && saw_video && saw_map && saw_apu && saw_oam &&
            saw_linebuf && saw_sprites && saw_nmi) {
            break;
        }
    }

    expect_true(saw_latch, "STA $FE02 hit island D latch");
    expect_true(saw_vram, "STA $FE12 wrote $AA to VRAM[0]");
    expect_true(saw_vram_read, "LDA $FE12 read VRAM back into A");
    expect_true(saw_pad, "LDA $FE60 read island E pads");
    expect_true(saw_beam_hblank, "island H HBlank");
    expect_true(saw_beam_line, "island H advanced past line 0");
    expect_true(saw_raster_hit, "beam-Y PLD EQ# on Y vs $FE04");
    expect_true(saw_bg_tile, "island I latched nametable tile $42");
    expect_true(saw_bg_attr, "island I latched nametable attr $07");
    expect_true(saw_video, "island O lit logical pixels from PROM");
    expect_true(saw_map, "island J MAP $FE93 read cart magic R");
    expect_true(saw_apu, "island K APU PWM tone edges");
    expect_true(saw_oam, "island L OAM $FE21 readback + clk");
    expect_true(saw_linebuf, "island M linebuf mux both paths");
    expect_true(saw_sprites, "island N sprite pixels in linebuf");
    expect_true(saw_nmi, "island P VBlank NMI pulse");
    expect_true(b->health_saw_nmi, "health saw NMI");
    expect_true(b->nmi_pulses >= 1, "NMI pulse count");
    expect_true(r01s_atmega1284p_oam_peek(&b->mcu1284, 0) == 0x10, "OAM Y=$10");
    expect_true(r01s_atmega1284p_oam_peek(&b->mcu1284, 1) == 0x01, "OAM tile=$01");
    expect_true(r01s_atmega1284p_oam_peek(&b->mcu1284, 3) == 0x20, "OAM X=$20");
    expect_true(b->linebuf_saw_mux_mcu, "linebuf mux MCU path");
    expect_true(b->linebuf_saw_mux_beam, "linebuf mux beam path");
    expect_true(b->health_saw_sprites ||
                    r01s_as6c62256_peek(&b->linebuf, 0x20) == 0x01 ||
                    r01s_as6c62256_peek(&b->linebuf, 0xA0) == 0x01,
                "linebuf has sprite color at X=$20");
    expect_true(r01s_as6c62256_peek(&b->vram, 0) == 0x42, "VRAM[0] final tile");
    expect_true(b->cycles > 0, "CPU cycles advanced");
    expect_true(r01s_bus_conflict_count() == 0, "no bus fight");

    {
        const char *paths[] = {"../retr01_studio/project.retr01", "../../retr01_studio/project.retr01",
                               "retr01_studio/project.retr01", NULL};
        int pi;
        int loaded = 0;
        for (pi = 0; paths[pi]; pi++) {
            if (r01s_board_load_cart(b, paths[pi]) == 0) {
                loaded = 1;
                expect_true(r01s_sst39sf040_peek(&b->cart_flash, 0) == 'R', "studio cart magic");
                expect_true(r01s_sst39sf040_peek(&b->cart_flash, b->cart_off_prg) == 0xA9,
                            "bring-up LDA overlay");
                expect_true(b->cart_off_chr != 0, "world0 CHR base from cart");
                expect_true(b->cart_off_map_screen0 != 0, "world0 screen0 MAP payload");
                r01s_island_group_reset(group);
                for (i = 0; i < 200000; i++) {
                    int vi;
                    int nz = 0;
                    r01s_island_group_step(group);
                    if ((i & 0x3FFF) != 0) {
                        continue;
                    }
                    for (vi = 0; vi < 240; vi++) {
                        if (r01s_as6c62256_peek(&b->vram, (uint16_t)vi) != 0) {
                            nz++;
                        }
                    }
                    if (nz > 40 &&
                        (b->active_pal[0] != 0 || b->active_pal[1] != 0 || b->active_pal[2] != 0)) {
                        break;
                    }
                }
                {
                    int nz = 0;
                    int vi;
                    for (vi = 0; vi < 240; vi++) {
                        if (r01s_as6c62256_peek(&b->vram, (uint16_t)vi) != 0) {
                            nz++;
                        }
                    }
                    expect_true(nz > 40, "bring-up MAP stream filled nametable");
                }
                expect_true(b->active_pal[0] != 0 || b->active_pal[1] != 0 || b->active_pal[2] != 0,
                            "bring-up loaded active palette via MAP");
                break;
            }
        }
        expect_true(loaded, "studio project.retr01 found");
    }

    r01s_island_builder_shutdown(&builder);
    return test_done("test_island_abcdeghiojklmnp");
}
