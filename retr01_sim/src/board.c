#include "retr01_sim/board.h"

#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/entity.h"
#include "retr01_sim/health.h"
#include "retr01_sim/play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R01S_RESET_HOLD 12
#define R01S_CART_HDR_SIZE 16

#define R01S_PAL_ROW_BYTES 16u
#define R01S_PAL_PLANE_BYTES 128u
#define R01S_ACTIVE_PAL_BYTES 32u
/*
 * DOT/beam ticks per board step. Real silicon runs DOT ~ PHI2 order; the UI
 * only does a few board steps/frame, so without a burst first VBlank takes minutes.
 * Keep this modest: CPU load scales with dots x settle x steps/frame.
 */
#define R01S_BEAM_DOTS_PER_STEP 32
/* Host Play: faster fields so ~1px/VBlank feels closer to 60 Hz game time. */
#define R01S_BEAM_DOTS_PER_STEP_PLAY 640
/* PRG init LDA #imm offsets (must match retr01_studio/prg_phase1.c). */
#define R01S_PRG_INIT_SCROLL_X 11u
#define R01S_PRG_INIT_SCROLL_Y 16u

/*
 * Bring-up smoke PRG (overlay into cart PRG window: not Studio game code).
 * Body through OAM readback is fixed. When cart meta is valid, install appends
 * pal+$FE08/$FE09 load and 480 B MAP->VRAM, then pad hang (addresses patched).
 * Ends with MAP seek 0 + LDA $FE93 ('r' cart magic) BEFORE any MAP stream so island health sticks.
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
    0xA9, 0x00,       /* LDA #$00: MAP seek 0 */
    0x8D, 0x90, 0xFE, /* STA $FE90 */
    0x8D, 0x91, 0xFE, /* STA $FE91 */
    0x8D, 0x92, 0xFE, /* STA $FE92 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 expect $72 'r' (cart magic) */
    0xA9, 0x10,       /* LDA #$10: APU period lo */
    0x8D, 0x41, 0xFE, /* STA $FE41 */
    0xA9, 0x00,       /* LDA #$00: APU period hi */
    0x8D, 0x42, 0xFE, /* STA $FE42 */
    0xA9, 0x8F,       /* LDA #$8F: enable + vol 8 */
    0x8D, 0x40, 0xFE, /* STA $FE40 */
    0xA9, 0x00,       /* LDA #$00: OAM addr 0 */
    0x8D, 0x20, 0xFE, /* STA $FE20 */
    0xA9, 0x10,       /* LDA #$10: Y */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x01,       /* LDA #$01: tile */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x00,       /* LDA #$00: attr */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x20,       /* LDA #$20: X */
    0x8D, 0x21, 0xFE, /* STA $FE21 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x20, 0xFE, /* STA $FE20 */
    0xAD, 0x21, 0xFE, /* LDA $FE21 expect $10 */
};

/* Pad hang only (used when cart has no world-0 MAP/CHR meta). */
static const uint8_t R01S_BRINGUP_HANG[] = {
    0xAD, 0x60, 0xFE, /* LDA $FE60 */
    0x4C, 0x00, 0x80, /* JMP hang: lo patched at install */
};

/*
 * Palette + MAP stream tail. Immediates for seeks patched at install.
 * Layout after smoke:
 *   seek global BG row (16 B) -> $FE08/$FE09
 *   seek global SPR row (16 B) -> continue auto-inc into sprite half
 *   seek MAP start screen -> 480 B VRAM
 *   hang
 */
enum {
    R01S_BR_OFF_BG_LO = 1,
    R01S_BR_OFF_BG_MID = 6,
    R01S_BR_OFF_BG_HI = 11,
    R01S_BR_OFF_SPR_LO = 32,
    R01S_BR_OFF_SPR_MID = 37,
    R01S_BR_OFF_SPR_HI = 42,
    R01S_BR_OFF_MAP_LO = 58,
    R01S_BR_OFF_MAP_MID = 63,
    R01S_BR_OFF_MAP_HI = 68,
};

static const uint8_t R01S_BRINGUP_STREAM[] = {
    /* BG palette row (16 master indices) */
    0xA9, 0x00,       /* LDA #bg_lo */
    0x8D, 0x90, 0xFE, /* STA $FE90 */
    0xA9, 0x00,       /* LDA #bg_mid */
    0x8D, 0x91, 0xFE, /* STA $FE91 */
    0xA9, 0x00,       /* LDA #bg_hi */
    0x8D, 0x92, 0xFE, /* STA $FE92 */
    0xA9, 0x00,       /* LDA #$00 */
    0x8D, 0x08, 0xFE, /* STA $FE08 */
    0xA2, 0x10,       /* LDX #16 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 */
    0x8D, 0x09, 0xFE, /* STA $FE09 */
    0xCA,             /* DEX */
    0xD0, 0xF7,       /* BNE *-9 */
    /* SPR palette row (pal_addr already 16 after BG copy) */
    0xA9, 0x00,       /* LDA #spr_lo */
    0x8D, 0x90, 0xFE, /* STA $FE90 */
    0xA9, 0x00,       /* LDA #spr_mid */
    0x8D, 0x91, 0xFE, /* STA $FE91 */
    0xA9, 0x00,       /* LDA #spr_hi */
    0x8D, 0x92, 0xFE, /* STA $FE92 */
    0xA2, 0x10,       /* LDX #16 */
    0xAD, 0x93, 0xFE, /* LDA $FE93 */
    0x8D, 0x09, 0xFE, /* STA $FE09 */
    0xCA,             /* DEX */
    0xD0, 0xF7,       /* BNE *-9 */
    /* Start-screen MAP -> VRAM */
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

static uint32_t cart_other_payload_abs(const uint8_t *img, size_t img_len, uint32_t off_other, uint32_t len_other,
                                       int id) {
    const uint8_t *blob;
    uint8_t count;
    int i;

    if (!img || len_other == 0 || id < 0 || id >= R01S_CART_OTHER_MAX ||
        (size_t)off_other + (size_t)len_other > img_len) {
        return 0;
    }
    blob = img + off_other;
    count = blob[0];
    if (count == 0 || count > R01S_CART_OTHER_MAX) {
        return 0;
    }
    for (i = 0; i < (int)count; i++) {
        const uint8_t *e = blob + R01S_CART_OTHER_HDR_BYTES + (size_t)i * R01S_CART_OTHER_DIR_BYTES;
        if ((int)e[0] == id) {
            uint32_t rel = get_u24(e + 4);
            return off_other + rel;
        }
    }
    return 0;
}

/* Absolute flash offset of one 16 B palette row inside a global plane. */
static uint32_t board_pal_row_off(uint32_t plane_off, uint32_t plane_len, unsigned row) {
    unsigned r = row & 7u;
    if (plane_len == 0 || plane_len >= R01S_PAL_PLANE_BYTES) {
        return plane_off + r * R01S_PAL_ROW_BYTES;
    }
    /* Legacy single-row blob (16 B): ignore row index. */
    return plane_off;
}

static void board_apply_active_pals_from_cart(R01sBoard *board) {
    uint32_t off_bg;
    uint32_t off_spr;
    unsigned i;

    if (!board || board->cart_off_pal_bg == 0) {
        return;
    }
    off_bg = board_pal_row_off(board->cart_off_pal_bg, board->cart_len_pal_bg, board->cart_default_pal_row);
    off_spr = board->cart_off_pal_spr
                  ? board_pal_row_off(board->cart_off_pal_spr, board->cart_len_pal_spr, board->cart_default_pal_row)
                  : off_bg + R01S_PAL_ROW_BYTES;
    for (i = 0; i < R01S_PAL_ROW_BYTES; i++) {
        board->active_pal[i] =
            (uint8_t)(r01s_sst39sf040_peek(&board->cart_flash, off_bg + i) & 63u);
        board->active_pal[R01S_PAL_ROW_BYTES + i] =
            (uint8_t)(r01s_sst39sf040_peek(&board->cart_flash, off_spr + i) & 63u);
    }
    board->chr_last_master = (uint8_t)(board->active_pal[0] & 63u);
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
     * (not A==$A5: that value is only used by the unit-test preload). */
    if (r01s_beam_xy_hblank(ctx->beam_impl.beam_x) || r01s_beam_xy_y(ctx->beam_impl.beam_x) > 0) {
        ctx->health_saw_beam = 1;
    }
    /* Smoke latches tile $42; after MAP stream VRAM is world data: either is OK. */
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
    if (r01s_atmega1284p_oam_peek(ctx->mcu_lb_impl.mcu, 0) == 0x10 &&
        r01s_atmega1284p_oam_peek(ctx->mcu_lb_impl.mcu, 1) == 0x01 &&
        r01s_atmega1284p_alive(ctx->mcu_lb_impl.mcu)) {
        ctx->health_saw_oam = 1;
    }
    /* Host Play: player OAM tile=1 (smoke Y=$10 is overwritten). */
    if (ctx->play.enabled && r01s_atmega1284p_oam_peek(ctx->mcu_lb_impl.mcu, 1) == 0x01 &&
        r01s_atmega1284p_oam_peek(ctx->mcu_lb_impl.mcu, 0) != 0xFF) {
        ctx->health_saw_oam = 1;
    }
    if (ctx->linebuf_saw_mux_mcu && ctx->linebuf_saw_mux_beam) {
        ctx->health_saw_linebuf = 1;
    }

}

/* Tone edges, or APU left disabled (studio cart MAP-only boot). */
static int board_apu_milestone_ok(const R01sBoard *ctx) {
    if (!ctx || !ctx->apu_impl.apu) {
        return 0;
    }
    if (ctx->health_saw_apu) {
        return 1;
    }
    return !r01s_atmega328p_enabled(ctx->apu_impl.apu);
}

static int board_integrated(const R01sBoard *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->health_saw_latch && ctx->health_saw_vram && ctx->health_saw_vram_read && ctx->health_saw_pad &&
           ctx->health_saw_beam && ctx->health_saw_bg_fetch && ctx->health_saw_video && ctx->health_saw_map &&
           board_apu_milestone_ok(ctx) && ctx->health_saw_oam && ctx->health_saw_linebuf &&
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

    /* Island A: power + clock / reset (merged canvas) */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_POWER_CLK];
        R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_clk_impl.pwr);
        R01sEntity *osc = r01s_osc8m_entity(ctx->power_clk_impl.osc);
        ih->letter = 'A';
        if (!group->powered) {
            ih->health = R01S_HEALTH_FAIL;
            snprintf(ih->activity, sizeof(ih->activity), "power switch off");
        } else if (!r01s_level_is_high(r01s_entity_sense(pwr, "VDD"))) {
            ih->health = R01S_HEALTH_FAIL;
            snprintf(ih->activity, sizeof(ih->activity), "5V rail missing");
        } else if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "reset hold (%d)", ctx->reset_hold);
        } else if (!group->running) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "5V up: clock halted");
        } else if (ctx->health_phi2_edges < 2) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "5V up: PHI2 starting");
        } else {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "5V + 8MHz PHI2");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=A POWER+CLK health=%s powered=%d VDD=%s running=%d reset_hold=%d PHI2=%s "
                 "edges=%u",
                 r01s_health_tag(ih->health), group->powered,
                 r01s_level_name(r01s_entity_sense(pwr, "VDD")), group->running, ctx->reset_hold,
                 r01s_level_name(r01s_entity_sense(osc, "PHI2")), (unsigned)ctx->health_phi2_edges);
    }

    /* Island C: CPU / RAM / PRG */
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

    /* Island D: $FExx latches */
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

    /* Island G: VRAM + BG nametable fetch (VRAM PLD) */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_VRAM];
        R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
        uint8_t v0 = r01s_as6c62256_peek(ctx->vram_impl.vram, 0);
        ih->letter = 'G';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "VRAM idle");
        } else if (ctx->health_saw_vram && ctx->health_saw_vram_read && ctx->health_saw_bg_fetch) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "VRAM[0]=$%02X NT $%02X/$%02X", v0,
                     r01s_bg_fetch_last_tile(bg), r01s_bg_fetch_last_attr(bg));
        } else if (ctx->health_saw_vram && ctx->health_saw_vram_read) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "VRAM ok await BG fetch");
        } else if (ctx->health_saw_vram) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await LDA $FE12 readback");
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await STA $FE12 ($%02X)", v0);
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=G VRAM health=%s saw_write=%d saw_readback=%d saw_bg=%d VA=$%04X VRAM[0]=$%02X "
                 "TILE=$%02X ATTR=$%02X fe12_armed=%d",
                 r01s_health_tag(ih->health), ctx->health_saw_vram, ctx->health_saw_vram_read,
                 ctx->health_saw_bg_fetch, (unsigned)r01s_bg_fetch_va(bg), v0,
                 r01s_bg_fetch_last_tile(bg), r01s_bg_fetch_last_attr(bg), ctx->vram_fe12_armed);
    }

    /* Island H: beam raster + VBlank NMI */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_BEAM];
        R01sIntegration *ig = ctx->integration_impl.integ;
        int bx = r01s_beam_xy_x(ctx->beam_impl.beam_x);
        int by = r01s_beam_xy_y(ctx->beam_impl.beam_x);
        int hb = r01s_beam_xy_hblank(ctx->beam_impl.beam_x);
        int vb = r01s_beam_xy_vblank(ctx->beam_impl.beam_x);
        unsigned pulses = (unsigned)r01s_integration_nmi_pulses(ig);
        ih->letter = 'H';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "beam idle");
        } else if (!group->running) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "raster frozen %d,%d", bx, by);
        } else if (ctx->health_saw_beam && ctx->health_saw_nmi) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "scan %d,%d NMI x%u%s%s", bx, by, pulses,
                     hb ? " HB" : "", vb ? " VB" : "");
        } else if (ctx->health_saw_beam) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "scan %d,%d await NMI", bx, by);
        } else {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "beam starting");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=H BEAM health=%s saw_beam=%d saw_nmi=%d pulses=%u X=%d Y=%d HBlank=%d VBlank=%d "
                 "EQ#=%s FE04=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_beam, ctx->health_saw_nmi, pulses, bx, by,
                 hb, vb,
                 r01s_level_name(r01s_entity_sense(r01s_atf22v10_entity(ctx->beam_impl.beam_y), "EQ#")),
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE04]));
    }

    /* Island O: Color PROM + compositor + LCD sink */
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
                 "island=O VIDEO health=%s saw=%d lit=%u samples=%u scale=%dx "
                 "comp_out=$%02X prom0=$%02X pixel00=$%02X",
                 r01s_health_tag(ih->health), ctx->health_saw_video,
                 (unsigned)r01s_video_sink_lit_pixels(sink), (unsigned)sink->dot_samples,
                 r01s_video_sink_scale_2x(sink) ? 2 : 1, r01s_compositor_out(ctx->video_impl.comp),
                 r01s_at28c16_peek(ctx->video_impl.prom, 0),
                 r01s_video_sink_pixel_packed(sink, 0, 0));
    }

    /* Island J: cart flash SST39SF040 */
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
            snprintf(ih->activity, sizeof(ih->activity), "await LDA $FE93 ('r')");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=J CART health=%s loaded=%d label=%s off_prg=$%06X len_prg=$%04X map=$%06X "
                 "saw_map=%d flash0=$%02X",
                 r01s_health_tag(ih->health), ctx->cart_loaded, ctx->cart_label[0] ? ctx->cart_label : "-",
                 (unsigned)ctx->cart_off_prg, (unsigned)ctx->cart_len_prg, (unsigned)(ctx->map_addr & 0xFFFFFFu),
                 ctx->health_saw_map, r01s_sst39sf040_peek(ctx->cart_impl.flash, 0));
    }

    /* Island K: ATmega328P APU */
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
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "APU idle");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=K APU health=%s saw=%d en=%d FE40=$%02X period=%u edges=%u hi=%u PWM=%s",
                 r01s_health_tag(ih->health), ctx->health_saw_apu, r01s_atmega328p_enabled(apu),
                 r01s_atmega328p_peek(apu, 0), (unsigned)r01s_atmega328p_period(apu),
                 (unsigned)r01s_atmega328p_pwm_edges(apu), (unsigned)r01s_atmega328p_pwm_hi_samples(apu),
                 r01s_level_name(r01s_entity_sense(r01s_atmega328p_entity(apu), "PWM")));
    }

    /* Island L: ATmega1284P + linebuf (merged canvas) */
    {
        R01sIslandHealth *ih = &out->islands[R01S_ISLAND_MCU_LB];
        R01sAtmega1284p *mcu = ctx->mcu_lb_impl.mcu;
        R01sAs6c62256 *lb = ctx->mcu_lb_impl.sram;
        R01sSpriteFetch *sf = ctx->sprites_impl.fetch;
        uint8_t p1 = r01s_pads_get(ctx->pads_impl.pads, 0);
        ih->letter = 'L';
        if (booting) {
            ih->health = R01S_HEALTH_BOOT;
            snprintf(ih->activity, sizeof(ih->activity), "1284/linebuf idle");
        } else if (ctx->health_saw_pad && ctx->health_saw_oam && ctx->health_saw_sprites &&
                   ctx->health_saw_linebuf) {
            ih->health = R01S_HEALTH_OK;
            snprintf(ih->activity, sizeof(ih->activity), "P1=$%02X OAM+LB show=%u", p1,
                     (unsigned)ctx->linebuf_show_half);
        } else if (ctx->health_saw_oam || ctx->health_saw_linebuf) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "OAM/LB partial pads=%d spr=%d",
                     ctx->health_saw_pad, ctx->health_saw_sprites);
        } else if (r01s_atmega1284p_alive(mcu)) {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "clk ok await OAM/LB");
        } else {
            ih->health = R01S_HEALTH_WARN;
            snprintf(ih->activity, sizeof(ih->activity), "await 20 MHz / OAM");
        }
        snprintf(ih->debug, sizeof(ih->debug),
                 "island=L MCU+LB health=%s saw_pad=%d saw_oam=%d saw_sprites=%d saw_lb=%d P1=$%02X "
                 "oam0=$%02X show=%u mux_mcu=%d mux_beam=%d spr_fills=%u",
                 r01s_health_tag(ih->health), ctx->health_saw_pad, ctx->health_saw_oam,
                 ctx->health_saw_sprites, ctx->health_saw_linebuf, p1,
                 r01s_atmega1284p_oam_peek(mcu, 0), (unsigned)ctx->linebuf_show_half,
                 ctx->linebuf_saw_mux_mcu, ctx->linebuf_saw_mux_beam,
                 (unsigned)r01s_sprite_fetch_fill_count(sf));
        (void)lb;
    }

    for (i = 0; i < out->island_count; i++) {

        system = r01s_health_worst(system, out->islands[i].health);
    }

    if (conflicts > 0) {
        out->system = R01S_HEALTH_FAIL;
        snprintf(out->system_label, sizeof(out->system_label), "BUS FAULT");
        snprintf(out->system_detail, sizeof(out->system_detail), "%u bus conflict(s): check wiring", conflicts);
    } else if (!group->powered || out->islands[R01S_ISLAND_POWER_CLK].health == R01S_HEALTH_FAIL) {
        out->system = R01S_HEALTH_FAIL;
        snprintf(out->system_label, sizeof(out->system_label), "POWER FAULT");
        snprintf(out->system_detail, sizeof(out->system_detail), "Island A must be up before integration");
    } else if (booting) {
        out->system = R01S_HEALTH_BOOT;
        snprintf(out->system_label, sizeof(out->system_label), "BOOTING");
        snprintf(out->system_detail, sizeof(out->system_detail), "Reset release: islands starting");
    } else if (!integrated) {
        out->system = R01S_HEALTH_WARN;
        snprintf(out->system_label, sizeof(out->system_label), "BRING-UP");
        snprintf(out->system_detail, sizeof(out->system_detail),
                 "L=%s V=%s P=%s B=%s BG=%s O=%s J=%s K=%s 1284=%s M=%s N=%s NMI=%s",
                 ctx->health_saw_latch ? "ok" : "-", ctx->health_saw_vram_read ? "ok" : "-",
                 ctx->health_saw_pad ? "ok" : "-", ctx->health_saw_beam ? "ok" : "-",
                 ctx->health_saw_bg_fetch ? "ok" : "-", ctx->health_saw_video ? "ok" : "-",
                 ctx->health_saw_map ? "ok" : "-", board_apu_milestone_ok(ctx) ? "ok" : "-",
                 ctx->health_saw_oam ? "ok" : "-", ctx->health_saw_linebuf ? "ok" : "-",
                 ctx->health_saw_sprites ? "ok" : "-", ctx->health_saw_nmi ? "ok" : "-");


    } else if (system == R01S_HEALTH_WARN) {
        out->system = R01S_HEALTH_WARN;
        snprintf(out->system_label, sizeof(out->system_label), "PAUSED");
        snprintf(out->system_detail, sizeof(out->system_detail),
                 group->running ? "One island idle or waiting" : "Integrated: press SPACE to run");
    } else {
        out->system = R01S_HEALTH_OK;
        snprintf(out->system_label, sizeof(out->system_label), "INTEGRATED");
        snprintf(out->system_detail, sizeof(out->system_detail),
                 group->running ? "All islands working together" : "All islands ok: paused");
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

static uint16_t board_cpu_addr(R01sBoard *ctx, R01sEntity *cpu_e) {
    (void)cpu_e;
    return r01s_w65c02s_ab(ctx->cpu_mem_impl.cpu);
}

static int board_cpu_read(R01sBoard *ctx, R01sEntity *cpu_e) {
    (void)cpu_e;
    return r01s_w65c02s_rwb(ctx->cpu_mem_impl.cpu);
}

static int board_cpu_be(R01sBoard *ctx, R01sEntity *cpu_e) {
    (void)ctx;
    return r01s_level_is_high(r01s_entity_sense(cpu_e, "BE"));
}

static uint8_t board_cpu_d_sample(R01sBoard *ctx, R01sEntity *cpu_e) {
    if (r01s_w65c02s_rwb(ctx->cpu_mem_impl.cpu)) {
        return (uint8_t)r01s_bus_read(cpu_e, "D", 8);
    }
    return r01s_w65c02s_a(ctx->cpu_mem_impl.cpu);
}

static void copy_bus_named(R01sBoard *ctx, R01sEntity *dst, const char *dst_prefix, R01sEntity *src,
                           const char *src_prefix, int width) {
    int i;
    char dn[16], sn[16];

    (void)ctx;
    for (i = 0; i < width; i++) {
        snprintf(dn, sizeof(dn), "%s%d", dst_prefix, i);
        snprintf(sn, sizeof(sn), "%s%d", src_prefix, i);
        r01s_entity_drive(dst, dn, r01s_entity_sense(src, sn));
    }
}

static void copy_cpu_d_to_latch_d(R01sBoard *ctx, R01sEntity *latch, R01sEntity *cpu) {
    int i;
    char ln[8], cn[8];

    (void)ctx;
    for (i = 0; i < 8; i++) {
        snprintf(ln, sizeof(ln), "%dD", i + 1);
        snprintf(cn, sizeof(cn), "D%d", i);
        r01s_entity_drive(latch, ln, r01s_entity_sense(cpu, cn));
    }
}

static void copy_latch_q_to_cpu_d(R01sBoard *ctx, R01sEntity *cpu, R01sEntity *latch) {
    int i;
    char ln[8], cn[8];

    (void)ctx;
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

/* MAP seek 0 must read flash[0] == 'r' ("retr01" cart header). */
static int board_map_byte_is_cart_magic(const R01sBoard *ctx, uint8_t dq) {
    return ctx && ctx->map_addr == 0 && dq == (uint8_t)'r';
}

/* Soft PLD: $FE02-$FE04 latches + $FE40-$FE5F APU + $FE60/$FE61 pads + $FE90-$FE93 MAP. */
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

enum {
    R01S_FLASH_CE_NONE = 0,
    R01S_FLASH_CE_PRG = 1,
    R01S_FLASH_CE_MAP = 2,
    R01S_FLASH_CE_CHR = 3,
};

/* Yield flash so CHR (DOT / HBlank) can own /CE without fighting a stale PRG claim. */
static void flash_yield_for_chr(R01sBoard *ctx) {
    R01sEntity *flash = r01s_sst39sf040_entity(ctx->cart_impl.flash);
    flash_deselect(flash);
    ctx->flash_ce_owner = R01S_FLASH_CE_NONE;
}

/* CHR plane read through flash CE#. Returns 0 if PRG/MAP owns the bus (caller holds). */
static int flash_chr_read_byte(R01sBoard *ctx, uint32_t abs, uint8_t *out) {
    R01sEntity *flash;
    if (!ctx || !out) {
        return 0;
    }
    if (ctx->flash_ce_owner == R01S_FLASH_CE_PRG || ctx->flash_ce_owner == R01S_FLASH_CE_MAP) {
        return 0;
    }
    flash = r01s_sst39sf040_entity(ctx->cart_impl.flash);
    ctx->flash_ce_owner = R01S_FLASH_CE_CHR;
    flash_read_selected(flash, abs);
    *out = (uint8_t)r01s_bus_read(flash, "DQ", 8);
    return 1;
}

static void flash_chr_release(R01sBoard *ctx) {
    if (!ctx || ctx->flash_ce_owner != R01S_FLASH_CE_CHR) {
        return;
    }
    flash_deselect(r01s_sst39sf040_entity(ctx->cart_impl.flash));
    ctx->flash_ce_owner = R01S_FLASH_CE_NONE;
}

static void sync_vram_addr_from_latches(R01sBoard *ctx) {
    uint8_t lo = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE10]);
    uint8_t hi = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE11]);
    ctx->vram_addr = (uint16_t)((((uint16_t)hi << 8) | lo) & 0x7FFFu);
}

static void sync_map_addr_from_latches(R01sBoard *ctx) {
    uint32_t lo = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE90]);
    uint32_t mid = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE91]);
    uint32_t hi = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE92]);
    ctx->map_addr = (hi << 16) | (mid << 8) | lo;
}

static void sync_pal_addr_from_latch(R01sBoard *ctx) {
    ctx->pal_addr =
        (uint8_t)(r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE08]) & 0x1Fu);
}

static void poke_vram_addr_latches(R01sBoard *ctx, uint16_t va) {
    va &= 0x7FFFu;
    r01s_sn74hc573_poke_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE10], (uint8_t)(va & 0xFFu));
    r01s_sn74hc573_poke_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE11], (uint8_t)((va >> 8) & 0xFFu));
    ctx->vram_addr = va;
}

static void poke_map_addr_latches(R01sBoard *ctx, uint32_t ma) {
    ma &= 0xFFFFFFu;
    r01s_sn74hc573_poke_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE90], (uint8_t)(ma & 0xFFu));
    r01s_sn74hc573_poke_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE91], (uint8_t)((ma >> 8) & 0xFFu));
    r01s_sn74hc573_poke_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE92], (uint8_t)((ma >> 16) & 0xFFu));
    ctx->map_addr = ma;
}

static void poke_pal_addr_latch(R01sBoard *ctx, uint8_t pa) {
    pa &= 0x1Fu;
    r01s_sn74hc573_poke_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE08], pa);
    ctx->pal_addr = pa;
}

/* Pulse HC573 LE from decode SEL; route D<->Q on pin path. */
static void latch_port_cycle(R01sBoard *ctx, R01sEntity *latch, R01sEntity *cpu, int selected,
                             int read) {
    r01s_entity_drive(latch, "OE", R01S_LVL_L);
    r01s_entity_drive(latch, "LE", R01S_LVL_L);
    if (!selected) {
        r01s_entity_eval(latch);
        return;
    }
    copy_cpu_d_to_latch_d(ctx, latch, cpu);
    if (!read) {
        r01s_entity_drive(latch, "LE", R01S_LVL_H);
    }
    r01s_entity_eval(latch);
    if (read) {
        copy_latch_q_to_cpu_d(ctx, cpu, latch);
    }
    r01s_entity_drive(latch, "LE", R01S_LVL_L);
    r01s_entity_eval(latch);
}

static int pld_sel(R01sEntity *pld, const char *name) {
    return r01s_level_is_high(r01s_entity_sense(pld, name));
}

/* Drive decode PLD from CPU A/BE; SEL_FE* qualify HC573 / MAP / VRAM ports. */
static void wire_decode(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *pld = r01s_atf22v10_entity(ctx->cpu_mem_impl.pld_decode);
    uint16_t addr = board_cpu_addr(ctx, cpu);
    int be = board_cpu_be(ctx, cpu);
    int i;
    char an[4];

    for (i = 0; i < 8; i++) {
        snprintf(an, sizeof(an), "A%d", i);
        r01s_entity_drive(pld, an, (addr & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
    r01s_entity_drive(pld, "FE#", ((addr & 0xFF00u) == 0xFE00u) ? R01S_LVL_L : R01S_LVL_H);
    r01s_entity_drive(pld, "BE", be ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(pld, "RWB", board_cpu_read(ctx, cpu) ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_eval(pld);
}

static void wire_io(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *pld = r01s_atf22v10_entity(ctx->cpu_mem_impl.pld_decode);
    R01sEntity *latch = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
    R01sEntity *scroll_y = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]);
    R01sEntity *raster = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE04]);
    R01sEntity *pal_latch = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE08]);
    R01sEntity *map_lo = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE90]);
    R01sEntity *map_mid = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE91]);
    R01sEntity *map_hi = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE92]);
    R01sEntity *pads = r01s_pads_entity(ctx->pads_impl.pads);
    R01sEntity *apu = r01s_atmega328p_entity(ctx->apu_impl.apu);
    R01sEntity *mcu = r01s_atmega1284p_entity(ctx->mcu_lb_impl.mcu);
    R01sEntity *flash = r01s_sst39sf040_entity(ctx->cart_impl.flash);
    uint16_t addr = board_cpu_addr(ctx, cpu);
    int read = board_cpu_read(ctx, cpu);
    int be = board_cpu_be(ctx, cpu);
    int hit_oam_addr = (addr == 0xFE20u);
    int hit_oam_data = (addr == 0xFE21u);
    int hit_oam = hit_oam_addr || hit_oam_data;
    int hit_eeprom = (addr >= 0xFE70u && addr <= 0xFE72u);
    int hit_apu = (addr >= 0xFE40u && addr <= 0xFE5Fu);
    int hit_pads = (addr == 0xFE60u || addr == 0xFE61u);
    int hit_pal_data = (addr == 0xFE09u);
    int hit_map_data;
    int sel_fe02, sel_fe03, sel_fe04, sel_fe08, sel_fe90, sel_fe91, sel_fe92, sel_fe93;
    int ai;

    wire_decode(ctx);
    sel_fe02 = pld_sel(pld, "SEL_FE02");
    sel_fe03 = pld_sel(pld, "SEL_FE03");
    sel_fe04 = pld_sel(pld, "SEL_FE04");
    sel_fe08 = pld_sel(pld, "SEL_FE08");
    sel_fe90 = pld_sel(pld, "SEL_FE90");
    sel_fe91 = pld_sel(pld, "SEL_FE91");
    sel_fe92 = pld_sel(pld, "SEL_FE92");
    sel_fe93 = pld_sel(pld, "SEL_FE93");
    hit_map_data = sel_fe93;

    /* Default: I/O devices idle */
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
        latch_port_cycle(ctx, latch, cpu, 0, read);
        latch_port_cycle(ctx, scroll_y, cpu, 0, read);
        latch_port_cycle(ctx, raster, cpu, 0, read);
        latch_port_cycle(ctx, pal_latch, cpu, 0, read);
        latch_port_cycle(ctx, map_lo, cpu, 0, read);
        latch_port_cycle(ctx, map_mid, cpu, 0, read);
        latch_port_cycle(ctx, map_hi, cpu, 0, read);
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

    /* Decode PLD SEL -> HC573 LE (BOM latches on pin path). */
    latch_port_cycle(ctx, latch, cpu, sel_fe02, read);
    latch_port_cycle(ctx, scroll_y, cpu, sel_fe03, read);
    latch_port_cycle(ctx, raster, cpu, sel_fe04, read);
    latch_port_cycle(ctx, pal_latch, cpu, sel_fe08, read);
    latch_port_cycle(ctx, map_lo, cpu, sel_fe90, read);
    latch_port_cycle(ctx, map_mid, cpu, sel_fe91, read);
    latch_port_cycle(ctx, map_hi, cpu, sel_fe92, read);
    if (sel_fe08) {
        sync_pal_addr_from_latch(ctx);
    }
    if (sel_fe90 || sel_fe91 || sel_fe92) {
        sync_map_addr_from_latches(ctx);
    }

    /* Island L: OAM $FE20/$FE21 (hold WE#/OE# across settle; chip edge-inc). */
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

    /* Soft $FE70-$FE72 machine-EEPROM mailbox (protocol TBD: Island F). */
    if (hit_eeprom) {
        unsigned ei = (unsigned)(addr - 0xFE70u);
        if (read) {
            r01s_bus_write(cpu, "D", 8, r01s_atmega1284p_eeprom_peek(ctx->mcu_lb_impl.mcu, ei));
        } else {
            r01s_atmega1284p_eeprom_poke(ctx->mcu_lb_impl.mcu, ei, board_cpu_d_sample(ctx, cpu));
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

    /* Island J: MAP $FE93: flash CE via decode SEL (seek from HC573 FE90-92). */
    if (hit_map_data && read && ctx->cart_loaded &&
        r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu) == R01S_CPU_OP_DATA) {
        uint8_t dq;
        sync_map_addr_from_latches(ctx);
        flash_read_selected(flash, ctx->map_addr);
        copy_bus_named(ctx, cpu, "D", flash, "DQ", 8);
        dq = board_cpu_d_sample(ctx, cpu);
        ctx->flash_ce_owner = R01S_FLASH_CE_MAP;
        if (board_map_byte_is_cart_magic(ctx, dq)) {
            ctx->health_saw_map = 1; /* cart magic 'r' at seek 0 */
        }
        ctx->map_fe93_armed = 1;
    } else if (hit_map_data) {
        flash_deselect(flash);
        if (ctx->flash_ce_owner == R01S_FLASH_CE_MAP) {
            ctx->flash_ce_owner = R01S_FLASH_CE_NONE;
        }
    }

    /* $FE09 palette data; addr index from HC573 FE08. */
    if (hit_pal_data && !read &&
        r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu) == R01S_CPU_OP_DATA && !ctx->pal_fe09_wrote) {
        sync_pal_addr_from_latch(ctx);
        ctx->active_pal[ctx->pal_addr & 0x1Fu] = board_cpu_d_sample(ctx, cpu);
        poke_pal_addr_latch(ctx, (uint8_t)((ctx->pal_addr + 1u) & 0x1Fu));
        ctx->pal_fe09_wrote = 1;
    }
    if (hit_pal_data && read) {
        sync_pal_addr_from_latch(ctx);
        r01s_bus_write(cpu, "D", 8, ctx->active_pal[ctx->pal_addr & 0x1Fu]);
    }
}

/* Island H: DOT osc + beam PLD + Y-compare vs $FE04; EQ# drives CPU IRQB. */
static void wire_beam(R01sBoard *ctx, R01sIslandGroup *group) {
    R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_clk_impl.pwr);
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
            char ln[16];
            snprintf(ln, sizeof(ln), "%dQ", i + 1);
            r01s_entity_drive(beam_y, qn, r01s_entity_sense(raster, ln));
        }
    }
    r01s_entity_drive(beam_y, "OE#", R01S_LVL_L);
    r01s_entity_eval(beam);
    r01s_entity_eval(beam_y);
    /* Active-low raster match -> IRQB (CPU IRQ service still Phase-1 stub). */
    r01s_entity_drive(cpu, "IRQB", r01s_entity_sense(beam_y, "EQ#"));
}

/*
 * Island G: VRAM port $FE10/$FE11/$FE12 + PHI2 interleave.
 * CPU phase (PHI2 high): CPU may R/W via HC573 FE10/FE11 + FE12.
 * PPU phase (PHI2 low): mux selects BG fetch VA; VRAM OE for nametable.
 * Auto-inc arms on FE12 access; committed on next PHI2 rising edge (poke latches).
 */
static void wire_vram(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *pld = r01s_atf22v10_entity(ctx->cpu_mem_impl.pld_decode);
    R01sEntity *vram = r01s_as6c62256_entity(ctx->vram_impl.vram);
    R01sEntity *mux = r01s_sn74hc157_entity(ctx->vram_impl.mux157[R01S_MUX157_VRAM0]);
    R01sEntity *osc = r01s_osc8m_entity(ctx->power_clk_impl.osc);
    R01sEntity *fe10 = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE10]);
    R01sEntity *fe11 = r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[R01S_LATCH_FE11]);
    R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
    uint16_t cpu_addr = board_cpu_addr(ctx, cpu);
    int read = board_cpu_read(ctx, cpu);
    int be = board_cpu_be(ctx, cpu);
    int cpu_phase = r01s_level_is_high(r01s_entity_sense(osc, "PHI2"));
    int hit_data;
    uint16_t va;
    uint16_t ppu_va = (uint16_t)(r01s_bg_fetch_va(bg) & 0x7FFFu);
    uint16_t sram_addr;
    int i;

    /* FE10/FE11 via decode SEL -> HC573 (not PHI2-gated). */
    wire_decode(ctx);
    latch_port_cycle(ctx, fe10, cpu, pld_sel(pld, "SEL_FE10"), read);
    latch_port_cycle(ctx, fe11, cpu, pld_sel(pld, "SEL_FE11"), read);
    if (pld_sel(pld, "SEL_FE10") || pld_sel(pld, "SEL_FE11")) {
        sync_vram_addr_from_latches(ctx);
    }
    hit_data = pld_sel(pld, "SEL_FE12");
    va = (uint16_t)(ctx->vram_addr & 0x7FFFu);
    (void)cpu_addr;
    (void)be;

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
 * Island M: sprite line-buffer SRAM (no CPU port).
 * Soft 1284 fill on HBlank entry; beam reads show half on visible dots.
 * HC157: AB low = MCU fill addr, AB high = beam X.
 */
static void linebuf_drive_addr(R01sBoard *ctx, uint16_t addr, int mcu_sel) {
    R01sEntity *sram = r01s_as6c62256_entity(ctx->mcu_lb_impl.sram);
    R01sEntity *mux = r01s_sn74hc157_entity(ctx->mcu_lb_impl.mux157[R01S_MUX157_LINEBUF0]);
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
    R01sEntity *sram = r01s_as6c62256_entity(ctx->mcu_lb_impl.sram);
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
#define R01S_ATTR_SOLID 0x40u

/* CHR plane byte via flash /CE. Returns 0 on CE deny (caller should hold). */
static int board_chr_flash_byte(R01sBoard *ctx, uint32_t abs, uint8_t *out) {
    if (!ctx || !out || abs >= sizeof(ctx->cart_flash.mem)) {
        return 0;
    }
    return flash_chr_read_byte(ctx, abs, out);
}

/* 2bpp CHR via flash CE#. *ok=0 if CE denied mid-fetch. */
static uint8_t board_chr_color(R01sBoard *ctx, uint32_t chr_base, uint8_t tile, uint8_t attr, int px,
                               int py, int *ok) {
    uint8_t bank = (uint8_t)(attr & R01S_ATTR_BANK);
    int row = py & 7;
    int col = px & 7;
    uint32_t tbase;
    uint8_t p0 = 0xFF, p1 = 0xFF;
    int bit;

    if (ok) {
        *ok = 1;
    }
    if (attr & R01S_ATTR_FLIP_V) {
        row = 7 - row;
    }
    if (attr & R01S_ATTR_FLIP_H) {
        col = 7 - col;
    }
    tbase = chr_base + (uint32_t)bank * R01S_CHR_BANK_BYTES + (uint32_t)tile * R01S_CHR_TILE_BYTES;
    if (!board_chr_flash_byte(ctx, tbase + (uint32_t)row, &p0) ||
        !board_chr_flash_byte(ctx, tbase + 8u + (uint32_t)row, &p1)) {
        if (ok) {
            *ok = 0;
        }
        return 0;
    }
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
    int sx;
    int sy;
    int slot_x, slot_y, slot, local_x, local_y, tx, ty, cell;
    uint16_t addr;
    uint8_t scroll_x = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
    uint8_t scroll_y = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]);

    *tile_out = 0;
    *attr_out = 0;
    if (lx < 0 || ly < 0 || lx >= R01S_LOGICAL_W || ly >= R01S_LOGICAL_H) {
        return;
    }
    /* Match bg_fetch: scroll within 0-127 / 0-119, then add logical (2x2 workbench). */
    sx = (int)(scroll_x & 127u) + lx;
    sy = (int)(scroll_y < 120u ? scroll_y : 119u) + ly;
    slot_x = (sx / R01S_BG_SCREEN_PX_W) & 1;
    slot_y = (sy / R01S_BG_SCREEN_PX_H) & 1;
    slot = slot_y * 2 + slot_x;
    local_x = sx - slot_x * R01S_BG_SCREEN_PX_W;
    local_y = sy - slot_y * R01S_BG_SCREEN_PX_H;
    if (local_x < 0) {
        local_x = 0;
    }
    if (local_y < 0) {
        local_y = 0;
    }
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

static uint32_t board_map_off_for_screen(const R01sBoard *board, int col, int row);

/* Host Play L0 sample under L1 color 0 / missing slot (cart BG0 cache + CHR via flash). */
static uint8_t board_l0_master_at(R01sBoard *ctx, int lx, int ly) {
    int wx, wy, gc, gr, local_x, local_y, tx, ty, cell, i;
    uint8_t tile, attr, color, pal, master;
    int chr_ok = 1;
    const uint8_t *map = NULL;

    if (!ctx || ctx->bg0_count < 1) {
        return (uint8_t)(ctx ? (ctx->active_pal[0] & 63u) : 0);
    }
    wx = ctx->l0_cam_x + lx;
    wy = ctx->l0_cam_y + ly;
    if (wx < 0 || wy < 0) {
        return (uint8_t)(ctx->active_pal[0] & 63u);
    }
    gc = wx / R01S_BG_SCREEN_PX_W;
    gr = wy / R01S_BG_SCREEN_PX_H;
    for (i = 0; i < ctx->bg0_count; i++) {
        if (ctx->bg0[i].present && (int)ctx->bg0[i].col == gc && (int)ctx->bg0[i].row == gr) {
            map = ctx->bg0[i].map;
            break;
        }
    }
    if (!map || ctx->cart_off_chr == 0) {
        return (uint8_t)(ctx->active_pal[0] & 63u);
    }
    local_x = wx - gc * R01S_BG_SCREEN_PX_W;
    local_y = wy - gr * R01S_BG_SCREEN_PX_H;
    tx = local_x / 8;
    ty = local_y / 8;
    if (tx < 0) {
        tx = 0;
    }
    if (ty < 0) {
        ty = 0;
    }
    if (tx >= R01S_BG_SCREEN_TILES_X) {
        tx = R01S_BG_SCREEN_TILES_X - 1;
    }
    if (ty >= 15) {
        ty = 14;
    }
    cell = ty * R01S_BG_SCREEN_TILES_X + tx;
    tile = map[cell];
    attr = map[R01S_BG_ATTR_OFF + cell];
    color = board_chr_color(ctx, ctx->cart_off_chr, tile, attr, local_x & 7, local_y & 7, &chr_ok);
    if (!chr_ok) {
        return ctx->chr_last_master;
    }
    if (color == 0) {
        flash_chr_release(ctx);
        return (uint8_t)(ctx->active_pal[0] & 63u);
    }
    pal = (uint8_t)((attr & R01S_ATTR_PAL) >> R01S_ATTR_PAL_SHIFT);
    master = board_pal_master(ctx, 0, pal, color);
    flash_chr_release(ctx);
    return master;
}

static uint8_t board_bg_master_at(R01sBoard *ctx, int lx, int ly) {
    uint8_t tile, attr, color, pal, master;
    int local_x, local_y;
    uint8_t scroll_x, scroll_y;
    int sx, sy, slot_x, slot_y, slot;
    int chr_ok = 1;

    scroll_x = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE02]);
    scroll_y = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch573[R01S_LATCH_FE03]);
    sx = (int)(scroll_x & 127u) + lx;
    sy = (int)(scroll_y < 120u ? scroll_y : 119u) + ly;
    slot_x = (sx / R01S_BG_SCREEN_PX_W) & 1;
    slot_y = (sy / R01S_BG_SCREEN_PX_H) & 1;
    slot = slot_y * 2 + slot_x;
    /* Match emu Host Play: missing L1 slot -> L0 show-through, else backdrop. */
    if (!ctx->vram_slot_present[slot & 3]) {
        master = board_l0_master_at(ctx, lx, ly);
        ctx->chr_last_master = master;
        return master;
    }

    if (ctx->cart_off_chr == 0) {
        board_vram_cell_at(ctx, lx, ly, &tile, &attr);
        master = (uint8_t)(tile & 0x3Fu);
        ctx->chr_last_master = master;
        return master;
    }
    board_vram_cell_at(ctx, lx, ly, &tile, &attr);
    local_x = sx - slot_x * R01S_BG_SCREEN_PX_W;
    local_y = sy - slot_y * R01S_BG_SCREEN_PX_H;
    color = board_chr_color(ctx, ctx->cart_off_chr, tile, attr, local_x & 7, local_y & 7, &chr_ok);
    if (!chr_ok) {
        /* PRG/MAP owns flash /CE: hold last master (no fight). */
        return ctx->chr_last_master;
    }
    if (color == 0) {
        flash_chr_release(ctx);
        master = board_l0_master_at(ctx, lx, ly);
        ctx->chr_last_master = master;
        return master;
    }
    pal = (uint8_t)((attr & R01S_ATTR_PAL) >> R01S_ATTR_PAL_SHIFT);
    master = board_pal_master(ctx, 0, pal, color);
    ctx->chr_last_master = master;
    flash_chr_release(ctx);
    return master;
}



/* Island N: clear half, OAM-scan logical Y, paint <=16 sprites (CHR via flash CE in HBlank). */
static void linebuf_oam_fill_half(R01sBoard *ctx, int half, int logical_y) {
    int i;
    int si;
    int painted = 0;
    uint32_t pixels = 0;
    uint8_t hit_x = 0;
    uint8_t hit_color = 0;
    uint16_t base = (uint16_t)((half & 1) << 7);
    R01sAtmega1284p *mcu = ctx->mcu_lb_impl.mcu;
    R01sSpriteFetch *sf = ctx->sprites_impl.fetch;

    /* HBlank steals flash from PRG: dedicated CHR window. */
    flash_yield_for_chr(ctx);

    for (i = 0; i < 128; i++) {
        linebuf_write_byte(ctx, (uint16_t)(base + (unsigned)i), 0);
    }

    for (si = 0; si < 64 && painted < 16; si++) {
        uint8_t oy_u = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 0));
        uint8_t tile = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 1));
        uint8_t attr = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 2));
        uint8_t ox_u = r01s_atmega1284p_oam_peek(mcu, (uint8_t)(si * 4 + 3));
        int oy = r01s_oam_coord_from_u8(oy_u);
        int ox = r01s_oam_coord_from_u8(ox_u);
        int h = (attr & 0x80u) ? 16 : 8;
        int px;

        if (tile == 0xFFu) {
            continue;
        }
        if (logical_y < oy || logical_y >= oy + h) {
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
                int chr_ok = 1;
                if (x < 0 || x >= 128) {
                    continue;
                }
                if (spr_chr) {
                    uint8_t c2 = board_chr_color(ctx, spr_chr, tile, attr, px, row, &chr_ok);
                    if (!chr_ok || c2 == 0) {
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

    flash_chr_release(ctx);

    r01s_sprite_fetch_note_fill(sf, (uint8_t)(logical_y & 0xFF), (uint8_t)painted, pixels, hit_x,
                                hit_color);
    /* Opaque pixels or an OAM hit on this line (CHR may be blank for smoke tile). */
    if (pixels > 0 || painted > 0) {
        ctx->health_saw_sprites = 1;
    }
}


static void wire_linebuf(R01sBoard *ctx) {
    R01sEntity *sram = r01s_as6c62256_entity(ctx->mcu_lb_impl.sram);
    int hblank = r01s_beam_xy_hblank(ctx->beam_impl.beam_x);
    int bx = r01s_beam_xy_x(ctx->beam_impl.beam_x);
    int by = r01s_beam_xy_y(ctx->beam_impl.beam_x);
    int lx;
    int ly;
    int scale_2x = r01s_video_sink_scale_2x(ctx->video_impl.sink);
    uint16_t show_addr;

    /* Entering HBlank: OAM-fill next half for next logical Y, then show it. */
    if (hblank && !ctx->linebuf_prev_hblank) {
        int next_by = by + 1;
        int next_ly;
        int fill_half;
        int probe_x = scale_2x ? 0 : R01S_SCALE_1X_OX;
        if (next_by >= R01S_BEAM_DOTS_Y) {
            next_by = 0;
        }
        fill_half = ctx->linebuf_show_half ^ 1;
        if (r01s_rgbs_beam_to_logical(scale_2x, probe_x, next_by, &lx, &next_ly)) {
            linebuf_oam_fill_half(ctx, fill_half, next_ly);
            ctx->linebuf_show_half = (uint8_t)(fill_half & 1);
        }
        /* Border / VBlank lines: keep prior half (no OAM fill). */
    }
    ctx->linebuf_prev_hblank = (uint8_t)(hblank ? 1 : 0);

    r01s_entity_drive(sram, "CE#", R01S_LVL_H);
    r01s_entity_drive(sram, "OE#", R01S_LVL_H);
    r01s_entity_drive(sram, "WE#", R01S_LVL_H);
    r01s_bus_hiz(sram, "DQ", 8);

    if (!hblank && r01s_rgbs_beam_to_logical(scale_2x, bx, by, &lx, &ly)) {
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


/* Island I: BG fetch address from beam + scroll; eval before VRAM uses VA. */
static void wire_bg_fetch(R01sBoard *ctx) {
    R01sEntity *osc = r01s_osc8m_entity(ctx->power_clk_impl.osc);
    R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
    int cpu_phase = r01s_level_is_high(r01s_entity_sense(osc, "PHI2"));

    r01s_bg_fetch_set_scale_2x(bg, r01s_video_sink_scale_2x(ctx->video_impl.sink));
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

/* Hold LCD while bring-up MAP-streams tiles then attrs (avoids sky->unflipped->flipped). */
static int board_video_held_for_map_stream(const R01sBoard *ctx) {
    if (!ctx || ctx->cart_off_map_screen0 == 0) {
        return 0;
    }
    /* Host Play applies camera scroll after catchup; cart PRG init owns boot scroll. */
    if (ctx->play.enabled) {
        return 0;
    }
    /* Hold during MAP stream and until play latches scroll + 2x2 camera. */
    return 1;
}

/*
 * Island O: dot-sampled BG -> compositor -> Color PROM -> LCD sink.
 * CHR: flash /CE during DOT window (yield PRG first); hold chr_last_master on deny.
 * Sink is the 256x240 RGBS field; SCALE maps beam -> logical 128x120.
 */
static void wire_video_dot(R01sBoard *ctx) {
    wire_bg_fetch(ctx);
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
    int scale_2x = r01s_video_sink_scale_2x(sink);
    uint8_t bg;
    uint8_t idx;

    if (r01s_beam_xy_hblank(beam) || r01s_beam_xy_vblank(beam) || bx >= R01S_BEAM_VISIBLE_W ||
        by >= R01S_BEAM_VISIBLE_H) {
        return;
    }
    /* Blank until nametable+attrs finished streaming (real games would VBlank-load). */
    if (board_video_held_for_map_stream(ctx)) {
        r01s_video_sink_plot(sink, bx, by, 0);
        return;
    }
    if (!r01s_rgbs_beam_to_logical(scale_2x, bx, by, &lx, &ly)) {
        /* 1x border: blank (cheap). Raster timing stays 341x262 regardless. */
        r01s_video_sink_plot(sink, bx, by, 0);
        return;
    }
    bg = board_bg_master_at(ctx, lx, ly);
    r01s_compositor_set_bg(comp, bg);
    {
        uint16_t spr_addr = (uint16_t)(((ctx->linebuf_show_half & 1u) << 7) | (lx & 0x7F));
        uint8_t spr = r01s_as6c62256_peek(ctx->mcu_lb_impl.sram, spr_addr);
        r01s_compositor_set_sprite(comp, (uint8_t)(spr & 0x3Fu), spr != 0);
        if (spr != 0) {
            ctx->health_saw_sprites = 1;
        }
    }
    r01s_entity_eval(comp_e);

    idx = r01s_compositor_out(comp);
    wire_video_prom_addr(prom_e, idx);
    (void)r01s_at28c16_peek(prom, idx); /* Color PROM still on the digital video path */
    r01s_video_sink_plot(sink, bx, by, idx);
}

static void wire_memory(R01sBoard *ctx) {
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
    /* PRG vs MAP vs CHR: only one flash /CE. MAP owns flash inside wire_io on $FE93. */
    if (!(io && addr == 0xFE93u)) {
        flash_deselect(flash);
        if (ctx->flash_ce_owner == R01S_FLASH_CE_PRG || ctx->flash_ce_owner == R01S_FLASH_CE_CHR) {
            ctx->flash_ce_owner = R01S_FLASH_CE_NONE;
        }
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
            ctx->flash_ce_owner = R01S_FLASH_CE_PRG;
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
    R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_clk_impl.pwr);
    R01sEntity *osc = r01s_osc8m_entity(ctx->power_clk_impl.osc);
    R01sEntity *hc = r01s_sn74hc14_entity(ctx->power_clk_impl.hc14);
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
    /* Unused Schmitt gates: tie inputs (real boards do this; avoids X on 3Y-6Y). */
    r01s_entity_drive(hc, "3A", R01S_LVL_H);
    r01s_entity_drive(hc, "4A", R01S_LVL_H);
    r01s_entity_drive(hc, "5A", R01S_LVL_H);
    r01s_entity_drive(hc, "6A", R01S_LVL_H);
    r01s_entity_eval(hc);

    r01s_entity_drive(cpu, "PHI2", phi2 == R01S_LVL_H ? R01S_LVL_H : R01S_LVL_L);
}

static void board_settle_n(R01sBoard *ctx, R01sIslandGroup *group, int passes) {
    int i;
    if (passes < 1) {
        passes = 1;
    }
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

static void board_settle(R01sBoard *ctx, R01sIslandGroup *group) {
    board_settle_n(ctx, group, R01S_SETTLE_PASSES);
}

void r01s_board_catchup_finish(R01sBoard *board) {
    if (!board || !board->cart_loaded) {
        return;
    }
    if (board->cart_off_map_screen0 != 0) {
        poke_map_addr_latches(board, board->cart_off_map_screen0 + 480u);
        board->health_saw_map = 1;
    }
    if (board->cart_off_sdir != 0) {
        (void)r01s_board_load_camera_2x2(board, (int)board->cart_start_col, (int)board->cart_start_row);
    } else if (board->cart_off_map_screen0 != 0) {
        /* Synthetic / no directory: pin stream wrote slot-0 bytes at VRAM $0000. */
        board->vram_slot_present[0] = 1;
    }
}

static void island_power_clk_init(R01sIsland *island) {
    R01sIslandPowerClkImpl *impl = (R01sIslandPowerClkImpl *)island->impl;
    r01s_pwr5v_init(impl->pwr, "PS1");
    r01s_osc8m_init(impl->osc, "Y1");
    r01s_sn74hc14_init(impl->hc14, "U2");
    r01s_island_add_entity(island, r01s_pwr5v_entity(impl->pwr));
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
    if (impl->bus245_cpu) {
        r01s_sn74hc245_init(impl->bus245_cpu, "U20A");
        r01s_island_add_entity(island, r01s_sn74hc245_entity(impl->bus245_cpu));
    }
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

static void island_video_init(R01sIsland *island) {
    R01sIslandVideoImpl *impl = (R01sIslandVideoImpl *)island->impl;
    r01s_compositor_init(impl->comp, "UPLDV");
    r01s_at28c16_init(impl->prom, "U24");
    r01s_video_sink_init(impl->sink, "SCR1");
    r01s_island_add_entity(island, r01s_compositor_entity(impl->comp));
    r01s_island_add_entity(island, r01s_at28c16_entity(impl->prom));
    r01s_island_add_entity(island, r01s_video_sink_entity(impl->sink));
    if (impl->bus245_video) {
        r01s_sn74hc245_init(impl->bus245_video, "U20B");
        r01s_island_add_entity(island, r01s_sn74hc245_entity(impl->bus245_video));
    }
}

static void island_cart_init(R01sIsland *island) {
    R01sIslandCartImpl *impl = (R01sIslandCartImpl *)island->impl;
    r01s_sst39sf040_init(impl->flash, "U40");
    r01s_i2c_eeprom_init(impl->save_eeprom, "U50");
    r01s_island_add_entity(island, r01s_sst39sf040_entity(impl->flash));
    r01s_island_add_entity(island, r01s_i2c_eeprom_entity(impl->save_eeprom));
    if (impl->bus245_cart) {
        r01s_sn74hc245_init(impl->bus245_cart, "U20C");
        r01s_island_add_entity(island, r01s_sn74hc245_entity(impl->bus245_cart));
    }
}

static void island_apu_init(R01sIsland *island) {
    R01sIslandApuImpl *impl = (R01sIslandApuImpl *)island->impl;
    r01s_atmega328p_init(impl->apu, "U328");
    r01s_island_add_entity(island, r01s_atmega328p_entity(impl->apu));
}

static void island_mcu_lb_init(R01sIsland *island) {
    R01sIslandMcuLbImpl *impl = (R01sIslandMcuLbImpl *)island->impl;
    static const char *const mux_ref[R01S_BOM_HC157_N] = {"U7A", "U7B", "U7C", "U7D", "U7E", "U7F"};
    int i;
    r01s_atmega1284p_init(impl->mcu, "U1284");
    r01s_as6c62256_init(impl->sram, "U41");
    r01s_island_add_entity(island, r01s_atmega1284p_entity(impl->mcu));
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->sram));
    for (i = 3; i < R01S_BOM_HC157_N; i++) {
        r01s_sn74hc157_init(impl->mux157[i], mux_ref[i]);
        r01s_island_add_entity(island, r01s_sn74hc157_entity(impl->mux157[i]));
    }
}

static const R01sIslandVTable ISLAND_VIDEO_VT = {island_video_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_POWER_CLK_VT = {island_power_clk_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CPU_VT = {island_cpu_mem_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_IO_VT = {island_io_latch_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_VRAM_VT = {island_vram_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_BEAM_VT = {island_beam_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CART_VT = {island_cart_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_APU_VT = {island_apu_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_MCU_LB_VT = {island_mcu_lb_init, NULL, NULL, NULL, NULL};


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
    board->cart_len_pal_bg = 0;
    board->cart_off_pal_spr = 0;
    board->cart_len_pal_spr = 0;
    board->cart_default_pal_row = 0;
    board->cart_world_base = 0;
    board->cart_off_sdir = 0;
    board->cart_screen_count = 0;
    board->cart_start_col = 0;
    board->cart_start_row = 0;
    board->cart_bg0_count = 0;
    board->cart_bg0_cols_hdr = 0;
    board->cart_bg0_rows_hdr = 0;
    board->cart_off_bg0_dir = 0;
    board->bg0_count = 0;
    board->bg0_cols = 0;
    board->bg0_rows = 0;
    board->l1_cols = 1;
    board->l1_rows = 1;
    board->l1_origin_x = 0;
    board->l1_origin_y = 0;
    board->l0_cam_x = 0;
    board->l0_cam_y = 0;
    board->cart_entity_type_count = 0;
    board->cart_entity_inst_count = 0;
    board->cart_off_entity_types = 0;
    board->cart_off_entity_insts = 0;
    board->cart_player_entity = 0xFF;
    board->cart_player_hit_x = 0;
    board->cart_player_hit_y = 0;
    board->cart_player_hit_w = 8;
    board->cart_player_hit_h = 8;
    board->cart_format_ver = 0;
    board->cart_off_other = 0;
    board->cart_len_other = 0;
    board->cart_off_other_title = 0;
    board->cart_off_other_inter = 0;
    board->cart_off_credits = 0;
    board->cart_len_credits = 0;
    if (!board || !board->cart_loaded) {
        return;
    }
    img = board->cart_flash.mem;
    if (memcmp(img, "retr01", 6) != 0) {
        return;
    }
    board->cart_format_ver = img[6];
    ptrs = img + R01S_CART_HDR_SIZE;
    board->cart_off_pal_bg = get_u24(ptrs + 6);
    board->cart_len_pal_bg = get_u24(ptrs + 9);
    board->cart_off_pal_spr = get_u24(ptrs + 12);
    board->cart_len_pal_spr = get_u24(ptrs + 15);
    off_wtable = get_u24(ptrs + 18);
    if (board->cart_format_ver != R01S_CART_FORMAT_VER) {
        return;
    }
    board->cart_off_other = get_u24(ptrs + 24);
    board->cart_len_other = get_u24(ptrs + 27);
    /* Legacy ASCII credits reserved -- ignored. */
    board->cart_off_credits = 0;
    board->cart_len_credits = 0;
    (void)get_u24(ptrs + 30);
    (void)get_u24(ptrs + 33);
    board->cart_off_other_title =
        cart_other_payload_abs(img, sizeof(board->cart_flash.mem), board->cart_off_other, board->cart_len_other, 0);
    board->cart_off_other_inter =
        cart_other_payload_abs(img, sizeof(board->cart_flash.mem), board->cart_off_other, board->cart_len_other, 1);
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
    board->cart_default_pal_row = (uint8_t)(hdr[4] & 7u);
    screen_count = hdr[5];
    if (screen_count > R01S_MAX_PRESENT_SCREENS) {
        return;
    }
    off_chr = get_u24(hdr + 8);
    off_sdir = get_u24(hdr + 11);
    board->cart_off_chr = world_base + off_chr;
    board->cart_world_base = world_base;
    board->cart_start_col = start_col;
    board->cart_start_row = start_row;
    board->cart_screen_count = screen_count;
    board->cart_bg0_count = hdr[6];
    board->cart_bg0_cols_hdr = (uint8_t)(hdr[3] & 0x0Fu);
    board->cart_bg0_rows_hdr = (uint8_t)((hdr[3] >> 4) & 0x0Fu);
    {
        uint32_t off_bg0 = get_u24(hdr + 14);
        board->cart_off_bg0_dir = off_bg0 ? (world_base + off_bg0) : 0;
    }
    if ((size_t)world_base + (size_t)off_sdir + (size_t)screen_count * 12u > sizeof(board->cart_flash.mem)) {
        board->cart_off_chr = 0;
        board->cart_world_base = 0;
        board->cart_screen_count = 0;
        board->cart_bg0_count = 0;
        board->cart_off_bg0_dir = 0;
        return;
    }
    board->cart_off_sdir = world_base + off_sdir;
    board->cart_entity_type_count = hdr[17];
    board->cart_entity_inst_count = hdr[18];
    {
        uint32_t off_types = get_u24(hdr + 19);
        uint32_t off_insts = get_u24(hdr + 22);
        board->cart_off_entity_types = world_base + off_types;
        board->cart_off_entity_insts = world_base + off_insts;
    }
    board->cart_player_entity = hdr[25];
    board->cart_player_hit_x = hdr[26];
    board->cart_player_hit_y = hdr[27];
    board->cart_player_hit_w = hdr[28] ? hdr[28] : 8;
    board->cart_player_hit_h = hdr[29] ? hdr[29] : 8;
    board->cart_world_flags = hdr[7];
    board->cart_cam_deadzone_x = hdr[30];
    board->cart_cam_deadzone_y = hdr[31];
    board->cart_off_player_anim = 0;
    if ((board->cart_world_flags & 0x01u) != 0 && board->cart_player_entity != 0xFF &&
        board->cart_off_entity_insts != 0) {
        board->cart_off_player_anim =
            board->cart_off_entity_insts + (uint32_t)board->cart_entity_inst_count * 6u;
    }
    dir = img + board->cart_off_sdir;
    for (si = 0; si < (int)screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        uint32_t poff;
        if (e[0] != start_col || e[1] != start_row) {
            continue;
        }
        poff = get_u24(e + 4);
        board->cart_off_map_screen0 = world_base + poff;
        break;
    }
    /* Host Play L0 cache (does not touch IC VRAM slots 4-7). */
    r01s_board_load_bg0(board);
}

static void board_install_bringup_prg(R01sBoard *board) {
    uint32_t base;
    uint32_t i;
    uint8_t buf[512];
    size_t n = 0;
    uint16_t hang_pc;
    int stream = 0;

    if (!board || !board->cart_loaded) {
        return;
    }
    board_resolve_cart_meta(board);
    /* IC path: palette + 480 B MAP->VRAM via $FE93->$FE12 (replaces host softboot). */
    stream = (board->cart_off_map_screen0 != 0 && board->cart_off_pal_bg != 0);

    memcpy(buf + n, R01S_BRINGUP_SMOKE, sizeof(R01S_BRINGUP_SMOKE));
    n += sizeof(R01S_BRINGUP_SMOKE);
    if (stream) {
        uint32_t off_bg =
            board_pal_row_off(board->cart_off_pal_bg, board->cart_len_pal_bg, board->cart_default_pal_row);
        uint32_t off_spr =
            board->cart_off_pal_spr
                ? board_pal_row_off(board->cart_off_pal_spr, board->cart_len_pal_spr, board->cart_default_pal_row)
                : off_bg + R01S_PAL_ROW_BYTES;
        memcpy(buf + n, R01S_BRINGUP_STREAM, sizeof(R01S_BRINGUP_STREAM));
        buf[n + R01S_BR_OFF_BG_LO] = (uint8_t)(off_bg & 0xFFu);
        buf[n + R01S_BR_OFF_BG_MID] = (uint8_t)((off_bg >> 8) & 0xFFu);
        buf[n + R01S_BR_OFF_BG_HI] = (uint8_t)((off_bg >> 16) & 0xFFu);
        buf[n + R01S_BR_OFF_SPR_LO] = (uint8_t)(off_spr & 0xFFu);
        buf[n + R01S_BR_OFF_SPR_MID] = (uint8_t)((off_spr >> 8) & 0xFFu);
        buf[n + R01S_BR_OFF_SPR_HI] = (uint8_t)((off_spr >> 16) & 0xFFu);
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
    uint8_t ptrs[R01S_CART_PTR_TABLE_BYTES];
    uint32_t off_pal_bg = R01S_CART_HDR_SIZE + R01S_CART_PTR_TABLE_BYTES;
    uint32_t off_pal_spr = off_pal_bg + R01S_PAL_PLANE_BYTES;
    uint32_t off_prg = off_pal_spr + R01S_PAL_PLANE_BYTES;
    uint32_t off_wtable = off_prg + R01S_CART_PRG_BYTES;
    uint8_t pals[R01S_PAL_PLANE_BYTES * 2u];
    unsigned i;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "retr01", 6);
    hdr[6] = R01S_CART_FORMAT_VER;
    hdr[7] = 1;
    memset(ptrs, 0, sizeof(ptrs));
    put_u24(ptrs + 0, off_prg);
    put_u24(ptrs + 3, R01S_CART_PRG_BYTES);
    put_u24(ptrs + 6, off_pal_bg);
    put_u24(ptrs + 9, R01S_PAL_PLANE_BYTES);
    put_u24(ptrs + 12, off_pal_spr);
    put_u24(ptrs + 15, R01S_PAL_PLANE_BYTES);
    put_u24(ptrs + 18, off_wtable);
    put_u24(ptrs + 21, 64);
    /* off_other / len_other / off_credits / len_credits remain 0 */
    memset(pals, 0, sizeof(pals));
    /* Row 0 BG: kit strips; row 0 SPR: color1 = bright-ish red index 34. */
    for (i = 0; i < 4u; i++) {
        pals[i * 4u + 0] = 0;
        pals[i * 4u + 1] = (uint8_t)(16u + i);
        pals[i * 4u + 2] = (uint8_t)(32u + i);
        pals[i * 4u + 3] = (uint8_t)(48u + i);
        pals[R01S_PAL_PLANE_BYTES + i * 4u + 0] = 0;
        pals[R01S_PAL_PLANE_BYTES + i * 4u + 1] = 34;
        pals[R01S_PAL_PLANE_BYTES + i * 4u + 2] = 34;
        pals[R01S_PAL_PLANE_BYTES + i * 4u + 3] = 34;
    }

    memset(board->cart_flash.mem, 0xFF, sizeof(board->cart_flash.mem));
    r01s_sst39sf040_load(&board->cart_flash, 0, hdr, R01S_CART_HDR_SIZE);
    r01s_sst39sf040_load(&board->cart_flash, R01S_CART_HDR_SIZE, ptrs, R01S_CART_PTR_TABLE_BYTES);
    r01s_sst39sf040_load(&board->cart_flash, off_pal_bg, pals, (uint32_t)sizeof(pals));
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
    uint8_t format_ver;
    if (!board || !img || len < R01S_CART_HDR_SIZE + R01S_CART_PTR_TABLE_BYTES) {
        return -1;
    }
    if (memcmp(img, "retr01", 6) != 0) {
        /* Raw flash dump: treat whole image as mapped from 0; PRG at conventional off after hdr+ptrs+pals. */
        if (len > R01S_FLASH_BYTES) {
            len = R01S_FLASH_BYTES;
        }
        memset(board->cart_flash.mem, 0xFF, sizeof(board->cart_flash.mem));
        r01s_sst39sf040_load(&board->cart_flash, 0, img, (uint32_t)len);
        board->cart_off_prg = R01S_CART_HDR_SIZE + R01S_CART_PTR_TABLE_BYTES + 2u * 128u;
        board->cart_len_prg = R01S_CART_PRG_BYTES;
        board->cart_loaded = 1;
        return 0;
    }
    format_ver = img[6];
    if (format_ver != R01S_CART_FORMAT_VER) {
        return -1;
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
    board_resolve_cart_meta(board);
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
    /* Cart PRG in flash is executed as-is (Studio export includes MAP boot). */
    return 0;
}

int r01s_board_has_screen(const R01sBoard *board, int col, int row) {
    const uint8_t *dir;
    int si;

    if (!board || board->cart_off_sdir == 0 || board->cart_screen_count == 0) {
        return 0;
    }
    if (col < 0 || row < 0 || col > 255 || row > 255) {
        return 0;
    }
    dir = board->cart_flash.mem + board->cart_off_sdir;
    for (si = 0; si < (int)board->cart_screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        if (e[0] == (uint8_t)col && e[1] == (uint8_t)row) {
            return 1;
        }
    }
    return 0;
}

int r01s_board_first_screen(const R01sBoard *board, int *out_col, int *out_row) {
    const uint8_t *dir;
    const uint8_t *e;

    if (!board || board->cart_off_sdir == 0 || board->cart_screen_count == 0) {
        return 0;
    }
    dir = board->cart_flash.mem + board->cart_off_sdir;
    e = dir;
    if (out_col) {
        *out_col = (int)e[0];
    }
    if (out_row) {
        *out_row = (int)e[1];
    }
    return 1;
}

static uint32_t board_map_off_for_screen(const R01sBoard *board, int col, int row) {
    const uint8_t *dir;
    int si;

    if (!board || board->cart_off_sdir == 0) {
        return 0;
    }
    dir = board->cart_flash.mem + board->cart_off_sdir;
    for (si = 0; si < (int)board->cart_screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        if (e[0] == (uint8_t)col && e[1] == (uint8_t)row) {
            return board->cart_world_base + get_u24(e + 4);
        }
    }
    return 0;
}

int r01s_board_attr_at(const R01sBoard *board, int wx, int wy, uint8_t *out_attr) {
    int col, row, lx, ly, tx, ty, cell;
    uint32_t pay;

    if (!board || wx < 0 || wy < 0) {
        return -1;
    }
    col = wx / R01S_BG_SCREEN_PX_W;
    row = wy / R01S_BG_SCREEN_PX_H;
    pay = board_map_off_for_screen(board, col, row);
    if (pay == 0) {
        return -1;
    }
    lx = wx % R01S_BG_SCREEN_PX_W;
    ly = wy % R01S_BG_SCREEN_PX_H;
    tx = lx / 8;
    ty = ly / 8;
    cell = ty * 16 + tx;
    if (pay + 240u + (uint32_t)cell >= sizeof(board->cart_flash.mem)) {
        return -1;
    }
    if (out_attr) {
        *out_attr = r01s_sst39sf040_peek(&board->cart_flash, pay + 240u + (uint32_t)cell);
    }
    return 0;
}

int r01s_board_solid_at(const R01sBoard *board, int wx, int wy) {
    uint8_t attr;
    if (r01s_board_attr_at(board, wx, wy, &attr) != 0) {
        return 0;
    }
    return (attr & R01S_ATTR_SOLID) != 0;
}

int r01s_board_aabb_ok(const R01sBoard *board, int px, int py, int bw, int bh) {
    int x1, y1, c0, c1, r0, r1, col, row;

    if (!board || px < 0 || py < 0 || bw < 1 || bh < 1) {
        return 0;
    }
    x1 = px + bw - 1;
    y1 = py + bh - 1;
    c0 = px / R01S_BG_SCREEN_PX_W;
    c1 = x1 / R01S_BG_SCREEN_PX_W;
    r0 = py / R01S_BG_SCREEN_PX_H;
    r1 = y1 / R01S_BG_SCREEN_PX_H;
    for (col = c0; col <= c1; col++) {
        for (row = r0; row <= r1; row++) {
            if (!r01s_board_has_screen(board, col, row)) {
                return 0;
            }
        }
    }
    if (r01s_board_solid_at(board, px, py) || r01s_board_solid_at(board, x1, py) ||
        r01s_board_solid_at(board, px, y1) || r01s_board_solid_at(board, x1, y1)) {
        return 0;
    }
    return 1;
}

int r01s_board_player_aabb_ok(const R01sBoard *board, int px, int py) {
    return r01s_board_aabb_ok(board, px, py, R01S_PLAY_PLAYER_W, R01S_PLAY_PLAYER_H);
}

static void board_load_screen_slot(R01sBoard *board, int col, int row, int slot) {
    uint32_t map_off;
    uint16_t base;
    uint32_t i;

    if (!board || slot < 0 || slot > 3) {
        return;
    }
    base = (uint16_t)(slot * R01S_BG_SLOT_BYTES);
    map_off = board_map_off_for_screen(board, col, row);
    if (map_off == 0) {
        for (i = 0; i < 480u; i++) {
            r01s_as6c62256_poke(&board->vram, (uint16_t)(base + i), 0);
        }
        board->vram_slot_present[slot] = 0;
        return;
    }
    for (i = 0; i < 480u; i++) {
        uint8_t b = r01s_sst39sf040_peek(&board->cart_flash, map_off + i);
        r01s_as6c62256_poke(&board->vram, (uint16_t)(base + i), b);
    }
    board->vram_slot_present[slot] = 1;
}

int r01s_board_load_camera_2x2(R01sBoard *board, int origin_col, int origin_row) {
    int dx, dy;

    if (!board || !board->cart_loaded || board->cart_off_sdir == 0) {
        return -1;
    }
    for (dy = 0; dy < 2; dy++) {
        for (dx = 0; dx < 2; dx++) {
            board_load_screen_slot(board, origin_col + dx, origin_row + dy, dy * 2 + dx);
        }
    }
    return 0;
}

void r01s_board_set_scroll(R01sBoard *board, uint8_t scroll_x, uint8_t scroll_y) {
    if (!board) {
        return;
    }
    if (scroll_x > 127) {
        scroll_x = 127;
    }
    if (scroll_y > 119) {
        scroll_y = 119;
    }
    r01s_sn74hc573_poke_q(board->io_latch_impl.latch573[R01S_LATCH_FE02], scroll_x);
    r01s_sn74hc573_poke_q(board->io_latch_impl.latch573[R01S_LATCH_FE03], scroll_y);
    board->health_saw_latch = 1;
}

void r01s_board_mark_map_ready(R01sBoard *board) {
    if (!board || board->cart_off_map_screen0 == 0) {
        return;
    }
    poke_map_addr_latches(board, board->cart_off_map_screen0 + 480u);
}

void r01s_board_update_bg0_scroll(R01sBoard *board, int cam_x, int cam_y) {
    int rel_x;
    int rel_y;

    if (!board) {
        return;
    }
    /* Match emu: relative to L1 present bbox origin, scaled by present grid W/H. */
    rel_x = cam_x - board->l1_origin_x;
    rel_y = cam_y - board->l1_origin_y;
    if (rel_x < 0) {
        rel_x = 0;
    }
    if (rel_y < 0) {
        rel_y = 0;
    }
    if (board->bg0_cols < 2 || board->l1_cols < 1 || board->bg0_cols >= board->l1_cols) {
        board->l0_cam_x = 0;
    } else {
        board->l0_cam_x = (rel_x * board->bg0_cols) / board->l1_cols;
    }
    if (board->bg0_rows < 2 || board->l1_rows < 1 || board->bg0_rows >= board->l1_rows) {
        board->l0_cam_y = 0;
    } else {
        board->l0_cam_y = (rel_y * board->bg0_rows) / board->l1_rows;
    }
}

void r01s_board_load_bg0(R01sBoard *board) {
    const uint8_t *img;
    const uint8_t *dir;
    int si;
    int n;
    int min_c = 99, min_r = 99, max_c = 0, max_r = 0;
    int l1_min_c = 99, l1_min_r = 99, l1_max_c = 0, l1_max_r = 0;

    if (!board) {
        return;
    }
    memset(board->bg0, 0, sizeof(board->bg0));
    board->bg0_count = 0;
    board->bg0_cols = 0;
    board->bg0_rows = 0;
    board->l0_cam_x = 0;
    board->l0_cam_y = 0;
    board->l1_cols = 1;
    board->l1_rows = 1;
    board->l1_origin_x = 0;
    board->l1_origin_y = 0;
    if (!board->cart_loaded || board->cart_world_base == 0) {
        return;
    }
    img = board->cart_flash.mem;

    /* L1 present bbox (same rule as emu prepare_world). */
    if (board->cart_off_sdir != 0 && board->cart_screen_count > 0 &&
        (size_t)board->cart_off_sdir + (size_t)board->cart_screen_count * 12u <=
            sizeof(board->cart_flash.mem)) {
        dir = img + board->cart_off_sdir;
        for (si = 0; si < (int)board->cart_screen_count; si++) {
            const uint8_t *e = dir + (size_t)si * 12u;
            int c = (int)e[0];
            int r = (int)e[1];
            if (c < l1_min_c) {
                l1_min_c = c;
            }
            if (r < l1_min_r) {
                l1_min_r = r;
            }
            if (c > l1_max_c) {
                l1_max_c = c;
            }
            if (r > l1_max_r) {
                l1_max_r = r;
            }
        }
        if (l1_min_c <= l1_max_c) {
            board->l1_cols = l1_max_c - l1_min_c + 1;
            board->l1_rows = l1_max_r - l1_min_r + 1;
            if (board->l1_cols < 1) {
                board->l1_cols = 1;
            }
            if (board->l1_rows < 1) {
                board->l1_rows = 1;
            }
            board->l1_origin_x = l1_min_c * R01S_BG_SCREEN_PX_W;
            board->l1_origin_y = l1_min_r * R01S_BG_SCREEN_PX_H;
        }
    }

    if (board->cart_bg0_count == 0 || board->cart_off_bg0_dir == 0) {
        return;
    }
    n = (int)board->cart_bg0_count;
    if (n > R01S_BG0_SCREENS_MAX) {
        n = R01S_BG0_SCREENS_MAX;
    }
    if ((size_t)board->cart_off_bg0_dir + (size_t)n * 12u > sizeof(board->cart_flash.mem)) {
        return;
    }
    dir = img + board->cart_off_bg0_dir;
    for (si = 0; si < n; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        uint32_t poff = get_u24(e + 4);
        uint32_t abs = board->cart_world_base + poff;
        int c = (int)e[0];
        int r = (int)e[1];
        if ((size_t)abs + R01S_CART_SCREEN_PAYLOAD > sizeof(board->cart_flash.mem)) {
            continue;
        }
        board->bg0[board->bg0_count].present = 1;
        board->bg0[board->bg0_count].col = (uint8_t)c;
        board->bg0[board->bg0_count].row = (uint8_t)r;
        memcpy(board->bg0[board->bg0_count].map, img + abs, R01S_CART_SCREEN_PAYLOAD);
        if (c < min_c) {
            min_c = c;
        }
        if (r < min_r) {
            min_r = r;
        }
        if (c > max_c) {
            max_c = c;
        }
        if (r > max_r) {
            max_r = r;
        }
        board->bg0_count++;
    }
    if (board->bg0_count > 0) {
        board->bg0_cols = max_c - min_c + 1;
        board->bg0_rows = max_r - min_r + 1;
        if (board->bg0_cols < 1) {
            board->bg0_cols = 1;
        }
        if (board->bg0_rows < 1) {
            board->bg0_rows = 1;
        }
    }
}

int r01s_board_softboot_start_screen(R01sBoard *board) {
    uint32_t i;

    /* Opt-in debug only (R01S_SOFTBOOT=1). Default path is IC MAP stream. */
    if (!board || !board->cart_loaded) {
        return -1;
    }
    if (board->cart_off_map_screen0 == 0 || board->cart_off_pal_bg == 0) {
        return -1;
    }
    for (i = 0; i < 480u; i++) {
        uint8_t b = r01s_sst39sf040_peek(&board->cart_flash, board->cart_off_map_screen0 + i);
        r01s_as6c62256_poke(&board->vram, (uint16_t)i, b);
    }
    board->vram_slot_present[0] = 1;
    board->vram_slot_present[1] = 0;
    board->vram_slot_present[2] = 0;
    board->vram_slot_present[3] = 0;
    board_apply_active_pals_from_cart(board);
    poke_map_addr_latches(board, board->cart_off_map_screen0 + 480u);
    if (board->cart_off_sdir != 0) {
        (void)r01s_board_load_camera_2x2(board, (int)board->cart_start_col, (int)board->cart_start_row);
    }
    return 0;
}

int r01s_board_catchup_bringup(R01sBoard *board, R01sIslandGroup *group) {
    int i;
    const char *want_soft;
    uint32_t target;
    uint8_t expect0;

    if (!board || !board->cart_loaded || board->cart_off_map_screen0 == 0) {
        return 0;
    }

    want_soft = getenv("R01S_SOFTBOOT");
    if (want_soft && want_soft[0] != '\0' && strcmp(want_soft, "0") != 0) {
        if (r01s_board_softboot_start_screen(board) != 0) {
            return -1;
        }
        if (!group) {
            return 0;
        }
        for (i = 0; i < 12000; i++) {
            r01s_island_group_step(group);
            if (r01s_w65c02s_pc(board->cpu_mem_impl.cpu) >= 0x8078u) {
                break;
            }
        }
        return r01s_board_softboot_start_screen(board);
    }

    /* Default: run bring-up PRG MAP stream on the pin-level netlist (~12k steps). */
    target = board->cart_off_map_screen0 + 480u;
    expect0 = r01s_sst39sf040_peek(&board->cart_flash, board->cart_off_map_screen0);
    board->catchup_cancel = 0;
    if (!group) {
        return -1;
    }
    for (i = 0; i < 80000; i++) {
        if (board->catchup_cancel) {
            return -1;
        }
        r01s_island_group_step(group);
        if (board->map_addr >= target && r01s_as6c62256_peek(&board->vram, 0) == expect0) {
            /* Smoke LDA $FE12 leaves A==$AA for only a few cycles; pin catchup
             * often misses it. MAP-complete + matching VRAM[0] proves the port. */
            board->health_saw_vram = 1;
            board->health_saw_vram_read = 1;
            board->health_saw_map = 1;
            r01s_board_catchup_finish(board);
            return 0;
        }
    }
    return -1;
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
    ctx->flash_ce_owner = R01S_FLASH_CE_NONE;
    ctx->pal_addr = 0;
    ctx->pal_fe09_wrote = 0;
    memset(ctx->active_pal, 0, sizeof(ctx->active_pal));
    ctx->chr_last_master = 0;
    r01s_play_reset(&ctx->play);
    ctx->catchup_cancel = 0;
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
    ctx->vblank_prev = 0;
    ctx->linebuf_saw_mux_mcu = 0;
    ctx->linebuf_saw_mux_beam = 0;
    ctx->health_phi2_edges = 0;
    r01s_bus_clear_conflicts();
    r01s_entity_reset(r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu));
    r01s_entity_reset(r01s_osc8m_entity(ctx->power_clk_impl.osc));
    {
        int li;
        for (li = 0; li < R01S_BOM_HC573_N; li++) {
            r01s_entity_reset(r01s_sn74hc573_entity(ctx->io_latch_impl.latch573[li]));
        }
        poke_vram_addr_latches(ctx, 0);
        poke_map_addr_latches(ctx, 0);
        poke_pal_addr_latch(ctx, 0);
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
    r01s_entity_reset(r01s_atmega1284p_entity(ctx->mcu_lb_impl.mcu));
    r01s_entity_reset(r01s_as6c62256_entity(ctx->mcu_lb_impl.sram));
    r01s_entity_reset(r01s_sn74hc157_entity(ctx->mcu_lb_impl.mux157[R01S_MUX157_LINEBUF0]));
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
    int beam_dots;
    if (!ctx || !group->powered) {
        return;
    }
    osc = r01s_osc8m_entity(ctx->power_clk_impl.osc);
    cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    beam_dots = R01S_BEAM_DOTS_PER_STEP;
    if (ctx->play.enabled) {
        beam_dots = R01S_BEAM_DOTS_PER_STEP_PLAY;
    }

    board_settle(ctx, group);
    r01s_entity_tick(osc);
    r01s_entity_tick(r01s_atmega328p_entity(ctx->apu_impl.apu));
    r01s_entity_tick(r01s_atmega1284p_entity(ctx->mcu_lb_impl.mcu));
    board_settle(ctx, group);
    /* Beam/DOT domain: burst so interactive UI reaches VBlank without minutes of wait. */
    {
        R01sEntity *dot_osc = r01s_osc_dot_entity(ctx->beam_impl.osc_dot);
        R01sEntity *beam = r01s_beam_xy_entity(ctx->beam_impl.beam_x);
        R01sEntity *cpu_e = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
        int di;
        /* DOT window: yield flash so CHR can own /CE (CPU not advancing). */
        flash_yield_for_chr(ctx);
        for (di = 0; di < beam_dots; di++) {
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
            wire_linebuf(ctx);
            wire_video_dot(ctx);
            {
                int vb = r01s_beam_xy_vblank(ctx->beam_impl.beam_x);
                if (vb && !ctx->vblank_prev) {
                    if (ctx->video_impl.sink) {
                        r01s_video_sink_on_vblank(ctx->video_impl.sink);
                    }
                    r01s_play_on_vblank(ctx);
                }
                ctx->vblank_prev = (uint8_t)(vb ? 1 : 0);
            }
        }
        flash_chr_release(ctx);
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
                    poke_vram_addr_latches(ctx, (uint16_t)((ctx->vram_addr + 1u) & 0x7FFFu));
                    ctx->vram_fe12_armed = 0;
                }
                if (ctx->map_fe93_armed) {
                    poke_map_addr_latches(ctx, (ctx->map_addr + 1u) & 0xFFFFFFu);
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
    pwr = r01s_pwr5v_entity(ctx->power_clk_impl.pwr);
    osc = r01s_osc8m_entity(ctx->power_clk_impl.osc);
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
        *probe_vdd = r01s_level_is_high(r01s_entity_sense(r01s_pwr5v_entity(ctx->power_clk_impl.pwr), "VDD"));
    }
    if (probe_phi2) {
        *probe_phi2 = r01s_level_is_high(r01s_entity_sense(r01s_osc8m_entity(ctx->power_clk_impl.osc), "PHI2"));
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

    board->power_clk_impl.pwr = &board->pwr;
    board->power_clk_impl.osc = &board->osc;
    board->power_clk_impl.hc14 = &board->hc14;
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
    board->mcu_lb_impl.mcu = &board->mcu1284;
    board->mcu_lb_impl.sram = &board->linebuf;
    {
        int i;
        for (i = 0; i < R01S_BOM_HC157_N; i++) {
            board->mcu_lb_impl.mux157[i] = &board->mux157[i];
        }
    }
    {
        int i;
        for (i = 0; i < R01S_BOM_HC245_N; i++) {
            board->bus_impl.bus245[i] = &board->bus245[i];
        }
    }
    board->cpu_mem_impl.bus245_cpu = &board->bus245[R01S_BUS245_CPU];
    board->video_impl.bus245_video = &board->bus245[R01S_BUS245_VIDEO];
    board->cart_impl.bus245_cart = &board->bus245[R01S_BUS245_CART_OAM];
    board->sprites_impl.fetch = &board->sprite_fetch;
    board->integration_impl.integ = &board->integration;

    r01s_pads_init(&board->pads, "PAD");
    r01s_sprite_fetch_init(&board->sprite_fetch, "UPLDN");
    r01s_integration_init(&board->integration, "UPLDP");
    r01s_bg_fetch_init(&board->bg_fetch, "UPLDI");

    /* Add order = canvas Z / arrange_rows order: VIDEO (LCD) first -> top-left. */
    if (r01s_island_builder_add(b, &ISLAND_VIDEO_VT, "ISLAND O  VIDEO RGBS", 0, 0, 1, 1,
                                &board->video_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_POWER_CLK_VT, "ISLAND A  POWER+CLK", 0, 0, 1, 1,
                                &board->power_clk_impl) < 0) {
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
    if (r01s_island_builder_add(b, &ISLAND_VRAM_VT, "ISLAND G  VRAM+PLD", 0, 0, 1, 1, &board->vram_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_BEAM_VT, "ISLAND H  BEAM NMI", 0, 0, 1, 1, &board->beam_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_CART_VT, "ISLAND J  CART FLASH", 0, 0, 1, 1,
                                &board->cart_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_APU_VT, "ISLAND K  APU 328P", 0, 0, 1, 1, &board->apu_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_MCU_LB_VT, "ISLAND L  1284+LINEBUF", 0, 0, 1, 1,
                                &board->mcu_lb_impl) < 0) {
        return -1;
    }

    {
        R01sEntity *comp_e = r01s_compositor_entity(&board->compositor);
        R01sEntity *prom_e = r01s_at28c16_entity(&board->color_prom);
        R01sEntity *sink_e = r01s_video_sink_entity(&board->video_sink);
        R01sEntity *buf_e = r01s_sn74hc245_entity(&board->bus245[R01S_BUS245_VIDEO]);
        int x = 0;
        r01s_island_builder_mount_rel(b, comp_e, R01S_ISLAND_VIDEO, 0, 0);
        x += comp_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, prom_e, R01S_ISLAND_VIDEO, x, 0);
        x += prom_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, sink_e, R01S_ISLAND_VIDEO, x, 0);
        x += sink_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, buf_e, R01S_ISLAND_VIDEO, x, 0);
    }
    {
        R01sEntity *pwr_e = r01s_pwr5v_entity(&board->pwr);
        R01sEntity *osc_e = r01s_osc8m_entity(&board->osc);
        r01s_island_builder_mount_rel(b, pwr_e, R01S_ISLAND_POWER_CLK, 0, 0);
        r01s_island_builder_mount_rel(b, osc_e, R01S_ISLAND_POWER_CLK, pwr_e->body_w + R01S_CHIP_GAP, 0);
    }
    {
        R01sEntity *cpu_e = r01s_w65c02s_entity(&board->cpu);
        R01sEntity *ram_e = r01s_as6c62256_entity(&board->ram);
        int x = 0;
        r01s_island_builder_mount_rel(b, cpu_e, R01S_ISLAND_CPU, 0, 0);
        x = cpu_e->body_w + R01S_CHIP_GAP;
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
        int i;
        r01s_island_builder_mount_rel(b, vram_e, R01S_ISLAND_VRAM, 0, 0);
        x = vram_e->body_w + R01S_CHIP_GAP;
        for (i = 0; i < 3; i++) {
            R01sEntity *mux_e = r01s_sn74hc157_entity(&board->mux157[i]);
            r01s_island_builder_mount_rel(b, mux_e, R01S_ISLAND_VRAM, x, 0);
            x += mux_e->body_w + R01S_CHIP_GAP;
        }
        r01s_island_builder_mount_rel(b, r01s_atf22v10_entity(&board->pld_vram), R01S_ISLAND_VRAM, x, 0);
    }
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
        R01sEntity *flash_e = r01s_sst39sf040_entity(&board->cart_flash);
        R01sEntity *ee_e = r01s_i2c_eeprom_entity(&board->cart_eeprom);
        R01sEntity *buf_e = r01s_sn74hc245_entity(&board->bus245[R01S_BUS245_CART_OAM]);
        int x = 0;
        r01s_island_builder_mount_rel(b, flash_e, R01S_ISLAND_CART, 0, 0);
        x = flash_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, ee_e, R01S_ISLAND_CART, x, 0);
        x += ee_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, buf_e, R01S_ISLAND_CART, x, 0);
    }
    r01s_island_builder_mount_rel(b, r01s_atmega328p_entity(&board->apu), R01S_ISLAND_APU, 0, 0);
    {
        R01sEntity *mcu_e = r01s_atmega1284p_entity(&board->mcu1284);
        R01sEntity *lb_e = r01s_as6c62256_entity(&board->linebuf);
        int x;
        int i;
        r01s_island_builder_mount_rel(b, mcu_e, R01S_ISLAND_MCU_LB, 0, 0);
        x = mcu_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, lb_e, R01S_ISLAND_MCU_LB, x, 0);
        x += lb_e->body_w + R01S_CHIP_GAP;
        for (i = 3; i < R01S_BOM_HC157_N; i++) {
            R01sEntity *mux_e = r01s_sn74hc157_entity(&board->mux157[i]);
            int mux_y = (lb_e->body_h - mux_e->body_h) / 2;
            if (mux_y < 0) {
                mux_y = 0;
            }
            r01s_island_builder_mount_rel(b, mux_e, R01S_ISLAND_MCU_LB, x, mux_y);
            x += mux_e->body_w + R01S_CHIP_GAP;
        }
    }

    r01s_island_builder_fit_all(b);
    r01s_island_builder_arrange_rows(b, 40, 40, R01S_ISLAND_GAP, R01S_ISLAND_GAP, R01S_ISLAND_ROW_MAX_W);

    {
        int bom_ic = r01s_island_builder_count_visual(b, R01S_ENTITY_VIS_IC);
        if (bom_ic != R01S_BOM_IC_N) {
            fprintf(stderr, "board: expected %d BOM IC mounts, got %d\n", R01S_BOM_IC_N, bom_ic);
            return -1;
        }
    }

    if (r01s_island_builder_finish(b) != 0) {
        return -1;
    }
    /* After island init (flash memset): install bring-up cart image. */
    board_install_synthetic_cart(board);
    return 0;
}
