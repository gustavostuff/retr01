#include "retr01_sim/board.h"

#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"

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

static int addr_is_io(uint16_t addr) {
    return addr >= 0xFE00u && addr <= 0xFEFFu;
}

/* Soft PLD: $FE02 latch + $FE60/$FE61 pads. Deselect RAM/PRG in $FExx. */
static void wire_io(R01sBoard *ctx) {
    R01sEntity *cpu = r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu);
    R01sEntity *latch = r01s_sn74hc573_entity(ctx->io_latch_impl.latch);
    R01sEntity *pads = r01s_pads_entity(ctx->pads_impl.pads);
    uint16_t addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int be = r01s_level_is_high(r01s_entity_sense(cpu, "BE"));
    int hit_latch = (addr == 0xFE02u);
    int hit_pads = (addr == 0xFE60u || addr == 0xFE61u);

    /* Default: I/O devices idle */
    r01s_entity_drive(latch, "OE", R01S_LVL_L); /* Q visible */
    r01s_entity_drive(latch, "LE", R01S_LVL_L);
    r01s_entity_drive(pads, "CE#", R01S_LVL_H);
    r01s_entity_drive(pads, "OE#", R01S_LVL_H);
    r01s_entity_drive(pads, "A0", R01S_LVL_L);

    if (!be || !addr_is_io(addr)) {
        r01s_entity_eval(latch);
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

    if (hit_pads && read) {
        r01s_entity_drive(pads, "CE#", R01S_LVL_L);
        r01s_entity_drive(pads, "OE#", R01S_LVL_L);
        r01s_entity_eval(pads);
        copy_bus_named(cpu, "D", pads, "DQ", 8);
    } else {
        r01s_entity_eval(pads);
    }
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
    /* Boot: STA $FE02 = $55 once, then loop LDA $FE60 (live pad read into A). */
    uint8_t prog[] = {0xA9, 0x55, 0x8D, 0x02, 0xFE, 0xAD, 0x60, 0xFE, 0x4C, 0x06, 0x80};

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
    r01s_island_add_entity(island, r01s_sn74hc573_entity(impl->latch));
}

static void island_pads_init(R01sIsland *island) {
    R01sIslandPadsImpl *impl = (R01sIslandPadsImpl *)island->impl;
    r01s_pads_init(impl->pads, "PAD");
    r01s_island_add_entity(island, r01s_pads_entity(impl->pads));
}

static const R01sIslandVTable ISLAND_POWER_VT = {island_power_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CLOCK_VT = {island_clock_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_CPU_VT = {island_cpu_mem_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_IO_VT = {island_io_latch_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_PADS_VT = {island_pads_init, NULL, NULL, NULL, NULL};

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
    r01s_bus_clear_conflicts();
    r01s_entity_reset(r01s_w65c02s_entity(ctx->cpu_mem_impl.cpu));
    r01s_entity_reset(r01s_osc8m_entity(ctx->clock_impl.osc));
    r01s_entity_reset(r01s_sn74hc573_entity(ctx->io_latch_impl.latch));
    r01s_entity_reset(r01s_pads_entity(ctx->pads_impl.pads));
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
    board_settle(ctx, group);

    phi2 = r01s_entity_sense(osc, "PHI2");
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
        }
    }
    ctx->phi2_prev = phi2;
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
             "%s  VDD=%c PHI2=%c RESB=%c  PC=%04X A=%02X AB=%04X IR=%02X %s  LE=%02X P1=%02X P2=%02X  cyc=%u",
             group->running ? "RUN" : "PAUSE",
             r01s_level_is_high(r01s_entity_sense(pwr, "VDD")) ? 'H' : 'L',
             r01s_level_is_high(r01s_entity_sense(osc, "PHI2")) ? 'H' : 'L',
             r01s_level_is_low(r01s_entity_sense(cpu, "RESB")) ? 'L' : 'H',
             r01s_w65c02s_pc(ctx->cpu_mem_impl.cpu), r01s_w65c02s_a(ctx->cpu_mem_impl.cpu),
             (unsigned)r01s_bus_read(cpu, "A", 16), ctx->cpu.ir,
             phase_name(r01s_w65c02s_phase(ctx->cpu_mem_impl.cpu)),
             r01s_sn74hc573_peek_q(ctx->io_latch_impl.latch),
             r01s_pads_get(ctx->pads_impl.pads, 0), r01s_pads_get(ctx->pads_impl.pads, 1),
             (unsigned)ctx->cycles);
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
    board->pads_impl.pads = &board->pads;

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
    r01s_island_builder_mount_rel(b, r01s_pads_entity(&board->pads), R01S_ISLAND_PADS, 0, 0);

    r01s_island_builder_fit_all(b);
    r01s_island_builder_arrange(b, 40, 40, R01S_ISLAND_GAP, 1);

    if (r01s_island_builder_finish(b) != 0) {
        return -1;
    }
    return 0;
}
