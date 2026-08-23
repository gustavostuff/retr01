#include "island_abc.h"

#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

#define R01S_ABC_RESET_HOLD 12
#define R01S_ABC_HALF_STEPS_PER_FRAME 8

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

static void wire_memory(R01sIslandAbc *isl) {
    R01sEntity *cpu = r01s_w65c02s_entity(&isl->cpu);
    R01sEntity *ram = r01s_as6c62256_entity(&isl->ram);
    R01sEntity *prg = r01s_prg_rom_entity(&isl->prg);
    uint16_t addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int a15 = (addr & 0x8000u) != 0;

    /* Default deselect */
    r01s_entity_drive(ram, "CE#", R01S_LVL_H);
    r01s_entity_drive(ram, "OE#", R01S_LVL_H);
    r01s_entity_drive(ram, "WE#", R01S_LVL_H);
    r01s_entity_drive(prg, "CE#", R01S_LVL_H);
    r01s_entity_drive(prg, "OE#", R01S_LVL_H);
    r01s_bus_hiz(ram, "DQ", 8);
    r01s_bus_hiz(prg, "DQ", 8);

    if (!r01s_level_is_high(r01s_entity_sense(cpu, "BE"))) {
        r01s_entity_eval(ram);
        r01s_entity_eval(prg);
        return;
    }

    if (!a15) {
        /* System RAM $0000-$7FFF */
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
        /* Tiny PRG window: CPU $8000-$FFFF -> ROM[A14:0] */
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

static void wire_power_clock_reset(R01sIslandAbc *isl) {
    R01sEntity *pwr = r01s_pwr5v_entity(&isl->pwr);
    R01sEntity *osc = r01s_osc8m_entity(&isl->osc);
    R01sEntity *hc = r01s_sn74hc14_entity(&isl->hc14);
    R01sEntity *cpu = r01s_w65c02s_entity(&isl->cpu);
    R01sLevel vdd, phi2, resb;

    r01s_entity_drive(pwr, "VIN", isl->powered ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(pwr, "EN", R01S_LVL_H);
    r01s_entity_eval(pwr);
    vdd = r01s_entity_sense(pwr, "VDD");

    r01s_entity_drive(osc, "VDD", vdd);
    r01s_entity_drive(osc, "OE#", R01S_LVL_H);

    resb = (isl->reset_hold > 0) ? R01S_LVL_L : R01S_LVL_H;
    r01s_entity_drive(cpu, "RESB", resb);
    r01s_entity_drive(cpu, "BE", R01S_LVL_H);
    r01s_entity_drive(cpu, "RDY", R01S_LVL_H);
    r01s_entity_drive(cpu, "IRQB", R01S_LVL_H);
    r01s_entity_drive(cpu, "NMIB", R01S_LVL_H);

    /* HC14: invert PHI2 on gate 1 for a visible blink; gate 2 buffers RESB. */
    phi2 = r01s_entity_sense(osc, "PHI2");
    r01s_entity_drive(hc, "1A", phi2 == R01S_LVL_Z ? R01S_LVL_L : phi2);
    r01s_entity_drive(hc, "2A", resb);
    r01s_entity_eval(hc);

    r01s_entity_drive(cpu, "PHI2", phi2 == R01S_LVL_H ? R01S_LVL_H : R01S_LVL_L);
}

void r01s_island_abc_init(R01sIslandAbc *isl) {
    uint8_t spin[] = {0xEA, 0xEA, 0x4C, 0x00, 0x80}; /* NOP NOP JMP $8000 */
    if (!isl) {
        return;
    }
    memset(isl, 0, sizeof(*isl));
    r01s_pwr5v_init(&isl->pwr, "PS1");
    r01s_osc8m_init(&isl->osc, "Y1");
    r01s_sn74hc14_init(&isl->hc14, "U2");
    r01s_w65c02s_init(&isl->cpu, "U1");
    r01s_as6c62256_init(&isl->ram, "U3");
    r01s_prg_rom_init(&isl->prg, "U4");

    r01s_prg_rom_load(&isl->prg, 0x0000, spin, (uint16_t)sizeof(spin));
    r01s_prg_rom_set_reset_vec(&isl->prg, 0x8000);

    isl->powered = 1;
    isl->running = 1;
    r01s_island_abc_reset(isl);
}

void r01s_island_abc_shutdown(R01sIslandAbc *isl) {
    if (!isl) {
        return;
    }
    r01s_entity_destroy(r01s_w65c02s_entity(&isl->cpu));
    r01s_entity_destroy(r01s_as6c62256_entity(&isl->ram));
    r01s_entity_destroy(r01s_prg_rom_entity(&isl->prg));
    r01s_entity_destroy(r01s_sn74hc14_entity(&isl->hc14));
    r01s_entity_destroy(r01s_osc8m_entity(&isl->osc));
    r01s_entity_destroy(r01s_pwr5v_entity(&isl->pwr));
}

static void register_traces(R01sIslandAbc *isl, R01sUi *ui) {
    R01sEntity *pwr = r01s_pwr5v_entity(&isl->pwr);
    R01sEntity *osc = r01s_osc8m_entity(&isl->osc);
    R01sEntity *hc = r01s_sn74hc14_entity(&isl->hc14);
    R01sEntity *cpu = r01s_w65c02s_entity(&isl->cpu);
    R01sEntity *ram = r01s_as6c62256_entity(&isl->ram);
    R01sEntity *prg = r01s_prg_rom_entity(&isl->prg);
    static const char *const AN[15] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6", "A7",
                                      "A8",  "A9",  "A10", "A11", "A12", "A13", "A14"};
    static const char *const DN[8] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};
    static const char *const DQN[8] = {"DQ0", "DQ1", "DQ2", "DQ3", "DQ4", "DQ5", "DQ6", "DQ7"};
    int i;

    r01s_traces_clear(&ui->traces);

    /* Net 1: VDD */
    r01s_traces_add(&ui->traces, pwr, "VDD", osc, "VDD", 1, 0);
    r01s_traces_add(&ui->traces, pwr, "VDD", cpu, "VDD", 1, 1);
    r01s_traces_add(&ui->traces, pwr, "VDD", ram, "VCC", 1, 2);
    r01s_traces_add(&ui->traces, pwr, "VDD", prg, "VCC", 1, 3);

    /* Net 2: PHI2 */
    r01s_traces_add(&ui->traces, osc, "PHI2", cpu, "PHI2", 2, 0);
    r01s_traces_add(&ui->traces, osc, "PHI2", hc, "1A", 2, 1);

    /* Net 3: RESB (CPU ← HC14 sense path visual: 2A monitors RESB) */
    r01s_traces_add(&ui->traces, cpu, "RESB", hc, "2A", 3, 0);

    /* Address A0..A14 */
    for (i = 0; i < 15; i++) {
        r01s_traces_add(&ui->traces, cpu, AN[i], ram, AN[i], 10 + i, i);
        r01s_traces_add(&ui->traces, cpu, AN[i], prg, AN[i], 10 + i, i + 16);
    }
    /* A15 to PRG CE# (window select hint) */
    r01s_traces_add(&ui->traces, cpu, "A15", prg, "CE#", 25, 0);

    /* Data bus */
    for (i = 0; i < 8; i++) {
        r01s_traces_add(&ui->traces, cpu, DN[i], ram, DQN[i], 30 + i, i);
        r01s_traces_add(&ui->traces, cpu, DN[i], prg, DQN[i], 30 + i, i + 10);
    }

    /* Controls */
    r01s_traces_add(&ui->traces, cpu, "RWB", ram, "WE#", 50, 0);
    r01s_traces_add(&ui->traces, cpu, "BE", ram, "OE#", 51, 0);
    r01s_traces_add(&ui->traces, cpu, "BE", ram, "CE#", 52, 1);
    r01s_traces_add(&ui->traces, cpu, "BE", prg, "OE#", 53, 0);
}

void r01s_island_abc_mount(R01sIslandAbc *isl, R01sUi *ui) {
    if (!isl || !ui) {
        return;
    }
    /* Spread layout for readable traces on 1600x900 board / 1280x720 view */
    r01s_entity_place(r01s_pwr5v_entity(&isl->pwr), 100, 90);
    r01s_entity_place(r01s_osc8m_entity(&isl->osc), 100, 280);
    r01s_entity_place(r01s_sn74hc14_entity(&isl->hc14), 90, 400);
    r01s_entity_place(r01s_w65c02s_entity(&isl->cpu), 380, 80);
    r01s_entity_place(r01s_as6c62256_entity(&isl->ram), 720, 80);
    r01s_entity_place(r01s_prg_rom_entity(&isl->prg), 1060, 80);

    r01s_ui_add_chip(ui, r01s_pwr5v_entity(&isl->pwr));
    r01s_ui_add_chip(ui, r01s_osc8m_entity(&isl->osc));
    r01s_ui_add_chip(ui, r01s_sn74hc14_entity(&isl->hc14));
    r01s_ui_add_chip(ui, r01s_w65c02s_entity(&isl->cpu));
    r01s_ui_add_chip(ui, r01s_as6c62256_entity(&isl->ram));
    r01s_ui_add_chip(ui, r01s_prg_rom_entity(&isl->prg));

    register_traces(isl, ui);
}

void r01s_island_abc_reset(R01sIslandAbc *isl) {
    if (!isl) {
        return;
    }
    isl->reset_hold = R01S_ABC_RESET_HOLD;
    isl->cycles = 0;
    isl->phi2_prev = R01S_LVL_L;
    r01s_entity_reset(r01s_w65c02s_entity(&isl->cpu));
    r01s_entity_reset(r01s_osc8m_entity(&isl->osc));
    wire_power_clock_reset(isl);
    r01s_entity_eval(r01s_w65c02s_entity(&isl->cpu));
    wire_memory(isl);
}

void r01s_island_abc_step(R01sIslandAbc *isl) {
    R01sEntity *osc;
    R01sEntity *cpu;
    R01sLevel phi2;
    if (!isl || !isl->powered) {
        return;
    }
    osc = r01s_osc8m_entity(&isl->osc);
    cpu = r01s_w65c02s_entity(&isl->cpu);

    wire_power_clock_reset(isl);
    r01s_entity_tick(osc); /* toggle PHI2 */
    wire_power_clock_reset(isl);

    phi2 = r01s_entity_sense(osc, "PHI2");
    /* Advance CPU on rising PHI2 */
    if (phi2 == R01S_LVL_H && isl->phi2_prev != R01S_LVL_H) {
        if (isl->reset_hold > 0) {
            isl->reset_hold--;
            wire_power_clock_reset(isl);
            r01s_entity_eval(cpu);
        } else {
            wire_memory(isl);
            r01s_entity_tick(cpu);
            isl->cycles++;
            r01s_entity_eval(cpu);
            wire_memory(isl);
        }
    }
    isl->phi2_prev = phi2;
}

void r01s_island_abc_frame(R01sIslandAbc *isl, R01sUi *ui) {
    int i;
    R01sEntity *cpu;
    R01sEntity *pwr;
    R01sEntity *osc;
    if (!isl) {
        return;
    }
    if (isl->running) {
        for (i = 0; i < R01S_ABC_HALF_STEPS_PER_FRAME; i++) {
            r01s_island_abc_step(isl);
        }
    } else {
        wire_power_clock_reset(isl);
        wire_memory(isl);
    }

    if (!ui) {
        return;
    }
    cpu = r01s_w65c02s_entity(&isl->cpu);
    pwr = r01s_pwr5v_entity(&isl->pwr);
    osc = r01s_osc8m_entity(&isl->osc);
    snprintf(ui->status, sizeof(ui->status),
             "%s  VDD=%c PHI2=%c RESB=%c  PC=%04X AB=%04X IR=%02X %s  cyc=%u  [SPC pause R reset . step]",
             isl->running ? "RUN" : "PAUSE",
             r01s_level_is_high(r01s_entity_sense(pwr, "VDD")) ? 'H' : 'L',
             r01s_level_is_high(r01s_entity_sense(osc, "PHI2")) ? 'H' : 'L',
             r01s_level_is_low(r01s_entity_sense(cpu, "RESB")) ? 'L' : 'H', r01s_w65c02s_pc(&isl->cpu),
             (unsigned)r01s_bus_read(cpu, "A", 16), isl->cpu.ir, phase_name(r01s_w65c02s_phase(&isl->cpu)),
             (unsigned)isl->cycles);
}
