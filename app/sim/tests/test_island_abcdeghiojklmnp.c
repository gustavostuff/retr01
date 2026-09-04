#include "atmega1284p.h"
#include "atmega328p.h"
#include "as6c62256.h"
#include "at28c16.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "retr01_sim/board.h"
#include "retr01_sim/bom32.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/entity.h"
#include "retr01_sim/island_builder.h"
#include "atf22v10.h"
#include "sn74hc573.h"
#include "sst39sf040.h"
#include "test_common.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdio.h>
#include <string.h>

#ifndef R01_TEST_CART
#define R01_TEST_CART "../output/test.retr01"
#endif

/*
 * Layer-2 islands O + A + C + D + G + H + J + K + L smoke:
 *   prior milestones + sprites + VBlank NMI (~60 Hz class) + no bus fight.
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
    expect_true(r01s_island_group_count(group) == R01S_ISLAND_COUNT,
                "10 islands incl cart mod (no flasher)");
    expect_true(r01s_island_builder_count_bom_ic(&builder) == R01S_BOM_IC_N,
                "32 BOM IC visuals mounted");
    expect_true(r01s_island_group_at(group, R01S_ISLAND_VIDEO) != NULL, "video island present");
    expect_true(r01s_island_group_at(group, R01S_ISLAND_VIDEO)->title != NULL &&
                    strstr(r01s_island_group_at(group, R01S_ISLAND_VIDEO)->title, "VIDEO") != NULL,
                "video island titled");
    /* LCD island is first in arrange order -> top-left of the canvas pack. */
    {
        const R01sIsland *video = r01s_island_group_at(group, R01S_ISLAND_VIDEO);
        const R01sIsland *other;
        int oi;
        expect_true(video != NULL, "video island");
        for (oi = 0; oi < r01s_island_group_count(group); oi++) {
            if (oi == R01S_ISLAND_VIDEO) {
                continue;
            }
            other = r01s_island_group_at(group, oi);
            expect_true(other != NULL, "peer island");
            expect_true(video->board_y < other->board_y ||
                            (video->board_y == other->board_y && video->board_x <= other->board_x),
                        "video island is top-left among peers");
        }
    }

    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");
    expect_true(b->cart_loaded, "cart loaded");
    expect_true(r01s_sst39sf040_peek(&b->cart_module.flash, 0) == 'r', "flash magic r (retr01)");
    expect_true(r01s_sst39sf040_peek(&b->cart_module.flash, 1) == 'e', "flash magic e (retr01)");

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
            r01s_as6c62256_peek(&b->linebuf, 0x20) == 0x01 ||
            r01s_as6c62256_peek(&b->linebuf, (uint16_t)(16u * 128u + 0x20u)) == 0x01) {
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
    expect_true(saw_pad, "LDA $FE60 read pads (1284 island L)");
    expect_true(saw_beam_hblank, "island H HBlank");
    expect_true(saw_beam_line, "island H advanced past line 0");
    expect_true(saw_raster_hit, "beam-Y PLD EQ# on Y vs $FE04");
    expect_true(saw_bg_tile, "VRAM PLD latched nametable tile $42");
    expect_true(saw_bg_attr, "VRAM PLD latched nametable attr $07");
    expect_true(saw_video, "island O lit logical pixels from PROM");
    expect_true(saw_map, "island J MAP $FE93 read cart magic r");
    expect_true(saw_apu, "island K APU PWM tone edges");
    expect_true(saw_oam, "island L OAM $FE21 readback + clk");
    expect_true(saw_linebuf, "island L linebuf mux both paths");
    expect_true(saw_sprites, "1284 OAM fill -> VBlank sprite field");
    expect_true(saw_nmi, "beam island H VBlank NMI pulse");
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
        const char *paths[] = {R01_TEST_CART, "../output/test.retr01", "../../output/test.retr01",
                               "output/test.retr01", NULL};
        int pi;
        int loaded = 0;
        for (pi = 0; paths[pi]; pi++) {
            if (r01s_board_load_cart(b, paths[pi]) == 0) {
                loaded = 1;
                expect_true(r01s_sst39sf040_peek(&b->cart_module.flash, 0) == 'r', "studio cart magic");
                expect_true(r01s_sst39sf040_peek(&b->cart_module.flash, b->cart_off_prg) == 0x78,
                            "studio cart PRG SEI (not sim overlay)");
                expect_true(b->cart_off_chr != 0, "world0 CHR base from cart");
                expect_true(b->cart_off_map_screen0 != 0, "world0 start-screen MAP payload");
                r01s_island_group_reset(group);
                expect_true(r01s_board_catchup_bringup(b, group) == 0, "IC MAP stream catchup");
                expect_true(b->map_addr >= b->cart_off_map_screen0 + 480u, "MAP addr past start screen");
                {
                    uint8_t expect0 =
                        r01s_sst39sf040_peek(&b->cart_module.flash, b->cart_off_map_screen0);
                    expect_true(r01s_as6c62256_peek(&b->vram, 0) == expect0,
                                "VRAM[0] matches streamed MAP byte");
                }
                break;
            }
        }
        expect_true(loaded, "output/test.retr01 found");
    }

    r01s_island_builder_shutdown(&builder);
    return test_done("test_island_abcdeghiojklmnp");
}
