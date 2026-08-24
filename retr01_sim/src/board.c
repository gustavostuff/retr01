#include "retr01_sim/board.h"

#include "retr01_sim/board_fast.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/health.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R01S_RESET_HOLD 12
#define R01S_CART_HDR_SIZE 16
#define R01S_CART_PTR_SIZE 24
#define R01S_CART_PRG_BYTES 0x8000u
#define R01S_SETTLE_PASSES_FAST 1
/*
 * DOT/beam ticks per board step. Real silicon runs DOT ≈ PHI2 order; the UI
 * only does ~32 board steps/frame, so without a burst first VBlank takes minutes.
 * 128 dots/step * 32 steps/frame ≈ 4k dots/frame → VBlank in ~1 s wall.
 */
#define R01S_BEAM_DOTS_PER_STEP 128


/*
 * Bring-up smoke PRG (overlay into cart PRG window — not Studio game code).
 * Body through OAM readback is fixed. When cart meta is valid, install appends
 * pal+$FE08/$FE09 load and 480 B MAP→VRAM, then pad hang (addresses patched).
 * Ends with MAP seek 0 + LDA $FE93 ('R') BEFORE any MAP stream so island health sticks.
 */
static const uint8_t R01S_BRINGUP_SMOKE[] = {
    0xA9, 0x55,       /* LDA #$55 */
    0x8D, 0x02, 0xFE, /* STA $FE02 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x10, 0xFE, /* STA $FE10 */
    0x8D, 0x11, 0xFE, /* STA $FE11 */
    0xA9, 0xAA,       /* LDA #$AA */
    0x8D, 0x12, 0xFE, /* STA $FE12 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x10, 0xFE, /* STA $FE10 */
    0x8D, 0x11, 0xFE, /* STA $FE11 */
    0xAD, 0x12, 0xFE, /* LDA $FE12 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x02, 0xFE, /* STA $FE02 */
    0x8D, 0x03, 0xFE, /* STA $FE03 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x10, 0xFE, /* STA $FE10 */
    0x8D, 0x11, 0xFE, /* STA $FE11 */
    0xA9, 0x42,       /* LDA #$42 */
    0x8D, 0x12, 0xFE, /* STA $FE12 tile[0] */
    0xA9, 0xF0,       /* LDA #$F0 */
    0x8D, 0x10, 0xFE, /* STA $FE10 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x11, 0xFE, /* STA $FE11 */
    0xA9, 0x07,       /* LDA #$07 */
    0x8D, 0x12, 0xFE, /* STA $FE12 attr[0] */
    0xA9, 0x00,       /* LDA #$00 — MAP seek 0 */
    0x8D, 0x90, 0xFE, /* STA $FE90 */
    0x8D, 0x91, 0xFE, /* STA $FE91 */
    0x8D, 0x92, 0xFE, /* STA $FE92 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 expect $52 'R' */
    0xA9, 0x10,       /* LDA #$10 — APU period lo */
    0x8D, 0x41, 0xFE, /* STA $FE41 */
    0xA9, 0x00,       /* LDA #$00 — APU period hi */
    0x8D, 0x42, 0xFE, /* STA $FE42 */
    0xA9, 0x8F,       /* LDA #$8F — enable + vol 8 */
    0x8D, 0x40, 0xFE, /* STA $FE40 */
    0xA9, 0x00,       /* LDA #$00 — OAM addr 0 */
    0x8D, 0x20, 0xFE, /* STA $FE20 */
    0xA9, 0x10,       /* LDA #$10 — Y */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x01,       /* LDA #$01 — tile */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x00,       /* LDA #$00 — attr */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x20,       /* LDA #$20 — X */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x20, 0xFE, /* STA $FE20 */
    0xAD, 0x21, 0xFE, /* LDA $FE21 expect $10 */
};

/* Pad hang only (used when cart has no world-0 MAP/CHR meta). */
static const uint8_t R01S_BRINGUP_HANG[] = {
    0xAD, 0x60, 0xFE, /* LDA $FE60 */
    0x4C, 0x00, 0x80, /* JMP hang — lo patched at install */
};

/*
 * Palette + MAP stream tail. Immediates for seeks patched at install.
 * Layout after smoke:
 *   LDA #pal_lo / STA $FE90 / LDA #pal_mid / STA $FE91 / LDA #pal_hi / STA $FE92
 *   LDA #$00 / STA $FE08
 *   LDX #32 / loop: LDA $FE93 / STA $FE09 / DEX / BNE
 *   LDA #map_lo..hi seek
 *   LDA #$00 / STA $FE10 / STA $FE11
 *   LDX #240 / copy / LDX #240 / copy
 *   hang
 */
enum {
    R01S_BR_OFF_PAL_LO = 1,
    R01S_BR_OFF_PAL_MID = 6,
    R01S_BR_OFF_PAL_HI = 11,
    R01S_BR_OFF_MAP_LO = 32,
    R01S_BR_OFF_MAP_MID = 37,
    R01S_BR_OFF_MAP_HI = 42,
};

static const uint8_t R01S_BRINGUP_STREAM[] = {
    0xA9, 0x00,       /* LDA #pal_lo */
    0x8D, 0x90, 0xFE, /* STA $FE90 */
    0xA9, 0x00,       /* LDA #pal_mid */
    0x8D, 0x91, 0xFE, /* STA $FE91 */
    0xA9, 0x00,       /* LDA #pal_hi */
    0x8D, 0x92, 0xFE, /* STA $FE92 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x08, 0xFE, /* STA $FE08 */
    0xA2, 0x20,       /* LDX #32 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 */
    0x8D, 0x09, 0xFE, /* STA $FE09 */
    0xCA,             /* DEX */
    0xD0, 0xF7,       /* BNE *-9 */
    0xA9, 0x00,       /* LDA #map_lo */
    0x8D, 0x90, 0xFE, /* STA $FE90 */
    0xA9, 0x00,       /* LDA #map_mid */
    0x8D, 0x91, 0xFE, /* STA $FE91 */
    0xA9, 0x00,       /* LDA #map_hi */
    0x8D, 0x92, 0xFE, /* STA $FE92 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x10, 0xFE, /* STA $FE10 */
    0x8D, 0x11, 0xFE, /* STA $FE11 */
    0xA2, 0xF0,       /* LDX #240 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 */
    0x8D, 0x12, 0xFE, /* STA $FE12 */
    0xCA,             /* DEX */
    0xD0, 0xF7,       /* BNE *-9 */
    0xA2, 0xF0,       /* LDX #240 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 */
    0x8D, 0x12, 0xFE, /* STA $FE12 */
    0xCA,             /* DEX */
    0xD0, 0xF7,       /* BNE *-9 */
};


static void put_u24(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
}

static uint32_t get_u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static R01sBoard *board_from_group(R01sIslandGroup *group) {
    return group ? (R01sBoard *)group->impl : NULL;
}

R01sBoard *r01s_board_from_group(R01sIslandGroup *group) {
    return board_from_group(group);
}

static const char *phase_name(R01sCpuPhase p) {
    switch (p) {
    case R01S_CPU_RES_HOLD:
        return "HOLD";
    case R01S_CPU_RES_WAIT:
        return "RWAIT";
    case R01S_CPU_VEC_PCL:
        return "VPCL";
    case R01S_CPU_VEC_PCH:
        return "VPCH";
    case R01S_CPU_FETCH:
        return "FETCH";
    case R01S_CPU_OP_IMM:
        return "IMM";
    case R01S_CPU_OP_ADL:
        return "ADL";
    case R01S_CPU_OP_ADH:
        return "ADH";
    case R01S_CPU_OP_DATA:
        return "DATA";
    default:
        return "?";
    }
}

static void board_update_milestones(R01sBoard *ctx) {
    if (!ctx) {
        return;
    }
    if (r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]) == 0x55) {
        ctx->health_saw_latch = 1;
    }
    if (r01s_as6c62256_peek(ctx->vram_impl.vram, 0) == 0xAA) {
        ctx->health_saw_vram = 1;
    }
    if (r01s_w65c02s_a(ctx->cpu_mem_impl.cpu) == 0xAA) {
        ctx->health_saw_vram_read = 1;
    }
    /* Pad milestone is set in wire_io when CPU actually reads $FE60/$FE61
     * (not A==$A5 — that value is only used by the unit-test preload). */
    if (r01s_beam_xy_hblank(ctx->beam_impl.beam_x) || r01s_beam_xy_y(ctx->beam_impl.beam_x) > 0) {
        ctx->health_saw_beam = 1;
    }
    /* Smoke latches tile $42; after MAP stream VRAM is world data — either is OK. */
    if (r01s_bg_fetch_count(ctx->bg_fetch_impl.fetch) > 0) {
        uint8_t tile = r01s_bg_fetch_last_tile(ctx->bg_fetch_impl.fetch);
        if (tile == 0x42 ||
            (ctx->cart_off_map_screen0 != 0 &&
             ctx->map_addr >= ctx->cart_off_map_screen0 + 480u)) {
            ctx->health_saw_bg_fetch = 1;
        }
    }
    if (r01s_video_sink_lit_pixels(ctx->video_impl.sink) > 64) {
        ctx->health_saw_video = 1;
    }
    if (r01s_atmega328p_enabled(ctx->apu_impl.apu) && r01s_atmega328p_pwm_edges(ctx->apu_impl.apu) >= 2) {
        ctx->health_saw_apu = 1;
    }
    if (r01s_atmega1284p_oam_peek(ctx->mcu1284_impl.mcu, 0) == 0x10 &&
        r01s_atmega1284p_oam_peek(ctx->mcu1284_impl.mcu, 1) == 0x01 &&
        r01s_atmega1284p_alive(ctx->mcu1284_impl.mcu)) {
        ctx->health_saw_oam = 1;
    }
    if (ctx->linebuf_saw_mux_mcu && ctx->linebuf_saw_mux_beam) {
        ctx->health_saw_linebuf = 1;
    }

}

static int board_integrated(const R01sBoard *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->health_saw_latch && ctx->health_saw_vram && ctx->health_saw_vram_read && ctx->health_saw_pad &&
           ctx->health_saw_beam && ctx->health_saw_bg_fetch && ctx->health_saw_video && ctx->health_saw_map &&
           ctx->health_saw_apu && ctx->health_saw_oam && ctx->health_saw_linebuf &&
           ctx->health_saw_sprites && ctx->health_saw_nmi;
}


static void board_fill_health(R01sIslandGroup *group, R01sSystemHealth *out) {
    R01sBoard *ctx = board_from_group(group);
    R01sHealth system = R01S_HEALTH_OK;
    int integrated;
    int booting;
    unsigned conflicts;
    int i;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->island_count = group ? group->island_count : 0;
    if (!ctx || !group) {
        out->system = R01S_HEALTH_FAIL;
        snprintf(out->system_label, sizeof(out->system_label), "NO BOARD");
        snprintf(out->system_detail, sizeof(out->system_detail), "Island group not wired");
        return;
    }

    integrated = board_integrated(ctx);
    booting = ctx->reset_hold > 0;
    conflicts = r01s_bus_conflict_count();

    /* Island A — power */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_POWER];
        R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_impl.pwr);
        ih->letter = 'A';
        if (!group->powered) {
            ih->health = R01S_HEALTH_FAIL;
            snprintf(ih->activity, sizeof(ih->activity), "power switch off");
        } else if (!r01s_level_is_high(r01s_entity_sense(pwr, "VDD"))) {
            ih->health = R01S_HEALTH_FAIL;
            snprintf(ih->activity, sizeof(ih->activity), "5V rail missing");
        } else {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "5V rail up");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=A POWER health=%s powered=%d VDD=%s", r01s_health_tag(ih->health),
                 group->powered, r01s_level_name(r01s_entity_sense(pwr, "VDD")));
    }

    /* Island B — clock / reset */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_CLOCK];
        R01sEntity *osc = r01s_osc8m_entity(ctx->clock_impl.osc);
        ih->letter = 'B';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "reset hold (%d)", ctx->reset_hold);
        } else if (!group->running) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "clock halted (SPACE)");
        } else if (ctx->health_phi2_edges < 2) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "PHI2 starting");
        } else {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "8MHz PHI2 running");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=B CLOCK health=%s running=%d reset_hold=%d PHI2=%s edges=%u",
                 r01s_health_tag(ih->health), group->running, ctx->reset_hold,
                 r01s_level_name(r01s_entity_sense(osc, "PHI2")), (unsigned)ctx->health_phi2_edges);
    }

    /* Island C — CPU / RAM / PRG */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_CPU];
        R01sW65C02S *cpu = ctx->cpu_mem_impl.cpu;
        R01sEntity *cpu_e = r01s_w65c02s_entity(cpu);
        ih->letter = 'C';
        if (conflicts > 0) {
            ih->health = R01S_HEALTH_FAIL;
            snprintf(ih->activity, sizeof(ih->activity), "bus fight (%u)", conflicts);
        } else if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "CPU in reset");
        } else if (!group->running && ctx->cycles == 0) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "waiting for clock");
        } else {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "%s PC=%04X cyc=%u",
                     phase_name(r01s_w65c02s_phase(cpu)), r01s_w65c02s_pc(cpu), (unsigned)ctx->cycles);
            if (!group->running) {
                ih->health = R01S_HEALTH_WARN;
            }
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=C CPU health=%s phase=%s PC=%04X A=%02X IR=%02X AB=%04X RWB=%s RESB=%s "
                 "BE=%s cyc=%u conflicts=%u",
                 r01s_health_tag(ih->health), phase_name(r01s_w65c02s_phase(cpu)),
                 r01s_w65c02s_pc(cpu), r01s_w65c02s_a(cpu), cpu->ir,
                 (unsigned)r01s_bus_read(cpu_e, "A", 16),
                 r01s_level_name(r01s_entity_sense(cpu_e, "RWB")),
                 r01s_level_name(r01s_entity_sense(cpu_e, "RESB")),
                 r01s_level_name(r01s_entity_sense(cpu_e, "BE")), (unsigned)ctx->cycles, conflicts);
    }

    /* Island D — $FExx latches */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_IO_LATCH];
        uint8_t le = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
        uint8_t ry = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE04]);
        ih->letter = 'D';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "await boot STA $FE02");
        } else if (ctx->health_saw_latch) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "scroll latched $%02X", le);
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await STA $FE02 ($%02X)", le);
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=D IO_LATCH health=%s saw_latch=%d FE02_Q=$%02X FE04_Q=$%02X expect_FE02=$55",
                 r01s_health_tag(ih->health), ctx->health_saw_latch, le, ry);
    }

    /* Island E — pads */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_PADS];
        uint8_t p1 = r01s_pads_get(ctx->pads_impl.pads, 0);
        uint8_t p2 = r01s_pads_get(ctx->pads_impl.pads, 1);
        R01sEntity *pads = r01s_pads_entity(ctx->pads_impl.pads);
        ih->letter = 'E';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "pads idle");
        } else if (ctx->health_saw_pad) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "LDA $FE60 ok P1=$%02X", p1);
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await CPU pad read");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=E PADS health=%s saw_pad_read=%d P1_FE60=$%02X P2_FE61=$%02X CE#=%s OE#=%s "
                 "CPU_A=%02X (boot loops LDA $FE60; idle pads often $00)",
                 r01s_health_tag(ih->health), ctx->health_saw_pad, p1, p2,
                 r01s_level_name(r01s_entity_sense(pads, "CE#")),
                 r01s_level_name(r01s_entity_sense(pads, "OE#")),
                 r01s_w65c02s_a(ctx->cpu_mem_impl.cpu));
    }

    /* Island G — VRAM */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_VRAM];
        uint8_t v0 = r01s_as6c62256_peek(ctx->vram_impl.vram, 0);
        ih->letter = 'G';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "VRAM idle");
        } else if (ctx->health_saw_vram && ctx->health_saw_vram_read) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "VRAM[0]=$%02X readback ok", v0);
        } else if (ctx->health_saw_vram) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await LDA $FE12 readback");
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await STA $FE12 ($%02X)", v0);
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=G VRAM health=%s saw_write=%d saw_readback=%d VA=$%04X VRAM[0]=$%02X "
                 "expect=$AA fe12_armed=%d",
                 r01s_health_tag(ih->health), ctx->health_saw_vram, ctx->health_saw_vram_read,
                 (unsigned)(ctx->vram_addr & 0x7FFFu), v0, ctx->vram_fe12_armed);
    }

    /* Island H — beam */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_BEAM];
        int bx = r01s_beam_xy_x(ctx->beam_impl.beam_x);
        int by = r01s_beam_xy_y(ctx->beam_impl.beam_x);
        int hb = r01s_beam_xy_hblank(ctx->beam_impl.beam_x);
        int vb = r01s_beam_xy_vblank(ctx->beam_impl.beam_x);
        ih->letter = 'H';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "beam idle");
        } else if (!group->running) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "raster frozen %d,%d", bx, by);
        } else if (ctx->health_saw_beam) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "scan %d,%d%s%s", bx, by, hb ? " HB" : "",
                     vb ? " VB" : "");
        } else {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "beam starting");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=H BEAM health=%s saw_beam=%d X=%d Y=%d HBlank=%d VBlank=%d EQ#=%s FE04=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_beam, bx, by, hb, vb,
                 r01s_level_name(r01s_entity_sense(r01s_atf22v10_entity(ctx->beam_impl.beam_y), "EQ#")),
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE04]));
    }

    /* Island I — BG nametable fetch */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_BG_FETCH];
        R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
        ih->letter = 'I';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "BG fetch idle");
        } else if (ctx->health_saw_bg_fetch) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "NT tile=$%02X attr=$%02X",
                     r01s_bg_fetch_last_tile(bg), r01s_bg_fetch_last_attr(bg));
        } else if (r01s_bg_fetch_count(bg) > 0) {
            ih->health = R01S_HEALTH_WARN;
            if (ctx->cart_off_map_screen0 &&
                ctx->map_addr < ctx->cart_off_map_screen0 + 480u) {
                snprintf(ih->activity, sizeof(ih->activity), "await MAP stream (tile=$%02X)",
                         r01s_bg_fetch_last_tile(bg));
            } else {
                snprintf(ih->activity, sizeof(ih->activity), "fetching tile=$%02X",
                         r01s_bg_fetch_last_tile(bg));
            }
        } else if (!group->running) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await PPU fetches");
        } else {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "await visible fetch");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=I BG_FETCH health=%s saw=$%02X n=%u VA=$%04X active=%d attr=%d TILE=$%02X "
                 "ATTR=$%02X SX=$%02X SY=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_bg_fetch, (unsigned)r01s_bg_fetch_count(bg),
                 (unsigned)r01s_bg_fetch_va(bg), r01s_bg_fetch_active(bg), r01s_bg_fetch_attr_cycle(bg),
                 r01s_bg_fetch_last_tile(bg), r01s_bg_fetch_last_attr(bg),
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]),
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]));
    }

    /* Island O — Color PROM + compositor + LCD sink */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_VIDEO];
        R01sVideoSink *sink = ctx->video_impl.sink;
        ih->letter = 'O';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "video idle");
        } else if (ctx->health_saw_video) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "RGBS %u px lit",
                     (unsigned)r01s_video_sink_lit_pixels(sink));
        } else if (r01s_video_sink_lit_pixels(sink) > 0) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "ramp %u px",
                     (unsigned)r01s_video_sink_lit_pixels(sink));
        } else if (!group->running) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await dot stream");
        } else {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "await visible dots");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=O VIDEO health=%s saw=%d lit=%u samples=%u comp_out=$%02X prom0=$%02X "
                 "pixel00=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_video,
                 (unsigned)r01s_video_sink_lit_pixels(sink), (unsigned)sink->dot_samples,
                 r01s_compositor_out(ctx->video_impl.comp),
                 r01s_at28c16_peek(ctx->video_impl.prom, 0),
                 r01s_video_sink_pixel_packed(sink, 0, 0));
    }

    /* Island J — cart flash SST39SF040 */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_CART];
        ih->letter = 'J';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "cart idle");
        } else if (!ctx->cart_loaded) {
            ih->health = R01S_HEALTH_FAIL;
            snprintf(ih->activity, sizeof(ih->activity), "no cart image");
        } else if (ctx->health_saw_map) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "MAP $FE93 ok MAP=$%06X",
                     (unsigned)(ctx->map_addr & 0xFFFFFFu));
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await LDA $FE93 ('R')");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=J CART health=%s loaded=%d label=%s off_prg=$%06X len_prg=$%04X map=$%06X "
                 "saw_map=%d flash0=$%02X",
                 r01s_health_tag(ih->health), ctx->cart_loaded, ctx->cart_label[0] ? ctx->cart_label : "-",
                 (unsigned)ctx->cart_off_prg, (unsigned)ctx->cart_len_prg, (unsigned)(ctx->map_addr & 0xFFFFFFu),
                 ctx->health_saw_map, r01s_sst39sf040_peek(ctx->cart_impl.flash, 0));
    }

    /* Island K — ATmega328P APU */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_APU];
        R01sAtmega328p *apu = ctx->apu_impl.apu;
        ih->letter = 'K';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "APU idle");
        } else if (ctx->health_saw_apu) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "tone PWM edges=%u",
                     (unsigned)r01s_atmega328p_pwm_edges(apu));
        } else if (r01s_atmega328p_enabled(apu)) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "enabled period=%u",
                     (unsigned)r01s_atmega328p_period(apu));
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await STA $FE40");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=K APU health=%s saw=%d en=%d FE40=$%02X period=%u edges=%u hi=%u PWM=%s",
                 r01s_health_tag(ih->health), ctx->health_saw_apu, r01s_atmega328p_enabled(apu),
                 r01s_atmega328p_peek(apu, 0), (unsigned)r01s_atmega328p_period(apu),
                 (unsigned)r01s_atmega328p_pwm_edges(apu), (unsigned)r01s_atmega328p_pwm_hi_samples(apu),
                 r01s_level_name(r01s_entity_sense(r01s_atmega328p_entity(apu), "PWM")));
    }

    /* Island L — ATmega1284P OAM / 20 MHz stub */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_MCU1284];
        R01sAtmega1284p *mcu = ctx->mcu1284_impl.mcu;
        ih->letter = 'L';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "1284 idle");
        } else if (ctx->health_saw_oam) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "OAM Y=$%02X clk=%u",
                     r01s_atmega1284p_oam_peek(mcu, 0), (unsigned)r01s_atmega1284p_clk_ticks(mcu));
        } else if (r01s_atmega1284p_alive(mcu)) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "clk ok await OAM");
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await 20 MHz / OAM");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=L MCU1284 health=%s saw_oam=%d alive=%d oam0=$%02X oam1=$%02X addr=$%02X "
                 "clk=%u RUN=%s",
                 r01s_health_tag(ih->health), ctx->health_saw_oam, r01s_atmega1284p_alive(mcu),
                 r01s_atmega1284p_oam_peek(mcu, 0), r01s_atmega1284p_oam_peek(mcu, 1),
                 r01s_atmega1284p_oam_addr(mcu), (unsigned)r01s_atmega1284p_clk_ticks(mcu),
                 r01s_level_name(r01s_entity_sense(r01s_atmega1284p_entity(mcu), "RUN")));
    }

    /* Island M — sprite line-buffer SRAM + HC157 mux */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_LINEBUF];
        R01sAs6c62256 *lb = ctx->linebuf_impl.sram;
        ih->letter = 'M';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "linebuf idle");
        } else if (ctx->health_saw_linebuf) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "ping-pong show=%u",
                     (unsigned)ctx->linebuf_show_half);
        } else if (ctx->linebuf_saw_mux_mcu || ctx->linebuf_saw_mux_beam) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "mux mcu=%d beam=%d", ctx->linebuf_saw_mux_mcu,
                     ctx->linebuf_saw_mux_beam);
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await HBlank fill");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=M LINEBUF health=%s saw=%d show=%u mux_mcu=%d mux_beam=%d "
                 "half0=$%02X half1=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_linebuf, (unsigned)ctx->linebuf_show_half,
                 ctx->linebuf_saw_mux_mcu, ctx->linebuf_saw_mux_beam, r01s_as6c62256_peek(lb, 0x00),
                 r01s_as6c62256_peek(lb, 0x80));
    }

    /* Island N — OAM → linebuf → compositor */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_SPRITES];
        R01sSpriteFetch *sf = ctx->sprites_impl.fetch;
        ih->letter = 'N';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "sprites idle");
        } else if (ctx->health_saw_sprites) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "spr ly=$%02X x=$%02X c=$%02X",
                     r01s_sprite_fetch_last_ly(sf), r01s_sprite_fetch_last_hit_x(sf),
                     r01s_sprite_fetch_last_hit_color(sf));
        } else if (r01s_sprite_fetch_fill_count(sf) > 0) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "fills=%u await OAM hit",
                     (unsigned)r01s_sprite_fetch_fill_count(sf));
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await HBlank OAM fill");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=N SPRITES health=%s saw=%d fills=%u px=%u last_ly=$%02X hit_x=$%02X "
                 "hit_c=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_sprites,
                 (unsigned)r01s_sprite_fetch_fill_count(sf), (unsigned)r01s_sprite_fetch_pixel_count(sf),
                 r01s_sprite_fetch_last_ly(sf), r01s_sprite_fetch_last_hit_x(sf),
                 r01s_sprite_fetch_last_hit_color(sf));
    }

    /* Island P — integration: pads + video + NMI ~60 Hz class + no bus fight */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_INTEGRATION];
        R01sIntegration *ig = ctx->integration_impl.integ;
        unsigned pulses = (unsigned)r01s_integration_nmi_pulses(ig);
        int ok = board_integrated(ctx) && conflicts == 0;
        ih->letter = 'P';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "integration idle");
        } else if (ok) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "NMI x%u pads+video ok", pulses);
        } else if (ctx->health_saw_nmi) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "NMI x%u await islands", pulses);
        } else {
            int by = r01s_beam_xy_y(ctx->beam_impl.beam_x);
            ih->health = R01S_HEALTH_WARN;
            /* Show scan progress — first VBlank is Y=240 (slow if UI steps/frame is tiny). */
            snprintf(ih->activity, sizeof(ih->activity), "beam Y=%d/240 await NMI", by);
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=P INTEGRATION health=%s saw_nmi=%d pulses=%u conflicts=%u integrated=%d",
                 r01s_health_tag(ih->health), ctx->health_saw_nmi, pulses, conflicts, integrated);
    }

    for (i = 0; i < out->island_count; i++) {

        system = r01s_health_worst(system, out->islands[i].health);
    }

    if (conflicts > 0) {
        out->system = R01S_HEALTH_FAIL;
        snprintf(out->system_label, sizeof(out->system_label), "BUS FAULT");
        snprintf(out->system_detail, sizeof(out->system_detail), "%u bus conflict(s) — check wiring", conflicts);
    } else if (!group->powered || out->islands[R01S_ISLAND_POWER].health == R01S_HEALTH_FAIL) {
        out->system = R01S_HEALTH_FAIL;
        snprintf(out->system_label, sizeof(out->system_label), "POWER FAULT");
        snprintf(out->system_detail, sizeof(out->system_detail), "Island A must be up before integration");
    } else if (booting) {
        out->system = R01S_HEALTH_BOOT;
        snprintf(out->system_label, sizeof(out->system_label), "BOOTING");
        snprintf(out->system_detail, sizeof(out->system_detail), "Reset release — islands starting");
    } else if (!integrated) {
        out->system = R01S_HEALTH_WARN;
        snprintf(out->system_label, sizeof(out->system_label), "BRING-UP");
        snprintf(out->system_detail, sizeof(out->system_detail),
                 "L=%s V=%s P=%s B=%s BG=%s O=%s J=%s K=%s 1284=%s M=%s N=%s NMI=%s",
                 ctx->health_saw_latch ? "ok" : "-", ctx->health_saw_vram_read ? "ok" : "-",
                 ctx->health_saw_pad ? "ok" : "-", ctx->health_saw_beam ? "ok" : "-",
                 ctx->health_saw_bg_fetch ? "ok" : "-", ctx->health_saw_video ? "ok" : "-",
                 ctx->health_saw_map ? "ok" : "-", ctx->health_saw_apu ? "ok" : "-",
                 ctx->health_saw_oam ? "ok" : "-", ctx->health_saw_linebuf ? "ok" : "-",
                 ctx->health_saw_sprites ? "ok" : "-", ctx->health_saw_nmi ? "ok" : "-");


    } else if (system == R01S_HEALTH_WARN) {
        out->system = R01S_HEALTH_WARN;
        snprintf(out->system_label, sizeof(out->system_label), "PAUSED");
        snprintf(out->system_detail, sizeof(out->system_detail),
                 group->running ? "One island idle or waiting" : "Integrated — press SPACE to run");
    } else {
        out->system = R01S_HEALTH_OK;
        snprintf(out->system_label, sizeof(out->system_label), "INTEGRATED");
        snprintf(out->system_detail, sizeof(out->system_detail),
                 group->running ? "All islands working together" : "All islands ok — paused");
    }

    {
        size_t used = 0;
        int n = snprintf(out->system_debug, sizeof(out->system_debug),
                         "retr01_sim health system=%s label=%s detail=%s running=%d powered=%d cyc=%u "
                         "conflicts=%u milestones latch=%d vram_w=%d vram_r=%d pad=%d beam=%d bg=%d video=%d "
                         "map=%d apu=%d oam=%d linebuf=%d sprites=%d nmi=%d\n",
                         r01s_health_tag(out->system), out->system_label, out->system_detail,
                         group->running, group->powered, (unsigned)ctx->cycles, conflicts,
                         ctx->health_saw_latch, ctx->health_saw_vram, ctx->health_saw_vram_read,
                         ctx->health_saw_pad, ctx->health_saw_beam, ctx->health_saw_bg_fetch,
                         ctx->health_saw_video, ctx->health_saw_map, ctx->health_saw_apu,
                         ctx->health_saw_oam, ctx->health_saw_linebuf, ctx->health_saw_sprites, ctx->health_saw_nmi);

        if (n > 0) {
            used = (size_t)n;
        }
        for (i = 0; i < out->island_count && used + 2 < sizeof(out->system_debug); i++) {
            n = snprintf(out->system_debug + used, sizeof(out->system_debug) - used, "%s\n",
                         out->islands[i].debug);
            if (n <= 0) {
                break;
            }
            used += (size_t)n;
        }
    }
}

static void copy_bus_named(R01sBoard *ctx, R01sEntity *dst, const char *dst_prefix, R01sEntity *src,
                           const char *src_prefix, int width);

static int board_pin_idx(R01sEntity *e, const char *name) {
    R01sPin *p;
    if (!e || !name) {
        return -1;
    }
    p = r01s_entity_pin_named(e, name);
    if (!p) {
        return -1;
    }
    return (int)(p - e->pins);
}

static void board_wire_cache_fill_idx(int *out, int n, R01sEntity *e, const char *prefix, int start) {
    int i;
    for (i = 0; i < n; i++) {
        char name[8];
        snprintf(name, sizeof(name), "%s%d", prefix, start + i);
        out[i] = board_pin_idx(e, name);
    }
}

static void board_wire_cache_fill_latch_dq(R01sBoard *ctx) {
    R01sEntity *latch = r01s_sn74hc573_entity(&ctx->latch573[R01S_LATCH_FE02]);
    int i;
    for (i = 0; i < R01S_WIRE_CACHE_D; i++) {
        char dn[8], qn[8];
        snprintf(dn, sizeof(dn), "%dD", i + 1);
        snprintf(qn, sizeof(qn), "%dQ", i + 1);
        ctx->wire_cache.latch_d[i] = board_pin_idx(latch, dn);
        ctx->wire_cache.latch_q[i] = board_pin_idx(latch, qn);
    }
}

static void board_wire_cache_build(R01sBoard *ctx) {
    R01sBoardWireCache *wc;
    R01sEntity *cpu;
    R01sEntity *ram;
    R01sEntity *prg;
    R01sEntity *flash;
    R01sEntity *vram;
    R01sEntity *mcu;
    R01sEntity *apu;
    R01sEntity *pads;

    if (!ctx || ctx->wire_cache.built) {
        return;
    }
    wc = &ctx->wire_cache;
    cpu = r01s_w65c02s_entity(&ctx->cpu);
    ram = r01s_as6c62256_entity(&ctx->ram);
    prg = r01s_prg_rom_entity(&ctx->prg);
    flash = r01s_sst39sf040_entity(&ctx->cart_flash);
    vram = r01s_as6c62256_entity(&ctx->vram);
    mcu = r01s_atmega1284p_entity(&ctx->mcu1284);
    apu = r01s_atmega328p_entity(&ctx->apu);
    pads = r01s_pads_entity(&ctx->pads);

    board_wire_cache_fill_idx(wc->cpu_a, R01S_WIRE_CACHE_A, cpu, "A", 0);
    board_wire_cache_fill_idx(wc->cpu_d, R01S_WIRE_CACHE_D, cpu, "D", 0);
    wc->cpu_rwb = board_pin_idx(cpu, "RWB");
    wc->cpu_be = board_pin_idx(cpu, "BE");
    board_wire_cache_fill_idx(wc->ram_a, 15, ram, "A", 0);
    board_wire_cache_fill_idx(wc->ram_dq, R01S_WIRE_CACHE_D, ram, "DQ", 0);
    board_wire_cache_fill_idx(wc->prg_a, 15, prg, "A", 0);
    board_wire_cache_fill_idx(wc->prg_dq, R01S_WIRE_CACHE_D, prg, "DQ", 0);
    board_wire_cache_fill_idx(wc->flash_dq, R01S_WIRE_CACHE_D, flash, "DQ", 0);
    board_wire_cache_fill_idx(wc->vram_dq, R01S_WIRE_CACHE_D, vram, "DQ", 0);
    board_wire_cache_fill_idx(wc->mcu_dq, R01S_WIRE_CACHE_D, mcu, "DQ", 0);
    board_wire_cache_fill_idx(wc->apu_dq, R01S_WIRE_CACHE_D, apu, "DQ", 0);
    board_wire_cache_fill_idx(wc->pads_dq, R01S_WIRE_CACHE_D, pads, "DQ", 0);
    board_wire_cache_fill_latch_dq(ctx);
    wc->built = 1;
}

static void copy_bus_idx(R01sEntity *dst, const int *dst_idx, R01sEntity *src, const int *src_idx, int width) {
    int i;
    for (i = 0; i < width; i++) {
        int di = dst_idx[i];
        int si = src_idx[i];
        if (di >= 0 && si >= 0) {
            dst->pins[di].level = src->pins[si].level;
        }
    }
}

static int board_fast_cpu_path(const R01sBoard *ctx) {
    return ctx &&
           (r01s_fast_glue_enabled(R01S_FAST_GLUE_PINS) || r01s_fast_glue_enabled(R01S_FAST_GLUE_MEMORY));
}

static uint16_t board_cpu_addr(R01sBoard *ctx, R01sEntity *cpu_e) {
    if (board_fast_cpu_path(ctx)) {
        return r01s_w65c02s_ab(&ctx->cpu);
    }
    return (uint16_t)r01s_bus_read(cpu_e, "A", 16);
}

static int board_cpu_read(R01sBoard *ctx, R01sEntity *cpu_e) {
    if (board_fast_cpu_path(ctx)) {
        return r01s_w65c02s_rwb(&ctx->cpu) != 0;
    }
    return r01s_level_is_high(r01s_entity_sense(cpu_e, "RWB"));
}

static int board_cpu_be(R01sBoard *ctx, R01sEntity *cpu_e) {
    if (r01s_fast_glue_enabled(R01S_FAST_GLUE_PINS)) {
        board_wire_cache_build(ctx);
        if (ctx->wire_cache.cpu_be >= 0) {
            return r01s_level_is_high(cpu_e->pins[ctx->wire_cache.cpu_be].level);
        }
    }
    return r01s_level_is_high(r01s_entity_sense(cpu_e, "BE"));
}

static uint8_t board_cpu_d_sample(R01sBoard *ctx, R01sEntity *cpu_e) {
    if (board_fast_cpu_path(ctx) && !r01s_w65c02s_rwb(&ctx->cpu)) {
        return r01s_w65c02s_a(&ctx->cpu);
    }
    return (uint8_t)r01s_bus_read(cpu_e, "D", 8);
}

static void copy_bus_named(R01sBoard *ctx, R01sEntity *dst, const char *dst_prefix, R01sEntity *src,
                           const char *src_prefix, int width) {
    int i;
    char dn[16], sn[16];

    if (ctx && r01s_fast_glue_enabled(R01S_FAST_GLUE_PINS)) {
        R01sBoardWireCache *wc;
        R01sEntity *cpu = r01s_w65c02s_entity(&ctx->cpu);
        R01sEntity *ram = r01s_as6c62256_entity(&ctx->ram);
        R01sEntity *prg = r01s_prg_rom_entity(&ctx->prg);
        R01sEntity *flash = r01s_sst39sf040_entity(&ctx->cart_flash);
        R01sEntity *vram = r01s_as6c62256_entity(&ctx->vram);
        R01sEntity *mcu = r01s_atmega1284p_entity(&ctx->mcu1284);
        R01sEntity *apu = r01s_atmega328p_entity(&ctx->apu);
        R01sEntity *pads = r01s_pads_entity(&ctx->pads);

        board_wire_cache_build(ctx);
        wc = &ctx->wire_cache;
        if (width == 15 && dst_prefix[0] == 'A' && src_prefix[0] == 'A') {
            if (dst == ram && src == cpu) {
                copy_bus_idx(dst, wc->ram_a, src, wc->cpu_a, 15);
                return;
            }
            if (dst == prg && src == cpu) {
                copy_bus_idx(dst, wc->prg_a, src, wc->cpu_a, 15);
                return;
            }
        }
        if (width == 8 && dst_prefix[0] == 'D' && src_prefix[0] == 'D') {
            if (dst == cpu && src == ram) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->ram_dq, 8);
                return;
            }
            if (dst == cpu && src == prg) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->prg_dq, 8);
                return;
            }
            if (dst == cpu && src == flash) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->flash_dq, 8);
                return;
            }
            if (dst == cpu && src == vram) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->vram_dq, 8);
                return;
            }
            if (dst == cpu && src == mcu) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->mcu_dq, 8);
                return;
            }
            if (dst == cpu && src == apu) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->apu_dq, 8);
                return;
            }
            if (dst == cpu && src == pads) {
                copy_bus_idx(dst, wc->cpu_d, src, wc->pads_dq, 8);
                return;
            }
        }
        if (width == 8 && dst_prefix[0] == 'D' && src_prefix[0] == 'D' && src == cpu) {
            if (dst == ram) {
                copy_bus_idx(dst, wc->ram_dq, src, wc->cpu_d, 8);
                return;
            }
            if (dst == vram) {
                copy_bus_idx(dst, wc->vram_dq, src, wc->cpu_d, 8);
                return;
            }
            if (dst == mcu) {
                copy_bus_idx(dst, wc->mcu_dq, src, wc->cpu_d, 8);
                return;
            }
            if (dst == apu) {
                copy_bus_idx(dst, wc->apu_dq, src, wc->cpu_d, 8);
                return;
            }
        }
    }

    for (i = 0; i < width; i++) {
        snprintf(dn, sizeof(dn), "%s%d", dst_prefix, i);
        snprintf(sn, sizeof(sn), "%s%d", src_prefix, i);
        r01s_entity_drive(dst, dn, r01s_entity_sense(src, sn));
    }
}

static void copy_cpu_d_to_latch_d(R01sBoard *ctx, R01sEntity *latch, R01sEntity *cpu) {
    int i;
    char ln[8], cn[8];

    if (ctx && r01s_fast_glue_enabled(R01S_FAST_GLUE_PINS)) {
        board_wire_cache_build(ctx);
        copy_bus_idx(latch, ctx->wire_cache.latch_d, cpu, ctx->wire_cache.cpu_d, R01S_WIRE_CACHE_D);
        return;
    }
    for (i = 0; i < 8; i++) {
        snprintf(ln, sizeof(ln), "%dD", i + 1);
        snprintf(cn, sizeof(cn), "D%d", i);
        r01s_entity_drive(latch, ln, r01s_entity_sense(cpu, cn));
    }
}

static void copy_latch_q_to_cpu_d(R01sBoard *ctx, R01sEntity *cpu, R01sEntity *latch) {
    int i;
    char ln[8], cn[8];

    if (ctx && r01s_fast_glue_enabled(R01S_FAST_GLUE_PINS)) {
        board_wire_cache_build(ctx);
        copy_bus_idx(cpu, ctx->wire_cache.cpu_d, latch, ctx->wire_cache.latch_q, R01S_WIRE_CACHE_D);
        return;
    }
    for (i = 0; i < 8; i++) {
        snprintf(ln, sizeof(ln), "%dQ", i + 1);
        snprintf(cn, sizeof(cn), "D%d", i);
        r01s_entity_drive(cpu, cn, r01s_entity_sense(latch, ln));
    }
}

static void drive_level_bit(R01sEntity *e, const char *name, int bit_on) {
    r01s_entity_drive(e, name, bit_on ? R01S_LVL_H : R01S_LVL_L);
}

static int addr_is_io(uint16_t addr) {
    return addr >= 0xFE00u && addr <= 0xFEFFu;
}

/* Soft PLD: $FE02–$FE04 latches + $FE40–$FE5F APU + $FE60/$FE61 pads + $FE90–$FE93 MAP. */
static void flash_deselect(R01sEntity *flash) {
    r01s_entity_drive(flash, "CE#", R01S_LVL_H);
    r01s_entity_drive(flash, "OE#", R01S_LVL_H);
    r01s_entity_drive(flash, "WE#", R01S_LVL_H);
    r01s_bus_hiz(flash, "DQ", 8);
    r01s_entity_eval(flash);
}

static void flash_drive_abs(R01sEntity *flash, uint32_t abs) {
    static const char *const names[19] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6",  "A7",  "A8",
                                          "A9",  "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17",
                                          "A18"};
    int i;
    abs &= (R01S_FLASH_BYTES - 1u);
    for (i = 0; i < 19; i++) {
        r01s_entity_drive(flash, names[i], (abs & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void flash_read_selected(R01sEntity *flash, uint32_t abs) {
    flash_drive_abs(flash, abs);
    r01s_entity_drive(flash, "CE#", R01S_LVL_L);
    r01s_entity_drive(flash, "OE#", R01S_LVL_L);
    r01s_entity_drive(flash, "WE#", R01S_LVL_H);
    r01s_entity_eval(flash);
}

static void wire_io(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *latch = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
    R01sEntity *scroll_y = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]);
    R01sEntity *raster = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE04]);
    R01sEntity *pads = r01s_pads_entity(ctx->pads_impl.pads);
    R01sEntity *apu = r01s_atmega328p_entity(ctx->apu_impl.apu);
    R01sEntity *mcu = r01s_atmega1284p_entity(ctx->mcu1284_impl.mcu);
    R01sEntity *flash = r01s_sst39sf040_entity(ctx->cart_impl.flash);
    uint16_t addr = board_cpu_addr(ctx, cpu);
    int read = board_cpu_read(ctx, cpu);
    int be = board_cpu_be(ctx, cpu);
    int hit_latch = (addr == 0xFE02u);
    int hit_scroll_y = (addr == 0xFE03u);
    int hit_raster = (addr == 0xFE04u);
    int hit_oam_addr = (addr == 0xFE20u);
    int hit_oam_data = (addr == 0xFE21u);
    int hit_oam = hit_oam_addr || hit_oam_data;
    int hit_eeprom = (addr >= 0xFE70u && addr <= 0xFE72u);
    int hit_apu = (addr >= 0xFE40u && addr <= 0xFE5Fu);
    int hit_pads = (addr == 0xFE60u || addr == 0xFE61u);
    int hit_map_lo = (addr == 0xFE90u);
    int hit_map_mid = (addr == 0xFE91u);
    int hit_map_hi = (addr == 0xFE92u);
    int hit_map_data = (addr == 0xFE93u);
    int hit_pal_addr = (addr == 0xFE08u);
    int hit_pal_data = (addr == 0xFE09u);
    int ai;

    /* Default: I/O devices idle */
    r01s_entity_drive(latch, "OE", R01S_LVL_L); /* Q visible */
    r01s_entity_drive(latch, "LE", R01S_LVL_L);
    r01s_entity_drive(scroll_y, "OE", R01S_LVL_L);
    r01s_entity_drive(scroll_y, "LE", R01S_LVL_L);
    r01s_entity_drive(raster, "OE", R01S_LVL_L);
    r01s_entity_drive(raster, "LE", R01S_LVL_L);
    r01s_entity_drive(pads, "CE#", R01S_LVL_H);
    r01s_entity_drive(pads, "OE#", R01S_LVL_H);
    r01s_entity_drive(pads, "A0", R01S_LVL_L);
    r01s_entity_drive(apu, "CE#", R01S_LVL_H);
    r01s_entity_drive(apu, "OE#", R01S_LVL_H);
    r01s_entity_drive(apu, "WE#", R01S_LVL_H);
    r01s_entity_drive(apu, "RESET#", (ctx->reset_hold > 0) ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(mcu, "CE#", R01S_LVL_H);
    r01s_entity_drive(mcu, "OE#", R01S_LVL_H);
    r01s_entity_drive(mcu, "WE#", R01S_LVL_H);
    r01s_entity_drive(mcu, "A0", R01S_LVL_L);
    r01s_entity_drive(mcu, "RESET#", (ctx->reset_hold > 0) ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(mcu, "CLK", R01S_LVL_H);
    r01s_entity_drive(mcu, "HBLANK",
                      r01s_beam_xy_hblank(ctx->beam_impl.beam_x) ? R01S_LVL_H : R01S_LVL_L);

    if (!be || !addr_is_io(addr)) {
        r01s_entity_eval(latch);
        r01s_entity_eval(scroll_y);
        r01s_entity_eval(raster);
        r01s_entity_eval(pads);
        r01s_entity_eval(apu);
        r01s_entity_eval(mcu);
        return;
    }

    if (hit_pads) {
        r01s_entity_drive(pads, "A0", (addr == 0xFE61u) ? R01S_LVL_H : R01S_LVL_L);
    }

    if (hit_oam) {
        r01s_entity_drive(mcu, "A0", hit_oam_data ? R01S_LVL_H : R01S_LVL_L);
    }

    if (hit_apu) {
        unsigned reg = (unsigned)(addr - 0xFE40u);
        for (ai = 0; ai < 5; ai++) {
            char an[4];
            snprintf(an, sizeof(an), "A%d", ai);
            r01s_entity_drive(apu, an, (reg & (1u << ai)) ? R01S_LVL_H : R01S_LVL_L);
        }
    }

    if (hit_latch) {
        copy_cpu_d_to_latch_d(ctx, latch, cpu);
        if (!read) {
            r01s_entity_drive(latch, "LE", R01S_LVL_H);
        }
        r01s_entity_eval(latch);
        if (read) {
            copy_latch_q_to_cpu_d(ctx, cpu, latch);
        }
    } else {
        r01s_entity_eval(latch);
    }

    if (hit_scroll_y) {
        copy_cpu_d_to_latch_d(ctx, scroll_y, cpu);
        if (!read) {
            r01s_entity_drive(scroll_y, "LE", R01S_LVL_H);
        }
        r01s_entity_eval(scroll_y);
        if (read) {
            copy_latch_q_to_cpu_d(ctx, cpu, scroll_y);
        }
    } else {
        r01s_entity_eval(scroll_y);
    }

    if (hit_raster) {
        copy_cpu_d_to_latch_d(ctx, raster, cpu);
        if (!read) {
            r01s_entity_drive(raster, "LE", R01S_LVL_H);
        }
        r01s_entity_eval(raster);
        if (read) {
            copy_latch_q_to_cpu_d(ctx, cpu, raster);
        }
    } else {
        r01s_entity_eval(raster);
    }

    /* Island L — OAM $FE20/$FE21 (hold WE#/OE# across settle; chip edge-inc). */
    if (hit_oam) {
        r01s_entity_drive(mcu, "CE#", R01S_LVL_L);
        if (read) {
            r01s_entity_drive(mcu, "OE#", R01S_LVL_L);
            r01s_entity_drive(mcu, "WE#", R01S_LVL_H);
            r01s_entity_eval(mcu);
            copy_bus_named(ctx, cpu, "D", mcu, "DQ", 8);
            if (hit_oam_data && board_cpu_d_sample(ctx, cpu) == 0x10) {
                ctx->health_saw_oam = 1;
            }
        } else {
            r01s_entity_drive(mcu, "OE#", R01S_LVL_H);
            r01s_entity_drive(mcu, "WE#", R01S_LVL_L);
            copy_bus_named(ctx, mcu, "DQ", cpu, "D", 8);
            r01s_entity_eval(mcu);
        }
    } else {
        r01s_entity_eval(mcu);
    }

    /* Soft $FE70–$FE72 machine-EEPROM mailbox (protocol TBD — Island F). */
    if (hit_eeprom) {
        unsigned ei = (unsigned)(addr - 0xFE70u);
        if (read) {
            r01s_bus_write(cpu, "D", 8, r01s_atmega1284p_eeprom_peek(ctx->mcu1284_impl.mcu, ei));
        } else {
            r01s_atmega1284p_eeprom_poke(ctx->mcu1284_impl.mcu, ei, board_cpu_d_sample(ctx, cpu));
        }
    }

    if (hit_apu) {
        r01s_entity_drive(apu, "CE#", R01S_LVL_L);
        if (read) {
            r01s_entity_drive(apu, "OE#", R01S_LVL_L);
            r01s_entity_drive(apu, "WE#", R01S_LVL_H);
            r01s_entity_eval(apu);
            copy_bus_named(ctx, cpu, "D", apu, "DQ", 8);
        } else {
            r01s_entity_drive(apu, "OE#", R01S_LVL_H);
            r01s_entity_drive(apu, "WE#", R01S_LVL_L);
            copy_bus_named(ctx, apu, "DQ", cpu, "D", 8);
            r01s_entity_eval(apu);
            r01s_entity_drive(apu, "WE#", R01S_LVL_H);
            r01s_entity_eval(apu);
        }
    } else {
        r01s_entity_eval(apu);
    }

    if (hit_pads && read) {
        r01s_entity_drive(pads, "CE#", R01S_LVL_L);
        r01s_entity_drive(pads, "OE#", R01S_LVL_L);
        r01s_entity_eval(pads);
        copy_bus_named(ctx, cpu, "D", pads, "DQ", 8);
        ctx->health_saw_pad = 1;
    } else {
        r01s_entity_eval(pads);
    }

    /* Island J — MAP seek + auto-inc read (flash CE only here; PRG path leaves flash alone). */
    if (hit_map_lo && !read) {
        ctx->map_addr = (ctx->map_addr & 0xFFFF00u) | board_cpu_d_sample(ctx, cpu);
    }
    if (hit_map_mid && !read) {
        ctx->map_addr = (ctx->map_addr & 0xFF00FFu) | ((uint32_t)board_cpu_d_sample(ctx, cpu) << 8);
    }
    if (hit_map_hi && !read) {
        ctx->map_addr = (ctx->map_addr & 0x00FFFFu) | ((uint32_t)board_cpu_d_sample(ctx, cpu) << 16);
    }
    if (hit_map_lo && read) {
        r01s_bus_write(cpu, "D", 8, (uint8_t)(ctx->map_addr & 0xFFu));
    }
    if (hit_map_mid && read) {
        r01s_bus_write(cpu, "D", 8, (uint8_t)((ctx->map_addr >> 8) & 0xFFu));
    }
    if (hit_map_hi && read) {
        r01s_bus_write(cpu, "D", 8, (uint8_t)((ctx->map_addr >> 16) & 0xFFu));
    }
    if (hit_map_data && read && ctx->cart_loaded &&
        r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu) == R01S_CPU_OP_DATA) {
        uint8_t dq;
        if (r01s_fast_glue_enabled(R01S_FAST_GLUE_MEMORY)) {
            dq = r01s_sst39sf040_peek(ctx->cart_impl.flash, ctx->map_addr);
            r01s_bus_write(cpu, "D", 8, dq);
        } else {
            flash_read_selected(flash, ctx->map_addr);
            copy_bus_named(ctx, cpu, "D", flash, "DQ", 8);
            dq = board_cpu_d_sample(ctx, cpu);
        }
        if (dq == 0x52) {
            ctx->health_saw_map = 1; /* cart magic 'R' at seek 0 */
        }
        ctx->map_fe93_armed = 1;
    } else if (hit_map_data) {
        flash_deselect(flash);
    }

    /* Soft $FE08/$FE09 active palette (auto-inc on data write). */
    if (hit_pal_addr && !read) {
        ctx->pal_addr = (uint8_t)(board_cpu_d_sample(ctx, cpu) & 0x1Fu);
    }
    if (hit_pal_addr && read) {
        r01s_bus_write(cpu, "D", 8, ctx->pal_addr);
    }
    if (hit_pal_data && !read &&
        r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu) == R01S_CPU_OP_DATA && !ctx->pal_fe09_wrote) {
        ctx->active_pal[ctx->pal_addr & 0x1Fu] = board_cpu_d_sample(ctx, cpu);
        ctx->pal_addr = (uint8_t)((ctx->pal_addr + 1u) & 0x1Fu);
        ctx->pal_fe09_wrote = 1;
    }
    if (hit_pal_data && read) {
        r01s_bus_write(cpu, "D", 8, ctx->active_pal[ctx->pal_addr & 0x1Fu]);
    }
}

/* Island H — DOT osc + beam PLD + Y-compare vs $FE04; EQ# drives CPU IRQB. */
static void wire_beam(R01sBoard *ctx, R01sIslandGroup *group) {
    R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_impl.pwr);
    R01sEntity *osc = r01s_osc_dot_entity(ctx->beam_impl.osc_dot);
    R01sEntity *beam = r01s_beam_xy_entity(ctx->beam_impl.beam_x);
    R01sEntity *beam_y = r01s_atf22v10_entity(ctx->beam_impl.beam_y);
    R01sEntity *raster = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE04]);
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sLevel vdd = r01s_entity_sense(pwr, "VDD");
    R01sLevel resb = (ctx->reset_hold > 0) ? R01S_LVL_L : R01S_LVL_H;
    int i;
    char pn[8], qn[8], yn[8];

    (void)group;
    r01s_entity_drive(osc, "VDD", vdd);
    r01s_entity_drive(osc, "OE#", R01S_LVL_H);
    r01s_entity_drive(beam, "RES#", resb);
    r01s_entity_drive(beam, "DOT", r01s_entity_sense(osc, "DOT"));

    /* P = beam Y[7:0], Q = $FE04 latch */
    for (i = 0; i < 8; i++) {
        snprintf(pn, sizeof(pn), "P%d", i);
        snprintf(yn, sizeof(yn), "Y%d", i);
        snprintf(qn, sizeof(qn), "Q%d", i);
        r01s_entity_drive(beam_y, pn, r01s_entity_sense(beam, yn));
        {
            char ln[8];
            snprintf(ln, sizeof(ln), "%dQ", i + 1);
            r01s_entity_drive(beam_y, qn, r01s_entity_sense(raster, ln));
        }
    }
    r01s_entity_drive(beam_y, "OE#", R01S_LVL_L);
    r01s_entity_eval(beam);
    r01s_entity_eval(beam_y);
    /* Active-low raster match → IRQB (CPU IRQ service still Phase-1 stub). */
    r01s_entity_drive(cpu, "IRQB", r01s_entity_sense(beam_y, "EQ#"));
}

/*
 * Island G — VRAM port $FE10/$FE11/$FE12 + PHI2 interleave.
 * CPU phase (PHI2 high): CPU may R/W via soft addr latch + FE12.
 * PPU phase (PHI2 low): mux selects Island I BG fetch VA; VRAM OE for nametable.
 * Auto-inc arms on FE12 access; committed on next PHI2 rising edge.
 */
static void wire_vram(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *vram = r01s_as6c62256_entity(ctx->vram_impl.vram);
    R01sEntity *mux = r01s_sn74hc157_entity(ctx->vram_impl.mux157[R01S_MUX157_VRAM0]);
    R01sEntity *osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
    uint16_t cpu_addr = board_cpu_addr(ctx, cpu);
    int read = board_cpu_read(ctx, cpu);
    int be = board_cpu_be(ctx, cpu);
    int cpu_phase = r01s_level_is_high(r01s_entity_sense(osc, "PHI2"));
    int hit_lo = (cpu_addr == 0xFE10u);
    int hit_hi = (cpu_addr == 0xFE11u);
    int hit_data = (cpu_addr == 0xFE12u);
    uint16_t va = (uint16_t)(ctx->vram_addr & 0x7FFFu);
    uint16_t ppu_va = (uint16_t)(r01s_bg_fetch_va(bg) & 0x7FFFu);
    uint16_t sram_addr;
    int i;

    /* Soft addr latch ports (always CPU-side, not PHI2-gated). */
    if (be && addr_is_io(cpu_addr)) {
        if (hit_lo && !read) {
            ctx->vram_addr = (uint16_t)((ctx->vram_addr & 0xFF00u) | board_cpu_d_sample(ctx, cpu));
            va = (uint16_t)(ctx->vram_addr & 0x7FFFu);
        }
        if (hit_hi && !read) {
            ctx->vram_addr =
                (uint16_t)((ctx->vram_addr & 0x00FFu) | ((uint16_t)board_cpu_d_sample(ctx, cpu) << 8));
            va = (uint16_t)(ctx->vram_addr & 0x7FFFu);
        }
        if (hit_lo && read) {
            r01s_bus_write(cpu, "D", 8, (uint8_t)(ctx->vram_addr & 0xFFu));
        }
        if (hit_hi && read) {
            r01s_bus_write(cpu, "D", 8, (uint8_t)((ctx->vram_addr >> 8) & 0xFFu));
        }
    }

    /* HC157: A = CPU VRAM addr[3:0], B = PPU fetch VA[3:0], AB = !cpu_phase */
    r01s_entity_drive(mux, "G#", R01S_LVL_L);
    r01s_entity_drive(mux, "AB", cpu_phase ? R01S_LVL_L : R01S_LVL_H);
    drive_level_bit(mux, "1A", (va >> 0) & 1);
    drive_level_bit(mux, "2A", (va >> 1) & 1);
    drive_level_bit(mux, "3A", (va >> 2) & 1);
    drive_level_bit(mux, "4A", (va >> 3) & 1);
    drive_level_bit(mux, "1B", (ppu_va >> 0) & 1);
    drive_level_bit(mux, "2B", (ppu_va >> 1) & 1);
    drive_level_bit(mux, "3B", (ppu_va >> 2) & 1);
    drive_level_bit(mux, "4B", (ppu_va >> 3) & 1);
    r01s_entity_eval(mux);

    sram_addr = cpu_phase ? va : ppu_va;
    /* Low 4 bits from mux Y (visual interleave); upper from selected side. */
    for (i = 0; i < 4; i++) {
        char yn[4], an[8];
        snprintf(yn, sizeof(yn), "%dY", i + 1);
        snprintf(an, sizeof(an), "A%d", i);
        r01s_entity_drive(vram, an, r01s_entity_sense(mux, yn));
    }
    for (i = 4; i < 15; i++) {
        char an[8];
        snprintf(an, sizeof(an), "A%d", i);
        drive_level_bit(vram, an, (sram_addr >> i) & 1);
    }

    r01s_entity_drive(vram, "CE#", R01S_LVL_H);
    r01s_entity_drive(vram, "OE#", R01S_LVL_H);
    r01s_entity_drive(vram, "WE#", R01S_LVL_H);
    r01s_bus_hiz(vram, "DQ", 8);

    if (cpu_phase && be && hit_data &&
        r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu) == R01S_CPU_OP_DATA) {
        r01s_entity_drive(vram, "CE#", R01S_LVL_L);
        if (read) {
            r01s_entity_drive(vram, "OE#", R01S_LVL_L);
            r01s_entity_drive(vram, "WE#", R01S_LVL_H);
            r01s_entity_eval(vram);
            copy_bus_named(ctx, cpu, "D", vram, "DQ", 8);
        } else {
            r01s_entity_drive(vram, "OE#", R01S_LVL_H);
            r01s_entity_drive(vram, "WE#", R01S_LVL_L);
            copy_bus_named(ctx, vram, "DQ", cpu, "D", 8);
            r01s_entity_eval(vram);
        }
        ctx->vram_fe12_armed = 1;
    } else if (!cpu_phase && r01s_bg_fetch_active(bg)) {
        r01s_entity_drive(vram, "CE#", R01S_LVL_L);
        r01s_entity_drive(vram, "OE#", R01S_LVL_L);
        r01s_entity_drive(vram, "WE#", R01S_LVL_H);
        r01s_entity_eval(vram);
        {
            uint8_t dq = (uint8_t)r01s_bus_read(vram, "DQ", 8);
            r01s_bg_fetch_capture_dq(bg, dq);
        }
    } else {
        r01s_entity_eval(vram);
    }
}

/*
 * Island M — sprite line-buffer SRAM (no CPU port).
 * Soft 1284 fill on HBlank entry; beam reads show half on visible dots.
 * HC157: AB low = MCU fill addr, AB high = beam X.
 */
static void linebuf_drive_addr(R01sBoard *ctx, uint16_t addr, int mcu_sel) {
    R01sEntity *sram = r01s_as6c62256_entity(ctx->linebuf_impl.sram);
    R01sEntity *mux = r01s_sn74hc157_entity(ctx->linebuf_impl.mux157[R01S_MUX157_LINEBUF0]);
    int i;
    uint8_t lo = (uint8_t)(addr & 0x0Fu);

    r01s_entity_drive(mux, "G#", R01S_LVL_L);
    r01s_entity_drive(mux, "AB", mcu_sel ? R01S_LVL_L : R01S_LVL_H);
    /* Same lo on A and B so Y matches; SEL still exercises the mux path. */
    drive_level_bit(mux, "1A", (lo >> 0) & 1);
    drive_level_bit(mux, "2A", (lo >> 1) & 1);
    drive_level_bit(mux, "3A", (lo >> 2) & 1);
    drive_level_bit(mux, "4A", (lo >> 3) & 1);
    drive_level_bit(mux, "1B", (lo >> 0) & 1);
    drive_level_bit(mux, "2B", (lo >> 1) & 1);
    drive_level_bit(mux, "3B", (lo >> 2) & 1);
    drive_level_bit(mux, "4B", (lo >> 3) & 1);
    r01s_entity_eval(mux);

    for (i = 0; i < 4; i++) {
        char yn[4], an[8];
        snprintf(yn, sizeof(yn), "%dY", i + 1);
        snprintf(an, sizeof(an), "A%d", i);
        r01s_entity_drive(sram, an, r01s_entity_sense(mux, yn));
    }
    for (i = 4; i < 15; i++) {
        char an[8];
        snprintf(an, sizeof(an), "A%d", i);
        drive_level_bit(sram, an, (addr >> i) & 1);
    }
}

static void linebuf_write_byte(R01sBoard *ctx, uint16_t addr, uint8_t data) {
    R01sEntity *sram = r01s_as6c62256_entity(ctx->linebuf_impl.sram);
    linebuf_drive_addr(ctx, addr, 1);
    ctx->linebuf_saw_mux_mcu = 1;
    r01s_entity_drive(sram, "CE#", R01S_LVL_L);
    r01s_entity_drive(sram, "OE#", R01S_LVL_H);
    r01s_entity_drive(sram, "WE#", R01S_LVL_L);
    r01s_bus_write(sram, "DQ", 8, data);
    r01s_entity_eval(sram);
    r01s_entity_drive(sram, "WE#", R01S_LVL_H);
    r01s_entity_eval(sram);
    r01s_entity_drive(sram, "CE#", R01S_LVL_H);
    r01s_bus_hiz(sram, "DQ", 8);
}

#define R01S_CHR_TILE_BYTES 16u
#define R01S_CHR_BANK_BYTES 0x1000u
#define R01S_ATTR_BANK 0x03u
#define R01S_ATTR_PAL 0x0Cu
#define R01S_ATTR_PAL_SHIFT 2
#define R01S_ATTR_FLIP_H 0x10u
#define R01S_ATTR_FLIP_V 0x20u

static uint8_t board_flash_byte(const R01sBoard *ctx, uint32_t abs) {
    if (!ctx || abs >= sizeof(ctx->cart_flash.mem)) {
        return 0xFF;
    }
    return ctx->cart_flash.mem[abs];
}

static uint8_t board_chr_color(const R01sBoard *ctx, uint32_t chr_base, uint8_t tile, uint8_t attr,
                               int px, int py) {
    uint8_t bank = (uint8_t)(attr & R01S_ATTR_BANK);
    int row = py & 7;
    int col = px & 7;
    uint32_t tbase;
    uint8_t p0, p1;
    int bit;

    if (attr & R01S_ATTR_FLIP_V) {
        row = 7 - row;
    }
    if (attr & R01S_ATTR_FLIP_H) {
        col = 7 - col;
    }
    tbase = chr_base + (uint32_t)bank * R01S_CHR_BANK_BYTES + (uint32_t)tile * R01S_CHR_TILE_BYTES;
    p0 = board_flash_byte(ctx, tbase + (uint32_t)row);
    p1 = board_flash_byte(ctx, tbase + 8u + (uint32_t)row);
    bit = 7 - col;
    return (uint8_t)(((p1 >> bit) & 1u) << 1) | (uint8_t)((p0 >> bit) & 1u);
}

static uint8_t board_pal_master(const R01sBoard *ctx, int sprite, uint8_t pal, uint8_t color) {
    uint8_t idx;
    if (color == 0) {
        return (uint8_t)(ctx->active_pal[0] & 63u); /* shared backdrop */
    }
    idx = (uint8_t)((sprite ? 16u : 0u) + (uint8_t)((pal & 3u) * 4u) + (color & 3u));
    return (uint8_t)(ctx->active_pal[idx & 31u] & 63u);
}

static void board_vram_cell_at(const R01sBoard *ctx, int lx, int ly, uint8_t *tile_out, uint8_t *attr_out) {
    uint8_t sx;
    uint8_t sy;
    int slot_x, slot_y, slot, local_x, local_y, tx, ty, cell;
    uint16_t addr;
    uint8_t scroll_x = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
    uint8_t scroll_y = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]);

    *tile_out = 0;
    *attr_out = 0;
    if (lx < 0 || ly < 0 || lx >= R01S_VIDEO_W || ly >= R01S_VIDEO_H) {
        return;
    }
    sx = (uint8_t)((scroll_x + (unsigned)lx) & 127u);
    sy = (uint8_t)(scroll_y + (unsigned)ly);
    if (sy >= 120u) {
        sy = 119u;
    }
    slot_x = (sx / R01S_BG_SCREEN_PX_W) & 1;
    slot_y = (sy / R01S_BG_SCREEN_PX_H) & 1;
    slot = slot_y * 2 + slot_x;
    local_x = (int)sx - slot_x * R01S_BG_SCREEN_PX_W;
    local_y = (int)sy - slot_y * R01S_BG_SCREEN_PX_H;
    tx = local_x / 8;
    ty = local_y / 8;
    if (tx >= R01S_BG_SCREEN_TILES_X) {
        tx = R01S_BG_SCREEN_TILES_X - 1;
    }
    if (ty > 14) {
        ty = 14;
    }
    cell = ty * R01S_BG_SCREEN_TILES_X + tx;
    addr = (uint16_t)(slot * R01S_BG_SLOT_BYTES + cell);
    *tile_out = r01s_as6c62256_peek(ctx->vram_impl.vram, addr);
    *attr_out = r01s_as6c62256_peek(ctx->vram_impl.vram, (uint16_t)(addr - cell + R01S_BG_ATTR_OFF + cell));
}

static uint8_t board_bg_master_at(R01sBoard *ctx, int lx, int ly) {
    uint8_t tile, attr, color, pal, master;
    int local_x, local_y;
    uint8_t scroll_x, scroll_y, sx, sy;

    if (ctx->cart_off_chr == 0) {
        board_vram_cell_at(ctx, lx, ly, &tile, &attr);
        master = (uint8_t)(tile & 0x3Fu);
        ctx->chr_last_master = master;
        return master;
    }
    board_vram_cell_at(ctx, lx, ly, &tile, &attr);
    scroll_x = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
    scroll_y = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]);
    sx = (uint8_t)((scroll_x + (unsigned)lx) & 127u);
    sy = (uint8_t)(scroll_y + (unsigned)ly);
    if (sy >= 120u) {
        sy = 119u;
    }
    local_x = (int)(sx % R01S_BG_SCREEN_PX_W);
    local_y = (int)(sy % R01S_BG_SCREEN_PX_H);
    color = board_chr_color(ctx, ctx->cart_off_chr, tile, attr, local_x & 7, local_y & 7);
    pal = (uint8_t)((attr & R01S_ATTR_PAL) >> R01S_ATTR_PAL_SHIFT);
    master = board_pal_master(ctx, 0, pal, color);
    ctx->chr_last_master = master;
    return master;
}



/* Island N: clear half, OAM-scan logical Y, paint ≤16 sprites (CHR from flash when meta). */
static void linebuf_oam_fill_half(R01sBoard *ctx, int half, int logical_y) {
    int i;
    int si;
    int painted = 0;
    uint32_t pixels = 0;
    uint8_t hit_x = 0;
    uint8_t hit_color = 0;
    uint16_t base = (uint16_t)((half & 1) << 7);
    R01sAtmega1284p *mcu = ctx->mcu1284_impl.mcu;
    R01sSpriteFetch *sf = ctx->sprites_impl.fetch;

    for (i = 0; i < 128; i++) {
        linebuf_write_byte(ctx, (uint16_t)(base + (unsigned)i), 0);
    }

    for (si = 0; si < 64 && painted < 16; si++) {
        uint8_t oy = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 0));
        uint8_t tile = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 1));
        uint8_t attr = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 2));
        uint8_t ox = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 3));
        int h = (attr & 0x80u) ? 16 : 8;
        int px;

        if (logical_y < (int)oy || logical_y >= (int)oy + h) {
            continue;
        }
        painted++;
        {
            int row = logical_y - (int)oy;
            uint8_t pal = (uint8_t)((attr & R01S_ATTR_PAL) >> R01S_ATTR_PAL_SHIFT);
            uint32_t spr_chr = ctx->cart_off_chr ? (ctx->cart_off_chr + 4u * R01S_CHR_BANK_BYTES) : 0;
            for (px = 0; px < 8; px++) {
                int x = (int)ox + px;
                uint8_t master;
                if (x < 0 || x >= 128) {
                    continue;
                }
                if (spr_chr) {
                    uint8_t c2 = board_chr_color(ctx, spr_chr, tile, attr, px, row);
                    if (c2 == 0) {
                        continue;
                    }
                    master = board_pal_master(ctx, 1, pal, c2);
                } else {
                    master = (uint8_t)(tile & 0x3Fu);
                    if (master == 0) {
                        continue;
                    }
                }
                linebuf_write_byte(ctx, (uint16_t)(base + (unsigned)x), master);
                pixels++;
                if (!hit_color) {
                    hit_x = (uint8_t)x;
                    hit_color = master;
                }
            }
        }
    }

    r01s_sprite_fetch_note_fill(sf, (uint8_t)(logical_y & 0xFF), (uint8_t)painted, pixels, hit_x,
                                hit_color);
    /* Opaque pixels or an OAM hit on this line (CHR may be blank for smoke tile). */
    if (pixels > 0 || painted > 0) {
        ctx->health_saw_sprites = 1;
    }
}


static void wire_linebuf(R01sBoard *ctx) {
    R01sEntity *sram = r01s_as6c62256_entity(ctx->linebuf_impl.sram);
    int hblank = r01s_beam_xy_hblank(ctx->beam_impl.beam_x);
    int bx = r01s_beam_xy_x(ctx->beam_impl.beam_x);
    int by = r01s_beam_xy_y(ctx->beam_impl.beam_x);
    int lx;
    uint16_t show_addr;

    /* Entering HBlank: OAM-fill next half for next logical Y, then show it. */
    if (hblank && !ctx->linebuf_prev_hblank) {
        int next_by = by + 1;
        int next_ly;
        int fill_half;
        if (next_by >= R01S_BEAM_DOTS_Y) {
            next_by = 0;
        }
        next_ly = next_by / 2;
        fill_half = ctx->linebuf_show_half ^ 1;
        linebuf_oam_fill_half(ctx, fill_half, next_ly);
        ctx->linebuf_show_half = (uint8_t)(fill_half & 1);
    }
    ctx->linebuf_prev_hblank = (uint8_t)(hblank ? 1 : 0);

    r01s_entity_drive(sram, "CE#", R01S_LVL_H);
    r01s_entity_drive(sram, "OE#", R01S_LVL_H);
    r01s_entity_drive(sram, "WE#", R01S_LVL_H);
    r01s_bus_hiz(sram, "DQ", 8);

    lx = bx / 2;
    if (!hblank && bx >= 0 && bx < R01S_BEAM_VISIBLE_W && lx >= 0 && lx < 128) {
        show_addr = (uint16_t)(((ctx->linebuf_show_half & 1u) << 7) | (lx & 0x7F));
        linebuf_drive_addr(ctx, show_addr, 0);
        ctx->linebuf_saw_mux_beam = 1;
        r01s_entity_drive(sram, "CE#", R01S_LVL_L);
        r01s_entity_drive(sram, "OE#", R01S_LVL_L);
        r01s_entity_drive(sram, "WE#", R01S_LVL_H);
        r01s_entity_eval(sram);
    } else {
        r01s_entity_eval(sram);
    }
}


/* Island I — BG fetch address from beam + scroll; eval before VRAM uses VA. */
static void wire_bg_fetch(R01sBoard *ctx) {
    R01sEntity *osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
    int cpu_phase = r01s_level_is_high(r01s_entity_sense(osc, "PHI2"));

    r01s_bg_fetch_set_beam(bg, r01s_beam_xy_x(ctx->beam_impl.beam_x), r01s_beam_xy_y(ctx->beam_impl.beam_x),
                           r01s_beam_xy_hblank(ctx->beam_impl.beam_x),
                           r01s_beam_xy_vblank(ctx->beam_impl.beam_x));
    r01s_bg_fetch_set_scroll(bg, r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]),
                             r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]));
    r01s_bg_fetch_set_cpu_phase(bg, cpu_phase);
    r01s_entity_eval(r01s_bg_fetch_entity(bg));
}



static void wire_video_prom_addr(R01sEntity *prom, uint8_t index) {
    int i;
    char name[4];
    for (i = 0; i < 6; i++) {
        snprintf(name, sizeof(name), "A%d", i);
        r01s_entity_drive(prom, name, (index & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
    r01s_entity_drive(prom, "CE#", R01S_LVL_L);
    r01s_entity_drive(prom, "OE#", R01S_LVL_L);
    r01s_entity_drive(prom, "WE#", R01S_LVL_H);
    r01s_entity_eval(prom);
}

/* Hold LCD while bring-up MAP-streams tiles then attrs (avoids sky→unflipped→flipped). */
static int board_video_held_for_map_stream(const R01sBoard *ctx) {
    if (!ctx || ctx->cart_off_map_screen0 == 0) {
        return 0;
    }
    return ctx->map_addr < ctx->cart_off_map_screen0 + 480u;
}

/*
 * Playbook Pass 3 — inline Island O glue (same priority as compositor PLD, no entity eval).
 */
static void wire_video_dot_fast(R01sBoard *ctx) {
    R01sBeamXy *beam = ctx->beam_impl.beam_x;
    R01sAt28c16 *prom = ctx->video_impl.prom;
    R01sVideoSink *sink = ctx->video_impl.sink;
    int bx = r01s_beam_xy_x(beam);
    int by = r01s_beam_xy_y(beam);
    int lx;
    int ly;
    uint8_t bg;
    uint8_t spr;
    uint8_t idx;
    uint8_t packed;

    if (r01s_beam_xy_hblank(beam) || r01s_beam_xy_vblank(beam) || bx >= R01S_BEAM_VISIBLE_W ||
        by >= R01S_BEAM_VISIBLE_H) {
        return;
    }
    if (board_video_held_for_map_stream(ctx)) {
        return;
    }
    lx = bx / 2;
    ly = by / 2;
    bg = board_bg_master_at(ctx, lx, ly);
    spr = r01s_as6c62256_peek(ctx->linebuf_impl.sram,
                              (uint16_t)(((ctx->linebuf_show_half & 1u) << 7) | (lx & 0x7F)));
    if (spr != 0) {
        ctx->health_saw_sprites = 1;
    }
    if (spr != 0 && (spr & 0x3Fu) != 0) {
        idx = (uint8_t)(spr & 0x3Fu);
    } else {
        idx = (uint8_t)(bg & 0x3Fu);
    }
    packed = r01s_at28c16_peek(prom, idx);
    r01s_video_sink_plot(sink, lx, ly, packed);
}

/*
 * Island O — dot-sampled BG -> compositor -> Color PROM -> LCD sink.
 * CHR: behavioral flash peek (no /CE fight with PRG/MAP); 2bpp + $FE08/$FE09.
 */
static void wire_video_dot(R01sBoard *ctx) {
    if (r01s_fast_glue_enabled(R01S_FAST_GLUE_VIDEO)) {
        wire_video_dot_fast(ctx);
        return;
    }

    R01sBeamXy *beam = ctx->beam_impl.beam_x;
    R01sCompositor *comp = ctx->video_impl.comp;
    R01sAt28c16 *prom = ctx->video_impl.prom;
    R01sVideoSink *sink = ctx->video_impl.sink;
    R01sEntity *comp_e = r01s_compositor_entity(comp);
    R01sEntity *prom_e = r01s_at28c16_entity(prom);
    int bx = r01s_beam_xy_x(beam);
    int by = r01s_beam_xy_y(beam);
    int lx;
    int ly;
    uint8_t bg;
    uint8_t idx;
    uint8_t packed;

    if (r01s_beam_xy_hblank(beam) || r01s_beam_xy_vblank(beam) || bx >= R01S_BEAM_VISIBLE_W ||
        by >= R01S_BEAM_VISIBLE_H) {
        return;
    }
    /* Blank until nametable+attrs finished streaming (real games would VBlank-load). */
    if (board_video_held_for_map_stream(ctx)) {
        return;
    }
    lx = bx / 2;
    ly = by / 2;
    bg = board_bg_master_at(ctx, lx, ly);
    r01s_compositor_set_bg(comp, bg);
    {
        uint16_t spr_addr = (uint16_t)(((ctx->linebuf_show_half & 1u) << 7) | (lx & 0x7F));
        uint8_t spr = r01s_as6c62256_peek(ctx->linebuf_impl.sram, spr_addr);
        r01s_compositor_set_sprite(comp, (uint8_t)(spr & 0x3Fu), spr != 0);
        if (spr != 0) {
            ctx->health_saw_sprites = 1;
        }
    }
    r01s_entity_eval(comp_e);

    idx = r01s_compositor_out(comp);
    wire_video_prom_addr(prom_e, idx);
    packed = r01s_at28c16_peek(prom, idx);
    r01s_video_sink_plot(sink, lx, ly, packed);
}

/*
 * Playbook Target 3 — inline CPU RAM/PRG decode (no per-pin RAM/flash entity eval).
 */
static void wire_memory_fast(R01sBoard *ctx) {
    R01sEntity *cpu_e = r01s_w65c02s_entity(&ctx->cpu);
    uint16_t addr = r01s_w65c02s_ab(&ctx->cpu);
    int read = r01s_w65c02s_rwb(&ctx->cpu) != 0;
    int use_cart_prg = ctx->cart_loaded && ctx->cart_len_prg > 0;

    if (!board_cpu_be(ctx, cpu_e) || addr_is_io(addr)) {
        return;
    }

    if (!(addr & 0x8000u)) {
        uint16_t ram_addr = (uint16_t)(addr & 0x7FFFu);
        if (read) {
            r01s_bus_write(cpu_e, "D", 8, r01s_as6c62256_peek(&ctx->ram, ram_addr));
        } else {
            r01s_as6c62256_poke(&ctx->ram, ram_addr, r01s_w65c02s_a(&ctx->cpu));
        }
        return;
    }

    if (use_cart_prg && read) {
        uint32_t off = (uint32_t)(addr - 0x8000u);
        if (off < ctx->cart_len_prg) {
            r01s_bus_write(cpu_e, "D", 8,
                           r01s_sst39sf040_peek(&ctx->cart_flash, ctx->cart_off_prg + off));
        }
        return;
    }

    if (read) {
        r01s_bus_write(cpu_e, "D", 8, r01s_prg_rom_peek(&ctx->prg, (uint16_t)(addr - 0x8000u)));
    }
}

static void wire_memory(R01sBoard *ctx) {
    if (r01s_fast_glue_enabled(R01S_FAST_GLUE_MEMORY)) {
        wire_memory_fast(ctx);
        return;
    }

    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *ram = r01s_as6c62256_entity(ctx->cpu_mem_impl.ram);
    R01sEntity *prg = r01s_prg_rom_entity(ctx->cpu_mem_impl.prg);
    R01sEntity *flash = r01s_sst39sf040_entity(ctx->cart_impl.flash);
    uint16_t addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int a15 = (addr & 0x8000u) != 0;
    int io = addr_is_io(addr);
    int use_cart_prg = ctx->cart_loaded && ctx->cart_len_prg > 0;

    r01s_entity_drive(ram, "CE#", R01S_LVL_H);
    r01s_entity_drive(ram, "OE#", R01S_LVL_H);
    r01s_entity_drive(ram, "WE#", R01S_LVL_H);
    r01s_entity_drive(prg, "CE#", R01S_LVL_H);
    r01s_entity_drive(prg, "OE#", R01S_LVL_H);
    r01s_bus_hiz(ram, "DQ", 8);
    r01s_bus_hiz(prg, "DQ", 8);
    /* PRG vs MAP: only one flash /CE. MAP owns flash inside wire_io on $FE93. */
    if (!(io && addr == 0xFE93u)) {
        flash_deselect(flash);
    }

    if (!r01s_level_is_high(r01s_entity_sense(cpu, "BE")) || io) {
        r01s_entity_eval(ram);
        r01s_entity_eval(prg);
        return;
    }

    if (!a15) {
        copy_bus_named(ctx, ram, "A", cpu, "A", 15);
        r01s_entity_drive(ram, "CE#", R01S_LVL_L);
        if (read) {
            r01s_entity_drive(ram, "OE#", R01S_LVL_L);
            r01s_entity_drive(ram, "WE#", R01S_LVL_H);
            r01s_entity_eval(ram);
            copy_bus_named(ctx, cpu, "D", ram, "DQ", 8);
        } else {
            r01s_entity_drive(ram, "OE#", R01S_LVL_H);
            r01s_entity_drive(ram, "WE#", R01S_LVL_L);
            copy_bus_named(ctx, ram, "DQ", cpu, "D", 8);
            r01s_entity_eval(ram);
        }
        r01s_entity_eval(prg);
    } else if (use_cart_prg && read) {
        uint32_t off = (uint32_t)(addr - 0x8000u);
        if (off < ctx->cart_len_prg) {
            flash_read_selected(flash, ctx->cart_off_prg + off);
            copy_bus_named(ctx, cpu, "D", flash, "DQ", 8);
        }
        r01s_entity_eval(prg);
        r01s_entity_eval(ram);
    } else {
        /* Fallback breadboard PRG_ROM (writes ignored). */
        copy_bus_named(ctx, prg, "A", cpu, "A", 15);
        r01s_entity_drive(prg, "CE#", R01S_LVL_L);
        r01s_entity_drive(prg, "OE#", R01S_LVL_L);
        r01s_entity_eval(prg);
        if (read) {
            copy_bus_named(ctx, cpu, "D", prg, "DQ", 8);
        }
        r01s_entity_eval(ram);
    }
}

static void wire_power_clock_reset(R01sBoard *ctx, R01sIslandGroup *group) {
    R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_impl.pwr);
    R01sEntity *osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    R01sEntity *hc = r01s_sn74hc14_entity(ctx->clock_impl.hc14);
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sLevel vdd, phi2, resb;

    r01s_entity_drive(pwr, "VIN", group->powered ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(pwr, "EN", R01S_LVL_H);
    r01s_entity_eval(pwr);
    vdd = r01s_entity_sense(pwr, "VDD");

    r01s_entity_drive(osc, "VDD", vdd);
    r01s_entity_drive(osc, "OE#", R01S_LVL_H);

    resb = (ctx->reset_hold > 0) ? R01S_LVL_L : R01S_LVL_H;
    r01s_entity_drive(cpu, "RESB", resb);
    r01s_entity_drive(cpu, "BE", R01S_LVL_H);
    r01s_entity_drive(cpu, "RDY", R01S_LVL_H);
    /* IRQB from beam-Y EQ# in wire_beam; NMIB from beam NMI# in board_step */

    phi2 = r01s_entity_sense(osc, "PHI2");
    r01s_entity_drive(hc, "1A", phi2 == R01S_LVL_Z ? R01S_LVL_L : phi2);
    r01s_entity_drive(hc, "2A", resb);
    /* Unused Schmitt gates: tie inputs (real boards do this; avoids X on 3Y–6Y). */
    r01s_entity_drive(hc, "3A", R01S_LVL_H);
    r01s_entity_drive(hc, "4A", R01S_LVL_H);
    r01s_entity_drive(hc, "5A", R01S_LVL_H);
    r01s_entity_drive(hc, "6A", R01S_LVL_H);
    r01s_entity_eval(hc);

    r01s_entity_drive(cpu, "PHI2", phi2 == R01S_LVL_H ? R01S_LVL_H : R01S_LVL_L);
}

static void board_settle(R01sBoard *ctx, R01sIslandGroup *group) {
    int passes = r01s_fast_glue_enabled(R01S_FAST_GLUE_SETTLE) ? R01S_SETTLE_PASSES_FAST : R01S_SETTLE_PASSES;
    int i;
    for (i = 0; i < passes; i++) {
        wire_power_clock_reset(ctx, group);
        wire_memory(ctx);
        wire_io(ctx);
        wire_beam(ctx, group);
        wire_bg_fetch(ctx);
        wire_vram(ctx);
        wire_linebuf(ctx);
    }
}

static void island_power_init(R01sIsland *island) {
    R01sIslandPowerImpl *impl = (R01sIslandPowerImpl *)island->impl;
    r01s_pwr5v_init(impl->pwr, "PS1");
    r01s_island_add_entity(island, r01s_pwr5v_entity(impl->pwr));
}

static void island_clock_init(R01sIsland *island) {
    R01sIslandClockImpl *impl = (R01sIslandClockImpl *)island->impl;
    r01s_osc8m_init(impl->osc, "Y1");
    r01s_sn74hc14_init(impl->hc14, "U2");
    r01s_island_add_entity(island, r01s_osc8m_entity(impl->osc));
    r01s_island_add_entity(island, r01s_sn74hc14_entity(impl->hc14));
}

static void island_cpu_mem_init(R01sIsland *island) {
    R01sIslandCpuMemImpl *impl = (R01sIslandCpuMemImpl *)island->impl;
    /* Island C: CPU + RAM + breadboard PRG stub (deselected when cart owns $8000+). */
    r01s_w65c02s_init(impl->cpu, "U1");
    r01s_as6c62256_init(impl->ram, "U3");
    r01s_prg_rom_init(impl->prg, "U4");
    {
        uint8_t boot[sizeof(R01S_BRINGUP_SMOKE) + sizeof(R01S_BRINGUP_HANG)];
        uint16_t hang_pc;
        memcpy(boot, R01S_BRINGUP_SMOKE, sizeof(R01S_BRINGUP_SMOKE));
        memcpy(boot + sizeof(R01S_BRINGUP_SMOKE), R01S_BRINGUP_HANG, sizeof(R01S_BRINGUP_HANG));
        hang_pc = (uint16_t)(0x8000u + sizeof(R01S_BRINGUP_SMOKE));
        boot[sizeof(R01S_BRINGUP_SMOKE) + 4] = (uint8_t)(hang_pc & 0xFFu);
        boot[sizeof(R01S_BRINGUP_SMOKE) + 5] = (uint8_t)(hang_pc >> 8);
        r01s_prg_rom_load(impl->prg, 0x0000, boot, (uint16_t)sizeof(boot));
    }
    r01s_prg_rom_set_reset_vec(impl->prg, 0x8000);
    r01s_island_add_entity(island, r01s_w65c02s_entity(impl->cpu));
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->ram));
    r01s_atf22v10_init(impl->pld_decode, "UPLDA", R01S_PLD_DECODE);
    r01s_island_add_entity(island, r01s_atf22v10_entity(impl->pld_decode));
}

static void island_io_latch_init(R01sIsland *island) {
    R01sIslandIoLatchImpl *impl = (R01sIslandIoLatchImpl *)island->impl;
    static const char *const refdes[R01S_BOM_HC573_N] = {
        "U5A", "U5B", "U5C", "U5D", "U5E", "U5F", "U5G", "U5H", "U5I",
    };
    int i;
    for (i = 0; i < R01S_BOM_HC573_N; i++) {
        r01s_sn74hc573_init(impl->latch573[i], refdes[i]);
        r01s_island_add_entity(island, r01s_sn74hc573_entity(impl->latch573[i]));
    }
}

static void island_beam_init(R01sIsland *island) {
    R01sIslandBeamImpl *impl = (R01sIslandBeamImpl *)island->impl;
    r01s_osc_dot_init(impl->osc_dot, "Y2");
    r01s_beam_xy_init(impl->beam_x, "UPLDX");
    r01s_atf22v10_init(impl->beam_y, "UPLDY", R01S_PLD_BEAM_Y);
    r01s_island_add_entity(island, r01s_osc_dot_entity(impl->osc_dot));
    r01s_island_add_entity(island, r01s_beam_xy_entity(impl->beam_x));
    r01s_island_add_entity(island, r01s_atf22v10_entity(impl->beam_y));
}

static void island_pads_init(R01sIsland *island) {
    R01sIslandPadsImpl *impl = (R01sIslandPadsImpl *)island->impl;
    r01s_pads_init(impl->pads, "PAD");
    /* Wired via 1284 on silicon — sim model only; not drawn on the board canvas. */
}

static void island_vram_init(R01sIsland *island) {
    R01sIslandVramImpl *impl = (R01sIslandVramImpl *)island->impl;
    static const char *const mux_ref[R01S_BOM_HC157_N] = {"U7A", "U7B", "U7C", "U7D", "U7E", "U7F"};
    int i;
    r01s_as6c62256_init(impl->vram, "U6");
    r01s_atf22v10_init(impl->pld_vram, "UPLDB", R01S_PLD_VRAM);
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->vram));
    for (i = 0; i < 3; i++) {
        r01s_sn74hc157_init(impl->mux157[i], mux_ref[i]);
        r01s_island_add_entity(island, r01s_sn74hc157_entity(impl->mux157[i]));
    }
    r01s_island_add_entity(island, r01s_atf22v10_entity(impl->pld_vram));
}

static void island_bg_fetch_init(R01sIsland *island) {
    R01sIslandBgFetchImpl *impl = (R01sIslandBgFetchImpl *)island->impl;
    r01s_bg_fetch_init(impl->fetch, "UPLDI");
    r01s_island_add_entity(island, r01s_bg_fetch_entity(impl->fetch));
}

static void island_video_init(R01sIsland *island) {
    R01sIslandVideoImpl *impl = (R01sIslandVideoImpl *)island->impl;
    r01s_compositor_init(impl->comp, "UPLDV");
    r01s_at28c16_init(impl->prom, "U24");
    r01s_video_sink_init(impl->sink, "LCD1");
    r01s_island_add_entity(island, r01s_compositor_entity(impl->comp));
    r01s_island_add_entity(island, r01s_at28c16_entity(impl->prom));
    r01s_island_add_entity(island, r01s_video_sink_entity(impl->sink));
}

static void island_cart_init(R01sIsland *island) {
    R01sIslandCartImpl *impl = (R01sIslandCartImpl *)island->impl;
    r01s_sst39sf040_init(impl->flash, "U40");
    r01s_i2c_eeprom_init(impl->save_eeprom, "U50");
    r01s_island_add_entity(island, r01s_sst39sf040_entity(impl->flash));
    r01s_island_add_entity(island, r01s_i2c_eeprom_entity(impl->save_eeprom));
}

static void island_apu_init(R01sIsland *island) {
    R01sIslandApuImpl *impl = (R01sIslandApuImpl *)island->impl;
    r01s_atmega328p_init(impl->apu, "U328");
    r01s_island_add_entity(island, r01s_atmega328p_entity(impl->apu));
}

static void island_mcu1284_init(R01sIsland *island) {
    R01sIslandMcu1284Impl *impl = (R01sIslandMcu1284Impl *)island->impl;
    r01s_atmega1284p_init(impl->mcu, "U1284");
    r01s_island_add_entity(island, r01s_atmega1284p_entity(impl->mcu));
}

static void island_linebuf_init(R01sIsland *island) {
    R01sIslandLinebufImpl *impl = (R01sIslandLinebufImpl *)island->impl;
    static const char *const mux_ref[R01S_BOM_HC157_N] = {"U7A", "U7B", "U7C", "U7D", "U7E", "U7F"};
    int i;
    r01s_as6c62256_init(impl->sram, "U41");
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->sram));
    for (i = 3; i < R01S_BOM_HC157_N; i++) {
        r01s_sn74hc157_init(impl->mux157[i], mux_ref[i]);
        r01s_island_add_entity(island, r01s_sn74hc157_entity(impl->mux157[i]));
    }
}

static void island_bus_init(R01sIsland *island) {
    R01sIslandBusImpl *impl = (R01sIslandBusImpl *)island->impl;
    static const char *const ref[R01S_BOM_HC245_N] = {"U20A", "U20B", "U20C"};
    int i;
    for (i = 0; i < R01S_BOM_HC245_N; i++) {
        r01s_sn74hc245_init(impl->bus245[i], ref[i]);
        r01s_island_add_entity(island, r01s_sn74hc245_entity(impl->bus245[i]));
    }
}

static void island_sprites_init(R01sIsland *island) {
    R01sIslandSpritesImpl *impl = (R01sIslandSpritesImpl *)island->impl;
    r01s_sprite_fetch_init(impl->fetch, "UPLDN");
    r01s_island_add_entity(island, r01s_sprite_fetch_entity(impl->fetch));
}

static void island_integration_init(R01sIsland *island) {
    R01sIslandIntegrationImpl *impl = (R01sIslandIntegrationImpl *)island->impl;
    r01s_integration_init(impl->integ, "UPLDP");
    r01s_island_add_entity(island, r01s_integration_entity(impl->integ));
}


static const R01sIslandVTable ISLAND_POWER_VT = {island_power_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CLOCK_VT = {island_clock_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CPU_VT = {island_cpu_mem_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_IO_VT = {island_io_latch_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_PADS_VT = {island_pads_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_VRAM_VT = {island_vram_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_BEAM_VT = {island_beam_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_BG_FETCH_VT = {island_bg_fetch_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_VIDEO_VT = {island_video_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CART_VT = {island_cart_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_APU_VT = {island_apu_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_MCU1284_VT = {island_mcu1284_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_LINEBUF_VT = {island_linebuf_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_SPRITES_VT = {island_sprites_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_INTEGRATION_VT = {island_integration_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_BUS_VT = {island_bus_init, NULL, NULL, NULL, NULL};


static void board_resolve_cart_meta(R01sBoard *board) {
    const uint8_t *img;
    const uint8_t *ptrs;
    const uint8_t *slot;
    const uint8_t *hdr;
    const uint8_t *dir;
    uint32_t off_wtable;
    uint32_t world_base;
    uint32_t off_chr;
    uint32_t off_sdir;
    uint8_t start_col;
    uint8_t start_row;
    uint8_t screen_count;
    int si;

    board->cart_off_chr = 0;
    board->cart_off_map_screen0 = 0;
    board->cart_off_pal_bg = 0;
    board->cart_off_pal_spr = 0;
    if (!board || !board->cart_loaded) {
        return;
    }
    img = board->cart_flash.mem;
    if (memcmp(img, "RETR01", 6) != 0) {
        return;
    }
    ptrs = img + R01S_CART_HDR_SIZE;
    board->cart_off_pal_bg = get_u24(ptrs + 6);
    board->cart_off_pal_spr = get_u24(ptrs + 12);
    off_wtable = get_u24(ptrs + 18);
    if ((size_t)off_wtable + 8u > sizeof(board->cart_flash.mem)) {
        return;
    }
    slot = img + off_wtable;
    if (slot[0] == 0) {
        return;
    }
    world_base = get_u24(slot + 2);
    if ((size_t)world_base + 32u > sizeof(board->cart_flash.mem)) {
        return;
    }
    hdr = img + world_base;
    start_col = hdr[0];
    start_row = hdr[1];
    screen_count = hdr[5];
    off_chr = get_u24(hdr + 8);
    off_sdir = get_u24(hdr + 11);
    board->cart_off_chr = world_base + off_chr;
    if ((size_t)world_base + (size_t)off_sdir + (size_t)screen_count * 12u > sizeof(board->cart_flash.mem)) {
        board->cart_off_chr = 0;
        return;
    }
    dir = img + world_base + off_sdir;
    for (si = 0; si < (int)screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        uint32_t poff;
        if (e[0] != start_col || e[1] != start_row) {
            continue;
        }
        poff = get_u24(e + 4);
        board->cart_off_map_screen0 = world_base + poff;
        return;
    }
}

static void board_install_bringup_prg(R01sBoard *board) {
    uint32_t base;
    uint32_t i;
    uint8_t buf[512];
    size_t n = 0;
    uint16_t hang_pc;
    int stream;

    if (!board || !board->cart_loaded) {
        return;
    }
    board_resolve_cart_meta(board);
    stream = (board->cart_off_map_screen0 != 0 && board->cart_off_pal_bg != 0);

    memcpy(buf + n, R01S_BRINGUP_SMOKE, sizeof(R01S_BRINGUP_SMOKE));
    n += sizeof(R01S_BRINGUP_SMOKE);
    if (stream) {
        memcpy(buf + n, R01S_BRINGUP_STREAM, sizeof(R01S_BRINGUP_STREAM));
        buf[n + R01S_BR_OFF_PAL_LO] = (uint8_t)(board->cart_off_pal_bg & 0xFFu);
        buf[n + R01S_BR_OFF_PAL_MID] = (uint8_t)((board->cart_off_pal_bg >> 8) & 0xFFu);
        buf[n + R01S_BR_OFF_PAL_HI] = (uint8_t)((board->cart_off_pal_bg >> 16) & 0xFFu);
        buf[n + R01S_BR_OFF_MAP_LO] = (uint8_t)(board->cart_off_map_screen0 & 0xFFu);
        buf[n + R01S_BR_OFF_MAP_MID] = (uint8_t)((board->cart_off_map_screen0 >> 8) & 0xFFu);
        buf[n + R01S_BR_OFF_MAP_HI] = (uint8_t)((board->cart_off_map_screen0 >> 16) & 0xFFu);
        n += sizeof(R01S_BRINGUP_STREAM);
    }
    hang_pc = (uint16_t)(0x8000u + n);
    memcpy(buf + n, R01S_BRINGUP_HANG, sizeof(R01S_BRINGUP_HANG));
    buf[n + 4] = (uint8_t)(hang_pc & 0xFFu);
    buf[n + 5] = (uint8_t)(hang_pc >> 8);
    n += sizeof(R01S_BRINGUP_HANG);

    base = board->cart_off_prg;
    for (i = 0; i < R01S_CART_PRG_BYTES; i++) {
        r01s_sst39sf040_poke(&board->cart_flash, base + i, 0xEA);
    }
    r01s_sst39sf040_load(&board->cart_flash, base, buf, (uint32_t)n);
    /* Reset vector at CPU $FFFC/$FFFD => PRG offset $7FFC/$7FFD */
    r01s_sst39sf040_poke(&board->cart_flash, base + 0x7FFCu, 0x00);
    r01s_sst39sf040_poke(&board->cart_flash, base + 0x7FFDu, 0x80);
}

static void board_install_synthetic_cart(R01sBoard *board) {
    uint8_t hdr[R01S_CART_HDR_SIZE];
    uint8_t ptrs[R01S_CART_PTR_SIZE];
    uint32_t off_pal_bg = R01S_CART_HDR_SIZE + R01S_CART_PTR_SIZE; /* 0x28 */
    uint32_t off_pal_spr = off_pal_bg + 16;                         /* 0x38 */
    uint32_t off_prg = off_pal_spr + 16;                             /* 0x48 */
    uint32_t off_wtable = off_prg + R01S_CART_PRG_BYTES;
    uint8_t pals[32];

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "RETR01", 6);
    hdr[6] = 1;
    hdr[7] = 1;
    memset(ptrs, 0, sizeof(ptrs));
    put_u24(ptrs + 0, off_prg);
    put_u24(ptrs + 3, R01S_CART_PRG_BYTES);
    put_u24(ptrs + 6, off_pal_bg);
    put_u24(ptrs + 9, 16);
    put_u24(ptrs + 12, off_pal_spr);
    put_u24(ptrs + 15, 16);
    put_u24(ptrs + 18, off_wtable);
    put_u24(ptrs + 21, 64);
    memset(pals, 0, sizeof(pals));

    memset(board->cart_flash.mem, 0xFF, sizeof(board->cart_flash.mem));
    r01s_sst39sf040_load(&board->cart_flash, 0, hdr, R01S_CART_HDR_SIZE);
    r01s_sst39sf040_load(&board->cart_flash, R01S_CART_HDR_SIZE, ptrs, R01S_CART_PTR_SIZE);
    r01s_sst39sf040_load(&board->cart_flash, off_pal_bg, pals, 32);
    board->cart_off_prg = off_prg;
    board->cart_len_prg = R01S_CART_PRG_BYTES;
    board->cart_loaded = 1;
    snprintf(board->cart_label, sizeof(board->cart_label), "synthetic bring-up");
    board_install_bringup_prg(board);
}

static int board_parse_cart_image(R01sBoard *board, const uint8_t *img, size_t len) {
    const uint8_t *ptrs;
    uint32_t off_prg;
    uint32_t len_prg;
    if (!board || !img || len < R01S_CART_HDR_SIZE + R01S_CART_PTR_SIZE) {
        return -1;
    }
    if (memcmp(img, "RETR01", 6) != 0) {
        /* Raw flash dump: treat whole image as mapped from 0; PRG at 0x48 convention. */
        if (len > R01S_FLASH_BYTES) {
            len = R01S_FLASH_BYTES;
        }
        memset(board->cart_flash.mem, 0xFF, sizeof(board->cart_flash.mem));
        r01s_sst39sf040_load(&board->cart_flash, 0, img, (uint32_t)len);
        board->cart_off_prg = 0x48;
        board->cart_len_prg = R01S_CART_PRG_BYTES;
        board->cart_loaded = 1;
        return 0;
    }
    ptrs = img + R01S_CART_HDR_SIZE;
    off_prg = get_u24(ptrs + 0);
    len_prg = get_u24(ptrs + 3);
    if (len_prg == 0 || len_prg > R01S_CART_PRG_BYTES) {
        len_prg = R01S_CART_PRG_BYTES;
    }
    if ((size_t)off_prg + len_prg > len && (size_t)off_prg >= len) {
        return -1;
    }
    memset(board->cart_flash.mem, 0xFF, sizeof(board->cart_flash.mem));
    if (len > R01S_FLASH_BYTES) {
        len = R01S_FLASH_BYTES;
    }
    r01s_sst39sf040_load(&board->cart_flash, 0, img, (uint32_t)len);
    board->cart_off_prg = off_prg;
    board->cart_len_prg = len_prg;
    board->cart_loaded = 1;
    return 0;
}

int r01s_board_load_cart(R01sBoard *board, const char *path) {
    FILE *f;
    long sz;
    uint8_t *buf;
    size_t n;
    int rc;
    if (!board || !path) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return -1;
    }
    n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return -1;
    }
    rc = board_parse_cart_image(board, buf, n);
    free(buf);
    if (rc != 0) {
        return -1;
    }
    {
        const char *slash = strrchr(path, '/');
        snprintf(board->cart_label, sizeof(board->cart_label), "%s", slash ? slash + 1 : path);
    }
    /* Bring-up overlay: keep island smoke working on a real Studio cart. */
    board_install_bringup_prg(board);
    return 0;
}

static void board_shutdown(R01sIslandGroup *group) {
    int i;
    if (!group) {
        return;
    }
    for (i = group->island_count - 1; i >= 0; i--) {
        r01s_island_shutdown(group->islands[i]);
    }
    group->island_count = 0;
}

static void board_reset(R01sIslandGroup *group) {
    R01sBoard *ctx = board_from_group(group);
    if (!ctx) {
        return;
    }
    ctx->reset_hold = R01S_RESET_HOLD;
    ctx->cycles = 0;
    ctx->phi2_prev = R01S_LVL_L;
    ctx->vram_addr = 0;
    ctx->vram_fe12_armed = 0;
    ctx->map_addr = 0;
    ctx->map_fe93_armed = 0;
    ctx->pal_addr = 0;
    ctx->pal_fe09_wrote = 0;
    memset(ctx->active_pal, 0, sizeof(ctx->active_pal));
    ctx->chr_last_master = 0;
    ctx->health_saw_latch = 0;
    ctx->health_saw_vram = 0;
    ctx->health_saw_vram_read = 0;
    ctx->health_saw_pad = 0;
    ctx->health_saw_beam = 0;
    ctx->health_saw_bg_fetch = 0;
    ctx->health_saw_video = 0;
    ctx->health_saw_map = 0;
    ctx->health_saw_apu = 0;
    ctx->health_saw_oam = 0;
    ctx->health_saw_linebuf = 0;
    ctx->health_saw_sprites = 0;
    ctx->health_saw_nmi = 0;
    ctx->nmi_prev = R01S_LVL_H;
    ctx->nmi_pulses = 0;
    ctx->linebuf_show_half = 0;

    ctx->linebuf_prev_hblank = 0;
    ctx->linebuf_saw_mux_mcu = 0;
    ctx->linebuf_saw_mux_beam = 0;
    ctx->health_phi2_edges = 0;
    r01s_bus_clear_conflicts();
    r01s_entity_reset(r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu));
    r01s_entity_reset(r01s_osc8m_entity(ctx->clock_impl.osc));
    {
        int li;
        for (li = 0; li < R01S_BOM_HC573_N; li++) {
            r01s_entity_reset(r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[li]));
        }
    }
    r01s_entity_reset(r01s_pads_entity(ctx->pads_impl.pads));
    r01s_entity_reset(r01s_as6c62256_entity(ctx->vram_impl.vram));
    {
        int mi;
        for (mi = 0; mi < R01S_BOM_HC157_N; mi++) {
            r01s_entity_reset(r01s_sn74hc157_entity(ctx->vram_impl.mux157[mi]));
        }
    }
    r01s_entity_reset(r01s_atf22v10_entity(ctx->cpu_mem_impl.pld_decode));
    r01s_entity_reset(r01s_atf22v10_entity(ctx->vram_impl.pld_vram));
    r01s_entity_reset(r01s_osc_dot_entity(ctx->beam_impl.osc_dot));
    r01s_entity_reset(r01s_beam_xy_entity(ctx->beam_impl.beam_x));
    r01s_entity_reset(r01s_atf22v10_entity(ctx->beam_impl.beam_y));
    {
        int bi;
        for (bi = 0; bi < R01S_BOM_HC245_N; bi++) {
            r01s_entity_reset(r01s_sn74hc245_entity(ctx->bus_impl.bus245[bi]));
        }
    }
    r01s_entity_reset(r01s_bg_fetch_entity(ctx->bg_fetch_impl.fetch));
    r01s_entity_reset(r01s_compositor_entity(ctx->video_impl.comp));
    r01s_entity_reset(r01s_at28c16_entity(ctx->video_impl.prom));
    r01s_entity_reset(r01s_video_sink_entity(ctx->video_impl.sink));
    r01s_entity_reset(r01s_sst39sf040_entity(ctx->cart_impl.flash));
    r01s_entity_reset(r01s_atmega328p_entity(ctx->apu_impl.apu));
    r01s_entity_reset(r01s_atmega1284p_entity(ctx->mcu1284_impl.mcu));
    r01s_entity_reset(r01s_as6c62256_entity(ctx->linebuf_impl.sram));
    r01s_entity_reset(r01s_sn74hc157_entity(ctx->linebuf_impl.mux157[R01S_MUX157_LINEBUF0]));
    board_settle(ctx, group);
    r01s_entity_eval(r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu));
    board_settle(ctx, group);
}

static void board_eval_idle(R01sIslandGroup *group) {
    R01sBoard *ctx = board_from_group(group);
    if (!ctx) {
        return;
    }
    board_settle(ctx, group);
}

static void board_step(R01sIslandGroup *group) {
    R01sBoard *ctx = board_from_group(group);
    R01sEntity *osc;
    R01sEntity *cpu;
    R01sLevel phi2;
    if (!ctx || !group->powered) {
        return;
    }
    osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);

    board_settle(ctx, group);
    r01s_entity_tick(osc);
    r01s_entity_tick(r01s_atmega328p_entity(ctx->apu_impl.apu));
    r01s_entity_tick(r01s_atmega1284p_entity(ctx->mcu1284_impl.mcu));
    board_settle(ctx, group);
    /* Beam/DOT domain: burst so interactive UI reaches VBlank without minutes of wait. */
    {
        R01sEntity *dot_osc = r01s_osc_dot_entity(ctx->beam_impl.osc_dot);
        R01sEntity *beam = r01s_beam_xy_entity(ctx->beam_impl.beam_x);
        R01sEntity *cpu_e = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
        int di;
        for (di = 0; di < R01S_BEAM_DOTS_PER_STEP; di++) {
            r01s_entity_tick(dot_osc);
            r01s_entity_drive(beam, "DOT", r01s_entity_sense(dot_osc, "DOT"));
            r01s_entity_tick(beam);
            {
                R01sLevel nmi = r01s_entity_sense(beam, "NMI#");
                r01s_entity_drive(cpu_e, "NMIB", nmi);
                if (nmi == R01S_LVL_L && ctx->nmi_prev != R01S_LVL_L) {
                    ctx->nmi_pulses++;
                    ctx->health_saw_nmi = 1;
                    r01s_integration_note_nmi(ctx->integration_impl.integ);
                }
                ctx->nmi_prev = nmi;
            }
            wire_video_dot(ctx);
        }
    }
    board_settle(ctx, group);

    phi2 = r01s_entity_sense(osc, "PHI2");
    if (phi2 != ctx->phi2_prev && (phi2 == R01S_LVL_H || phi2 == R01S_LVL_L)) {
        ctx->health_phi2_edges++;
    }
    if (phi2 == R01S_LVL_H && ctx->phi2_prev != R01S_LVL_H) {
        if (ctx->reset_hold > 0) {
            ctx->reset_hold--;
            board_settle(ctx, group);
            r01s_entity_eval(cpu);
            board_settle(ctx, group);
        } else {
            R01sCpuPhase ph_before;
            board_settle(ctx, group);
            ph_before = r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu);
            r01s_entity_tick(cpu);
            ctx->cycles++;
            r01s_entity_eval(cpu);
            board_settle(ctx, group);
            /* Auto-inc after a completed DATA cycle that touched $FE12 / $FE93. */
            if (ph_before == R01S_CPU_OP_DATA) {
                if (ctx->vram_fe12_armed) {
                    ctx->vram_addr = (uint16_t)((ctx->vram_addr + 1u) & 0x7FFFu);
                    ctx->vram_fe12_armed = 0;
                }
                if (ctx->map_fe93_armed) {
                    ctx->map_addr = (ctx->map_addr + 1u) & 0xFFFFFFu;
                    ctx->map_fe93_armed = 0;
                }
                ctx->pal_fe09_wrote = 0;
            }
        }
    }
    ctx->phi2_prev = phi2;
    board_update_milestones(ctx);
}

static void board_status(R01sIslandGroup *group, char *buf, size_t buf_len) {
    R01sBoard *ctx = board_from_group(group);
    R01sEntity *cpu;
    R01sEntity *pwr;
    R01sEntity *osc;
    if (!ctx || !buf || buf_len == 0) {
        return;
    }
    cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    pwr = r01s_pwr5v_entity(ctx->power_impl.pwr);
    osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    snprintf(buf, buf_len,
             "%s  VDD=%c PHI2=%c RESB=%c  PC=%04X A=%02X AB=%04X IR=%02X %s  LE=%02X "
             "VA=%04X V0=%02X P1=%02X P2=%02X  XY=%d,%d%s%s BG=%02X/%02X cyc=%u",
             group->running ? "RUN" : "PAUSE",
             r01s_level_is_high(r01s_entity_sense(pwr, "VDD")) ? 'H' : 'L',
             r01s_level_is_high(r01s_entity_sense(osc, "PHI2")) ? 'H' : 'L',
             r01s_level_is_low(r01s_entity_sense(cpu, "RESB")) ? 'L' : 'H',
             r01s_w65c02s_pc(ctx->cpu_mem_impl.cpu), r01s_w65c02s_a(ctx->cpu_mem_impl.cpu),
             (unsigned)r01s_bus_read(cpu, "A", 16), ctx->cpu.ir,
             phase_name(r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu)),
             r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]), (unsigned)(ctx->vram_addr & 0x7FFFu),
             r01s_as6c62256_peek(ctx->vram_impl.vram, 0), r01s_pads_get(ctx->pads_impl.pads, 0),
             r01s_pads_get(ctx->pads_impl.pads, 1), r01s_beam_xy_x(ctx->beam_impl.beam_x),
             r01s_beam_xy_y(ctx->beam_impl.beam_x), r01s_beam_xy_hblank(ctx->beam_impl.beam_x) ? " HB" : "",
             r01s_beam_xy_vblank(ctx->beam_impl.beam_x) ? " VB" : "",
             r01s_bg_fetch_last_tile(ctx->bg_fetch_impl.fetch),
             r01s_bg_fetch_last_attr(ctx->bg_fetch_impl.fetch), (unsigned)ctx->cycles);
}

static void board_update_probes(R01sIslandGroup *group, int *probe_vdd, int *probe_phi2, int *probe_resb_low) {
    R01sBoard *ctx = board_from_group(group);
    if (!ctx) {
        return;
    }
    if (probe_vdd) {
        *probe_vdd = r01s_level_is_high(r01s_entity_sense(r01s_pwr5v_entity(ctx->power_impl.pwr), "VDD"));
    }
    if (probe_phi2) {
        *probe_phi2 = r01s_level_is_high(r01s_entity_sense(r01s_osc8m_entity(ctx->clock_impl.osc), "PHI2"));
    }
    if (probe_resb_low) {
        *probe_resb_low =
            r01s_level_is_low(r01s_entity_sense(r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu), "RESB"));
    }
}

static const R01sIslandGroupVTable BOARD_GROUP_VT = {
    board_shutdown,
    board_reset,
    NULL,
    board_step,
    board_eval_idle,
    board_status,
    board_update_probes,
    board_fill_health,
};

int r01s_board_build(R01sBoard *board, R01sIslandBuilder *b) {
    if (!board || !b) {
        return -1;
    }
    memset(board, 0, sizeof(*board));

    r01s_island_builder_bind(b, &BOARD_GROUP_VT, board);

    board->power_impl.pwr = &board->pwr;
    board->clock_impl.osc = &board->osc;
    board->clock_impl.hc14 = &board->hc14;
    board->cpu_mem_impl.cpu = &board->cpu;
    board->cpu_mem_impl.ram = &board->ram;
    board->cpu_mem_impl.prg = &board->prg;
    board->cpu_mem_impl.pld_decode = &board->pld_decode;
    {
        int i;
        for (i = 0; i < R01S_BOM_HC573_N; i++) {
            board->io_latch_impl.latch573[i] = &board->latch573[i];
        }
    }
    board->pads_impl.pads = &board->pads;
    board->vram_impl.vram = &board->vram;
    {
        int i;
        for (i = 0; i < R01S_BOM_HC157_N; i++) {
            board->vram_impl.mux157[i] = &board->mux157[i];
        }
    }
    board->vram_impl.bg_pld = &board->bg_fetch;
    board->vram_impl.pld_vram = &board->pld_vram;
    board->beam_impl.osc_dot = &board->osc_dot;
    board->beam_impl.beam_x = &board->pld_beam_x;
    board->beam_impl.beam_y = &board->pld_beam_y;
    board->bg_fetch_impl.fetch = &board->bg_fetch;
    board->video_impl.comp = &board->compositor;
    board->video_impl.prom = &board->color_prom;
    board->video_impl.sink = &board->video_sink;
    board->cart_impl.flash = &board->cart_flash;
    board->cart_impl.save_eeprom = &board->cart_eeprom;
    board->apu_impl.apu = &board->apu;
    board->mcu1284_impl.mcu = &board->mcu1284;
    board->linebuf_impl.sram = &board->linebuf;
    {
        int i;
        for (i = 0; i < R01S_BOM_HC157_N; i++) {
            board->linebuf_impl.mux157[i] = &board->mux157[i];
        }
    }
    {
        int i;
        for (i = 0; i < R01S_BOM_HC245_N; i++) {
            board->bus_impl.bus245[i] = &board->bus245[i];
        }
    }
    board->sprites_impl.fetch = &board->sprite_fetch;
    board->integration_impl.integ = &board->integration;

    r01s_pads_init(&board->pads, "PAD");
    r01s_sprite_fetch_init(&board->sprite_fetch, "UPLDN");
    r01s_integration_init(&board->integration, "UPLDP");

    if (r01s_island_builder_add(b, &ISLAND_POWER_VT, "ISLAND A  POWER", 0, 0, 1, 1, &board->power_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_CLOCK_VT, "ISLAND B  CLK RST", 0, 0, 1, 1, &board->clock_impl) <
        0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_CPU_VT, "ISLAND C  CPU RAM PLD", 0, 0, 1, 1,
                                &board->cpu_mem_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_IO_VT, "ISLAND D  FExx LATCH x9", 0, 0, 1, 1,
                                &board->io_latch_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_PADS_VT, "ISLAND E  PADS (1284)", 0, 0, 1, 1, &board->pads_impl) <
        0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_VRAM_VT, "ISLAND G  VRAM+PLD", 0, 0, 1, 1, &board->vram_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_BEAM_VT, "ISLAND H  BEAM PLD", 0, 0, 1, 1, &board->beam_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_BG_FETCH_VT, "ISLAND I  BG FETCH", 0, 0, 1, 1,
                                &board->bg_fetch_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_VIDEO_VT, "ISLAND O  VIDEO RGBS", 0, 0, 1, 1,
                                &board->video_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_CART_VT, "ISLAND J  CART FLASH", 0, 0, 1, 1,
                                &board->cart_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_APU_VT, "ISLAND K  APU 328P", 0, 0, 1, 1, &board->apu_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_MCU1284_VT, "ISLAND L  MCU 1284", 0, 0, 1, 1,
                                &board->mcu1284_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_LINEBUF_VT, "ISLAND M  LINEBUF", 0, 0, 1, 1,
                                &board->linebuf_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_SPRITES_VT, "ISLAND N  SPRITES", 0, 0, 1, 1,
                                &board->sprites_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_INTEGRATION_VT, "ISLAND P  INTEGRATION", 0, 0, 1, 1,
                                &board->integration_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_BUS_VT, "ISLAND Q  BUS HC245 x3", 0, 0, 1, 1, &board->bus_impl) <
        0) {
        return -1;
    }


    r01s_island_builder_mount_rel(b, r01s_pwr5v_entity(&board->pwr), R01S_ISLAND_POWER, 0, 0);
    {
        R01sEntity *osc_e = r01s_osc8m_entity(&board->osc);
        R01sEntity *hc_e = r01s_sn74hc14_entity(&board->hc14);
        int osc_y = (hc_e->body_h - osc_e->body_h) / 2;
        if (osc_y < 0) {
            osc_y = 0;
        }
        r01s_island_builder_mount_rel(b, osc_e, R01S_ISLAND_CLOCK, 0, osc_y);
        r01s_island_builder_mount_rel(b, hc_e, R01S_ISLAND_CLOCK, osc_e->body_w + R01S_CHIP_GAP, 0);
    }
    r01s_island_builder_mount_rel(b, r01s_w65c02s_entity(&board->cpu), R01S_ISLAND_CPU, 0, 0);
    {
        R01sEntity *cpu_e = r01s_w65c02s_entity(&board->cpu);
        R01sEntity *ram_e = r01s_as6c62256_entity(&board->ram);
        int x = cpu_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, ram_e, R01S_ISLAND_CPU, x, 0);
        x += ram_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, r01s_atf22v10_entity(&board->pld_decode), R01S_ISLAND_CPU, x, 0);
        x += r01s_atf22v10_entity(&board->pld_decode)->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, r01s_sn74hc245_entity(&board->bus245[R01S_BUS245_CPU]),
                                      R01S_ISLAND_CPU, x, 0);
    }
    {
        int i;
        int x = 0;
        int y = 0;
        for (i = 0; i < R01S_BOM_HC573_N; i++) {
            R01sEntity *e = r01s_sn74hc573_entity(&board->latch573[i]);
            r01s_island_builder_mount_rel(b, e, R01S_ISLAND_IO_LATCH, x, y);
            x += e->body_w + R01S_CHIP_GAP;
            if (x > 560) {
                x = 0;
                y += e->body_h + R01S_CHIP_GAP;
            }
        }
    }
    {
        R01sEntity *vram_e = r01s_as6c62256_entity(&board->vram);
        int x = 0;
        int y = 0;
        int i;
        r01s_island_builder_mount_rel(b, vram_e, R01S_ISLAND_VRAM, 0, 0);
        x = vram_e->body_w + R01S_CHIP_GAP;
        for (i = 0; i < 3; i++) {
            R01sEntity *mux_e = r01s_sn74hc157_entity(&board->mux157[i]);
            r01s_island_builder_mount_rel(b, mux_e, R01S_ISLAND_VRAM, x, 0);
            x += mux_e->body_w + R01S_CHIP_GAP;
        }
        r01s_island_builder_mount_rel(b, r01s_atf22v10_entity(&board->pld_vram), R01S_ISLAND_VRAM, x, 0);
        (void)y;
    }
    r01s_island_builder_mount_rel(b, r01s_bg_fetch_entity(&board->bg_fetch), R01S_ISLAND_BG_FETCH, 0, 0);
    {
        R01sEntity *dot_e = r01s_osc_dot_entity(&board->osc_dot);
        R01sEntity *beam_x = r01s_beam_xy_entity(&board->pld_beam_x);
        R01sEntity *beam_y = r01s_atf22v10_entity(&board->pld_beam_y);
        int x = 0;
        int beam_yoff = (dot_e->body_h - beam_x->body_h) / 2;
        if (beam_yoff < 0) {
            beam_yoff = 0;
        }
        r01s_island_builder_mount_rel(b, dot_e, R01S_ISLAND_BEAM, 0, 0);
        x = dot_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, beam_x, R01S_ISLAND_BEAM, x, beam_yoff);
        x += beam_x->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, beam_y, R01S_ISLAND_BEAM, x, 0);
    }
    {
        R01sEntity *comp_e = r01s_compositor_entity(&board->compositor);
        R01sEntity *prom_e = r01s_at28c16_entity(&board->color_prom);
        R01sEntity *sink_e = r01s_video_sink_entity(&board->video_sink);
        int x = 0;
        r01s_island_builder_mount_rel(b, comp_e, R01S_ISLAND_VIDEO, 0, 0);
        x += comp_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, prom_e, R01S_ISLAND_VIDEO, x, 0);
        x += prom_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, sink_e, R01S_ISLAND_VIDEO, x, 0);
    }
    {
        R01sEntity *flash_e = r01s_sst39sf040_entity(&board->cart_flash);
        R01sEntity *ee_e = r01s_i2c_eeprom_entity(&board->cart_eeprom);
        r01s_island_builder_mount_rel(b, flash_e, R01S_ISLAND_CART, 0, 0);
        r01s_island_builder_mount_rel(b, ee_e, R01S_ISLAND_CART, flash_e->body_w + R01S_CHIP_GAP, 0);
    }
    r01s_island_builder_mount_rel(b, r01s_atmega328p_entity(&board->apu), R01S_ISLAND_APU, 0, 0);
    r01s_island_builder_mount_rel(b, r01s_atmega1284p_entity(&board->mcu1284), R01S_ISLAND_MCU1284, 0, 0);
    {
        R01sEntity *lb_e = r01s_as6c62256_entity(&board->linebuf);
        int x = lb_e->body_w + R01S_CHIP_GAP;
        int i;
        r01s_island_builder_mount_rel(b, lb_e, R01S_ISLAND_LINEBUF, 0, 0);
        for (i = 3; i < R01S_BOM_HC157_N; i++) {
            R01sEntity *mux_e = r01s_sn74hc157_entity(&board->mux157[i]);
            int mux_y = (lb_e->body_h - mux_e->body_h) / 2;
            if (mux_y < 0) {
                mux_y = 0;
            }
            r01s_island_builder_mount_rel(b, mux_e, R01S_ISLAND_LINEBUF, x, mux_y);
            x += mux_e->body_w + R01S_CHIP_GAP;
        }
    }
    {
        int i;
        int x = 0;
        for (i = 0; i < R01S_BOM_HC245_N; i++) {
            R01sEntity *e = r01s_sn74hc245_entity(&board->bus245[i]);
            if (i == R01S_BUS245_CPU) {
                continue; /* mounted on CPU island */
            }
            r01s_island_builder_mount_rel(b, e, R01S_ISLAND_BUS, x, 0);
            x += e->body_w + R01S_CHIP_GAP;
        }
    }

    r01s_island_builder_fit_all(b);
    r01s_island_builder_arrange_rows(b, 40, 40, R01S_ISLAND_GAP, R01S_ISLAND_GAP, R01S_ISLAND_ROW_MAX_W);

    if (r01s_island_builder_finish(b) != 0) {
        return -1;
    }
    /* After island init (flash memset) — install bring-up cart image. */
    board_install_synthetic_cart(board);
    board_wire_cache_build(board);
    return 0;
}
