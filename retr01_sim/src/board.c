#include "retr01_sim/board.h"

#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/health.h"

#include <stdio.h>
#include <string.h>

#define R01S_RESET_HOLD 12

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
    if (r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch) == 0x55) {
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
    if (r01s_beam_xy_hblank(ctx->beam_impl.beam) || r01s_beam_xy_y(ctx->beam_impl.beam) > 0) {
        ctx->health_saw_beam = 1;
    }
    if (r01s_bg_fetch_count(ctx->bg_fetch_impl.fetch) > 0 &&
        r01s_bg_fetch_last_tile(ctx->bg_fetch_impl.fetch) == 0x42) {
        ctx->health_saw_bg_fetch = 1;
    }
}

static int board_integrated(const R01sBoard *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->health_saw_latch && ctx->health_saw_vram && ctx->health_saw_vram_read && ctx->health_saw_pad &&
           ctx->health_saw_beam && ctx->health_saw_bg_fetch;
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
        uint8_t le = r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch);
        uint8_t ry = r01s_sn74hc573_peek_q(ctx->io_latch_impl.raster);
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
        int bx = r01s_beam_xy_x(ctx->beam_impl.beam);
        int by = r01s_beam_xy_y(ctx->beam_impl.beam);
        int hb = r01s_beam_xy_hblank(ctx->beam_impl.beam);
        int vb = r01s_beam_xy_vblank(ctx->beam_impl.beam);
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
                 r01s_level_name(r01s_entity_sense(r01s_sn74hc688_entity(ctx->beam_impl.cmp), "EQ#")),
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.raster));
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
            snprintf(ih->activity, sizeof(ih->activity), "fetching tile=$%02X",
                     r01s_bg_fetch_last_tile(bg));
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
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch),
                 r01s_sn74hc573_peek_q(ctx->io_latch_impl.scroll_y));
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
                 "boot latch=%s vram=%s pads=%s beam=%s bg=%s",
                 ctx->health_saw_latch ? "ok" : "wait", ctx->health_saw_vram_read ? "ok" : "wait",
                 ctx->health_saw_pad ? "ok" : "wait", ctx->health_saw_beam ? "ok" : "wait",
                 ctx->health_saw_bg_fetch ? "ok" : "wait");
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
                         "conflicts=%u milestones latch=%d vram_w=%d vram_r=%d pad=%d beam=%d bg=%d\n",
                         r01s_health_tag(out->system), out->system_label, out->system_detail,
                         group->running, group->powered, (unsigned)ctx->cycles, conflicts,
                         ctx->health_saw_latch, ctx->health_saw_vram, ctx->health_saw_vram_read,
                         ctx->health_saw_pad, ctx->health_saw_beam, ctx->health_saw_bg_fetch);
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

static void copy_bus_named(R01sEntity *dst, const char *dst_prefix, R01sEntity *src, const char *src_prefix,
                           int width) {
    int i;
    char dn[16], sn[16];
    for (i = 0; i < width; i++) {
        snprintf(dn, sizeof(dn), "%s%d", dst_prefix, i);
        snprintf(sn, sizeof(sn), "%s%d", src_prefix, i);
        r01s_entity_drive(dst, dn, r01s_entity_sense(src, sn));
    }
}

static void copy_cpu_d_to_latch_d(R01sEntity *latch, R01sEntity *cpu) {
    int i;
    char ln[8], cn[8];
    for (i = 0; i < 8; i++) {
        snprintf(ln, sizeof(ln), "%dD", i + 1);
        snprintf(cn, sizeof(cn), "D%d", i);
        r01s_entity_drive(latch, ln, r01s_entity_sense(cpu, cn));
    }
}

static void copy_latch_q_to_cpu_d(R01sEntity *cpu, R01sEntity *latch) {
    int i;
    char ln[8], cn[8];
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

/* Soft PLD: $FE02 / $FE03 / $FE04 latches + $FE60/$FE61 pads. Deselect RAM/PRG in $FExx. */
static void wire_io(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *latch = r01s_sn74hc573_entity(ctx->io_latch_impl.latch);
    R01sEntity *scroll_y = r01s_sn74hc573_entity(ctx->io_latch_impl.scroll_y);
    R01sEntity *raster = r01s_sn74hc573_entity(ctx->io_latch_impl.raster);
    R01sEntity *pads = r01s_pads_entity(ctx->pads_impl.pads);
    uint16_t addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int be = r01s_level_is_high(r01s_entity_sense(cpu, "BE"));
    int hit_latch = (addr == 0xFE02u);
    int hit_scroll_y = (addr == 0xFE03u);
    int hit_raster = (addr == 0xFE04u);
    int hit_pads = (addr == 0xFE60u || addr == 0xFE61u);

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

    if (!be || !addr_is_io(addr)) {
        r01s_entity_eval(latch);
        r01s_entity_eval(scroll_y);
        r01s_entity_eval(raster);
        r01s_entity_eval(pads);
        return;
    }

    if (hit_pads) {
        r01s_entity_drive(pads, "A0", (addr == 0xFE61u) ? R01S_LVL_H : R01S_LVL_L);
    }

    if (hit_latch) {
        copy_cpu_d_to_latch_d(latch, cpu);
        if (!read) {
            r01s_entity_drive(latch, "LE", R01S_LVL_H);
        }
        r01s_entity_eval(latch);
        if (read) {
            copy_latch_q_to_cpu_d(cpu, latch);
        }
    } else {
        r01s_entity_eval(latch);
    }

    if (hit_scroll_y) {
        copy_cpu_d_to_latch_d(scroll_y, cpu);
        if (!read) {
            r01s_entity_drive(scroll_y, "LE", R01S_LVL_H);
        }
        r01s_entity_eval(scroll_y);
        if (read) {
            copy_latch_q_to_cpu_d(cpu, scroll_y);
        }
    } else {
        r01s_entity_eval(scroll_y);
    }

    if (hit_raster) {
        copy_cpu_d_to_latch_d(raster, cpu);
        if (!read) {
            r01s_entity_drive(raster, "LE", R01S_LVL_H);
        }
        r01s_entity_eval(raster);
        if (read) {
            copy_latch_q_to_cpu_d(cpu, raster);
        }
    } else {
        r01s_entity_eval(raster);
    }

    if (hit_pads && read) {
        r01s_entity_drive(pads, "CE#", R01S_LVL_L);
        r01s_entity_drive(pads, "OE#", R01S_LVL_L);
        r01s_entity_eval(pads);
        copy_bus_named(cpu, "D", pads, "DQ", 8);
        ctx->health_saw_pad = 1;
    } else {
        r01s_entity_eval(pads);
    }
}

/* Island H — DOT osc + beam PLD stub + HC688 raster compare vs $FE04. */
static void wire_beam(R01sBoard *ctx, R01sIslandGroup *group) {
    R01sEntity *pwr = r01s_pwr5v_entity(ctx->power_impl.pwr);
    R01sEntity *osc = r01s_osc_dot_entity(ctx->beam_impl.osc_dot);
    R01sEntity *beam = r01s_beam_xy_entity(ctx->beam_impl.beam);
    R01sEntity *cmp = r01s_sn74hc688_entity(ctx->beam_impl.cmp);
    R01sEntity *raster = r01s_sn74hc573_entity(ctx->io_latch_impl.raster);
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
        r01s_entity_drive(cmp, pn, r01s_entity_sense(beam, yn));
        {
            char ln[8];
            snprintf(ln, sizeof(ln), "%dQ", i + 1);
            r01s_entity_drive(cmp, qn, r01s_entity_sense(raster, ln));
        }
    }
    r01s_entity_drive(cmp, "OE#", R01S_LVL_L); /* compare always armed for smoke */
    r01s_entity_eval(beam);
    r01s_entity_eval(cmp);
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
    R01sEntity *mux = r01s_sn74hc157_entity(ctx->vram_impl.mux);
    R01sEntity *osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
    uint16_t cpu_addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int be = r01s_level_is_high(r01s_entity_sense(cpu, "BE"));
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
            ctx->vram_addr = (uint16_t)((ctx->vram_addr & 0xFF00u) | (r01s_bus_read(cpu, "D", 8) & 0xFFu));
            va = (uint16_t)(ctx->vram_addr & 0x7FFFu);
        }
        if (hit_hi && !read) {
            ctx->vram_addr =
                (uint16_t)((ctx->vram_addr & 0x00FFu) | ((r01s_bus_read(cpu, "D", 8) & 0xFFu) << 8));
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

    if (cpu_phase && be && hit_data) {
        r01s_entity_drive(vram, "CE#", R01S_LVL_L);
        if (read) {
            r01s_entity_drive(vram, "OE#", R01S_LVL_L);
            r01s_entity_drive(vram, "WE#", R01S_LVL_H);
            r01s_entity_eval(vram);
            copy_bus_named(cpu, "D", vram, "DQ", 8);
        } else {
            r01s_entity_drive(vram, "OE#", R01S_LVL_H);
            r01s_entity_drive(vram, "WE#", R01S_LVL_L);
            copy_bus_named(vram, "DQ", cpu, "D", 8);
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

/* Island I — BG fetch address from beam + scroll; eval before VRAM uses VA. */
static void wire_bg_fetch(R01sBoard *ctx) {
    R01sEntity *osc = r01s_osc8m_entity(ctx->clock_impl.osc);
    R01sBgFetch *bg = ctx->bg_fetch_impl.fetch;
    int cpu_phase = r01s_level_is_high(r01s_entity_sense(osc, "PHI2"));

    r01s_bg_fetch_set_beam(bg, r01s_beam_xy_x(ctx->beam_impl.beam), r01s_beam_xy_y(ctx->beam_impl.beam),
                           r01s_beam_xy_hblank(ctx->beam_impl.beam),
                           r01s_beam_xy_vblank(ctx->beam_impl.beam));
    r01s_bg_fetch_set_scroll(bg, r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch),
                             r01s_sn74hc573_peek_q(ctx->io_latch_impl.scroll_y));
    r01s_bg_fetch_set_cpu_phase(bg, cpu_phase);
    r01s_entity_eval(r01s_bg_fetch_entity(bg));
}

static void wire_memory(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *ram = r01s_as6c62256_entity(ctx->cpu_mem_impl.ram);
    R01sEntity *prg = r01s_prg_rom_entity(ctx->cpu_mem_impl.prg);
    uint16_t addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int a15 = (addr & 0x8000u) != 0;
    int io = addr_is_io(addr);

    r01s_entity_drive(ram, "CE#", R01S_LVL_H);
    r01s_entity_drive(ram, "OE#", R01S_LVL_H);
    r01s_entity_drive(ram, "WE#", R01S_LVL_H);
    r01s_entity_drive(prg, "CE#", R01S_LVL_H);
    r01s_entity_drive(prg, "OE#", R01S_LVL_H);
    r01s_bus_hiz(ram, "DQ", 8);
    r01s_bus_hiz(prg, "DQ", 8);

    if (!r01s_level_is_high(r01s_entity_sense(cpu, "BE")) || io) {
        r01s_entity_eval(ram);
        r01s_entity_eval(prg);
        return;
    }

    if (!a15) {
        copy_bus_named(ram, "A", cpu, "A", 15);
        r01s_entity_drive(ram, "CE#", R01S_LVL_L);
        if (read) {
            r01s_entity_drive(ram, "OE#", R01S_LVL_L);
            r01s_entity_drive(ram, "WE#", R01S_LVL_H);
            r01s_entity_eval(ram);
            copy_bus_named(cpu, "D", ram, "DQ", 8);
        } else {
            r01s_entity_drive(ram, "OE#", R01S_LVL_H);
            r01s_entity_drive(ram, "WE#", R01S_LVL_L);
            copy_bus_named(ram, "DQ", cpu, "D", 8);
            r01s_entity_eval(ram);
        }
        r01s_entity_eval(prg);
    } else {
        copy_bus_named(prg, "A", cpu, "A", 15);
        r01s_entity_drive(prg, "CE#", R01S_LVL_L);
        r01s_entity_drive(prg, "OE#", R01S_LVL_L);
        r01s_entity_eval(prg);
        if (read) {
            copy_bus_named(cpu, "D", prg, "DQ", 8);
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
    r01s_entity_drive(cpu, "IRQB", R01S_LVL_H);
    r01s_entity_drive(cpu, "NMIB", R01S_LVL_H);

    phi2 = r01s_entity_sense(osc, "PHI2");
    r01s_entity_drive(hc, "1A", phi2 == R01S_LVL_Z ? R01S_LVL_L : phi2);
    r01s_entity_drive(hc, "2A", resb);
    r01s_entity_eval(hc);

    r01s_entity_drive(cpu, "PHI2", phi2 == R01S_LVL_H ? R01S_LVL_H : R01S_LVL_L);
}

static void board_settle(R01sBoard *ctx, R01sIslandGroup *group) {
    int i;
    for (i = 0; i < R01S_SETTLE_PASSES; i++) {
        wire_power_clock_reset(ctx, group);
        wire_memory(ctx);
        wire_io(ctx);
        wire_beam(ctx, group);
        wire_bg_fetch(ctx);
        wire_vram(ctx);
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
    /*
     * Boot smoke for A–E + G + H + I:
     *   STA $FE02 = $55
     *   VRAM: addr=0, STA $FE12 = $AA, reseek 0, LDA $FE12
     *   Nametable cell0 tile=$42 attr=$07 for Island I
     *   then loop LDA $FE60
     */
    uint8_t prog[] = {
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
        0xAD, 0x12, 0xFE, /* LDA $FE12 @ $801A */
        0xA9, 0x00,       /* LDA #$00 — clear scroll for cell0 fetch */
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
        0xAD, 0x60, 0xFE, /* LDA $FE60 @ $8041 */
        0x4C, 0x41, 0x80, /* JMP $8041 (pad loop) */
    };

    r01s_w65c02s_init(impl->cpu, "U1");
    r01s_as6c62256_init(impl->ram, "U3");
    r01s_prg_rom_init(impl->prg, "U4");
    r01s_prg_rom_load(impl->prg, 0x0000, prog, (uint16_t)sizeof(prog));
    r01s_prg_rom_set_reset_vec(impl->prg, 0x8000);
    r01s_island_add_entity(island, r01s_w65c02s_entity(impl->cpu));
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->ram));
    r01s_island_add_entity(island, r01s_prg_rom_entity(impl->prg));
}

static void island_io_latch_init(R01sIsland *island) {
    R01sIslandIoLatchImpl *impl = (R01sIslandIoLatchImpl *)island->impl;
    r01s_sn74hc573_init(impl->latch, "U5");
    r01s_sn74hc573_init(impl->scroll_y, "U5Y");
    r01s_sn74hc573_init(impl->raster, "U5B");
    r01s_island_add_entity(island, r01s_sn74hc573_entity(impl->latch));
    r01s_island_add_entity(island, r01s_sn74hc573_entity(impl->scroll_y));
    r01s_island_add_entity(island, r01s_sn74hc573_entity(impl->raster));
}

static void island_beam_init(R01sIsland *island) {
    R01sIslandBeamImpl *impl = (R01sIslandBeamImpl *)island->impl;
    r01s_osc_dot_init(impl->osc_dot, "Y2");
    r01s_beam_xy_init(impl->beam, "UPLD");
    r01s_sn74hc688_init(impl->cmp, "U42");
    r01s_island_add_entity(island, r01s_osc_dot_entity(impl->osc_dot));
    r01s_island_add_entity(island, r01s_beam_xy_entity(impl->beam));
    r01s_island_add_entity(island, r01s_sn74hc688_entity(impl->cmp));
}

static void island_pads_init(R01sIsland *island) {
    R01sIslandPadsImpl *impl = (R01sIslandPadsImpl *)island->impl;
    r01s_pads_init(impl->pads, "PAD");
    r01s_island_add_entity(island, r01s_pads_entity(impl->pads));
}

static void island_vram_init(R01sIsland *island) {
    R01sIslandVramImpl *impl = (R01sIslandVramImpl *)island->impl;
    r01s_as6c62256_init(impl->vram, "U6");
    r01s_sn74hc157_init(impl->mux, "U7");
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->vram));
    r01s_island_add_entity(island, r01s_sn74hc157_entity(impl->mux));
}

static void island_bg_fetch_init(R01sIsland *island) {
    R01sIslandBgFetchImpl *impl = (R01sIslandBgFetchImpl *)island->impl;
    r01s_bg_fetch_init(impl->fetch, "UPLDI");
    r01s_island_add_entity(island, r01s_bg_fetch_entity(impl->fetch));
}

static const R01sIslandVTable ISLAND_POWER_VT = {island_power_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CLOCK_VT = {island_clock_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CPU_VT = {island_cpu_mem_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_IO_VT = {island_io_latch_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_PADS_VT = {island_pads_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_VRAM_VT = {island_vram_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_BEAM_VT = {island_beam_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_BG_FETCH_VT = {island_bg_fetch_init, NULL, NULL, NULL, NULL};

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
    ctx->health_saw_latch = 0;
    ctx->health_saw_vram = 0;
    ctx->health_saw_vram_read = 0;
    ctx->health_saw_pad = 0;
    ctx->health_saw_beam = 0;
    ctx->health_saw_bg_fetch = 0;
    ctx->health_phi2_edges = 0;
    r01s_bus_clear_conflicts();
    r01s_entity_reset(r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu));
    r01s_entity_reset(r01s_osc8m_entity(ctx->clock_impl.osc));
    r01s_entity_reset(r01s_sn74hc573_entity(ctx->io_latch_impl.latch));
    r01s_entity_reset(r01s_sn74hc573_entity(ctx->io_latch_impl.scroll_y));
    r01s_entity_reset(r01s_sn74hc573_entity(ctx->io_latch_impl.raster));
    r01s_entity_reset(r01s_pads_entity(ctx->pads_impl.pads));
    r01s_entity_reset(r01s_as6c62256_entity(ctx->vram_impl.vram));
    r01s_entity_reset(r01s_sn74hc157_entity(ctx->vram_impl.mux));
    r01s_entity_reset(r01s_osc_dot_entity(ctx->beam_impl.osc_dot));
    r01s_entity_reset(r01s_beam_xy_entity(ctx->beam_impl.beam));
    r01s_entity_reset(r01s_sn74hc688_entity(ctx->beam_impl.cmp));
    r01s_entity_reset(r01s_bg_fetch_entity(ctx->bg_fetch_impl.fetch));
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
    r01s_entity_tick(r01s_osc_dot_entity(ctx->beam_impl.osc_dot));
    board_settle(ctx, group);
    /* Beam senses DOT edges itself (domain independent of PHI2). */
    {
        R01sEntity *dot_osc = r01s_osc_dot_entity(ctx->beam_impl.osc_dot);
        R01sEntity *beam = r01s_beam_xy_entity(ctx->beam_impl.beam);
        r01s_entity_drive(beam, "DOT", r01s_entity_sense(dot_osc, "DOT"));
        r01s_entity_tick(beam);
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
            board_settle(ctx, group);
            r01s_entity_tick(cpu);
            ctx->cycles++;
            r01s_entity_eval(cpu);
            board_settle(ctx, group);
            /* Auto-inc once when the FE12 DATA cycle ends (not when it begins). */
            if (ctx->vram_fe12_armed &&
                r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu) != R01S_CPU_OP_DATA) {
                ctx->vram_addr = (uint16_t)((ctx->vram_addr + 1u) & 0x7FFFu);
                ctx->vram_fe12_armed = 0;
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
             r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch), (unsigned)(ctx->vram_addr & 0x7FFFu),
             r01s_as6c62256_peek(ctx->vram_impl.vram, 0), r01s_pads_get(ctx->pads_impl.pads, 0),
             r01s_pads_get(ctx->pads_impl.pads, 1), r01s_beam_xy_x(ctx->beam_impl.beam),
             r01s_beam_xy_y(ctx->beam_impl.beam), r01s_beam_xy_hblank(ctx->beam_impl.beam) ? " HB" : "",
             r01s_beam_xy_vblank(ctx->beam_impl.beam) ? " VB" : "",
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
    board->io_latch_impl.latch = &board->latch;
    board->io_latch_impl.scroll_y = &board->scroll_y_latch;
    board->io_latch_impl.raster = &board->raster_latch;
    board->pads_impl.pads = &board->pads;
    board->vram_impl.vram = &board->vram;
    board->vram_impl.mux = &board->vram_mux;
    board->beam_impl.osc_dot = &board->osc_dot;
    board->beam_impl.beam = &board->beam;
    board->beam_impl.cmp = &board->raster_cmp;
    board->bg_fetch_impl.fetch = &board->bg_fetch;

    if (r01s_island_builder_add(b, &ISLAND_POWER_VT, "ISLAND A  POWER", 0, 0, 1, 1, &board->power_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_CLOCK_VT, "ISLAND B  CLK RST", 0, 0, 1, 1, &board->clock_impl) <
        0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_CPU_VT, "ISLAND C  CPU RAM PRG", 0, 0, 1, 1,
                                &board->cpu_mem_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_IO_VT, "ISLAND D  FExx LATCH", 0, 0, 1, 1,
                                &board->io_latch_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_PADS_VT, "ISLAND E  PADS", 0, 0, 1, 1, &board->pads_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_VRAM_VT, "ISLAND G  VRAM", 0, 0, 1, 1, &board->vram_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_BEAM_VT, "ISLAND H  BEAM", 0, 0, 1, 1, &board->beam_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(b, &ISLAND_BG_FETCH_VT, "ISLAND I  BG FETCH", 0, 0, 1, 1,
                                &board->bg_fetch_impl) < 0) {
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
        R01sEntity *prg_e = r01s_prg_rom_entity(&board->prg);
        int x = cpu_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, ram_e, R01S_ISLAND_CPU, x, 0);
        x += ram_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, prg_e, R01S_ISLAND_CPU, x, 0);
    }
    r01s_island_builder_mount_rel(b, r01s_sn74hc573_entity(&board->latch), R01S_ISLAND_IO_LATCH, 0, 0);
    {
        R01sEntity *fe02 = r01s_sn74hc573_entity(&board->latch);
        R01sEntity *fe03 = r01s_sn74hc573_entity(&board->scroll_y_latch);
        int x = fe02->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, fe03, R01S_ISLAND_IO_LATCH, x, 0);
        x += fe03->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, r01s_sn74hc573_entity(&board->raster_latch), R01S_ISLAND_IO_LATCH, x,
                                      0);
    }
    r01s_island_builder_mount_rel(b, r01s_pads_entity(&board->pads), R01S_ISLAND_PADS, 0, 0);
    {
        R01sEntity *vram_e = r01s_as6c62256_entity(&board->vram);
        R01sEntity *mux_e = r01s_sn74hc157_entity(&board->vram_mux);
        int mux_y = (vram_e->body_h - mux_e->body_h) / 2;
        if (mux_y < 0) {
            mux_y = 0;
        }
        r01s_island_builder_mount_rel(b, vram_e, R01S_ISLAND_VRAM, 0, 0);
        r01s_island_builder_mount_rel(b, mux_e, R01S_ISLAND_VRAM, vram_e->body_w + R01S_CHIP_GAP, mux_y);
    }
    {
        R01sEntity *dot_e = r01s_osc_dot_entity(&board->osc_dot);
        R01sEntity *beam_e = r01s_beam_xy_entity(&board->beam);
        R01sEntity *cmp_e = r01s_sn74hc688_entity(&board->raster_cmp);
        int x = 0;
        int beam_y = (dot_e->body_h - beam_e->body_h) / 2;
        if (beam_y < 0) {
            beam_y = 0;
        }
        r01s_island_builder_mount_rel(b, dot_e, R01S_ISLAND_BEAM, 0, 0);
        x = dot_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, beam_e, R01S_ISLAND_BEAM, x, beam_y);
        x += beam_e->body_w + R01S_CHIP_GAP;
        r01s_island_builder_mount_rel(b, cmp_e, R01S_ISLAND_BEAM, x, 0);
    }
    r01s_island_builder_mount_rel(b, r01s_bg_fetch_entity(&board->bg_fetch), R01S_ISLAND_BG_FETCH, 0, 0);

    r01s_island_builder_fit_all(b);
    r01s_island_builder_arrange_rows(b, 40, 40, R01S_ISLAND_GAP, R01S_ISLAND_GAP, R01S_ISLAND_ROW_MAX_W);

    if (r01s_island_builder_finish(b) != 0) {
        return -1;
    }
    return 0;
}
