#include "islands/bringup_abc.h"

#include "retr01_sim/bus.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define R01S_ABC_RESET_HOLD 12

enum {
    R01S_ABC_ISLAND_A = 0,
    R01S_ABC_ISLAND_B = 1,
    R01S_ABC_ISLAND_C = 2,
};

static R01sBringupAbc *abc_from_group(R01sIslandGroup *group) {
    return group ? (R01sBringupAbc *)group->impl : NULL;
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

static void wire_memory(R01sBringupAbc *abc) {
    R01sEntity *cpu = r01s_w65c02s_entity(abc->cpu_mem_impl.cpu);
    R01sEntity *ram = r01s_as6c62256_entity(abc->cpu_mem_impl.ram);
    R01sEntity *prg = r01s_prg_rom_entity(abc->cpu_mem_impl.prg);
    uint16_t addr = (uint16_t)r01s_bus_read(cpu, "A", 16);
    int read = r01s_level_is_high(r01s_entity_sense(cpu, "RWB"));
    int a15 = (addr & 0x8000u) != 0;

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

static void wire_power_clock_reset(R01sBringupAbc *abc, R01sIslandGroup *group) {
    R01sEntity *pwr = r01s_pwr5v_entity(abc->power_impl.pwr);
    R01sEntity *osc = r01s_osc8m_entity(abc->clock_impl.osc);
    R01sEntity *hc = r01s_sn74hc14_entity(abc->clock_impl.hc14);
    R01sEntity *cpu = r01s_w65c02s_entity(abc->cpu_mem_impl.cpu);
    R01sLevel vdd, phi2, resb;

    r01s_entity_drive(pwr, "VIN", group->powered ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(pwr, "EN", R01S_LVL_H);
    r01s_entity_eval(pwr);
    vdd = r01s_entity_sense(pwr, "VDD");

    r01s_entity_drive(osc, "VDD", vdd);
    r01s_entity_drive(osc, "OE#", R01S_LVL_H);

    resb = (abc->reset_hold > 0) ? R01S_LVL_L : R01S_LVL_H;
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

static void island_a_init(R01sIsland *island) {
    R01sIslandPowerImpl *impl = (R01sIslandPowerImpl *)island->impl;
    r01s_pwr5v_init(impl->pwr, "PS1");
    r01s_island_add_entity(island, r01s_pwr5v_entity(impl->pwr));
}

static void island_b_init(R01sIsland *island) {
    R01sIslandClockImpl *impl = (R01sIslandClockImpl *)island->impl;
    r01s_osc8m_init(impl->osc, "Y1");
    r01s_sn74hc14_init(impl->hc14, "U2");
    r01s_island_add_entity(island, r01s_osc8m_entity(impl->osc));
    r01s_island_add_entity(island, r01s_sn74hc14_entity(impl->hc14));
}

static void island_c_init(R01sIsland *island) {
    R01sIslandCpuMemImpl *impl = (R01sIslandCpuMemImpl *)island->impl;
    uint8_t spin[] = {0xEA, 0xEA, 0x4C, 0x00, 0x80};

    r01s_w65c02s_init(impl->cpu, "U1");
    r01s_as6c62256_init(impl->ram, "U3");
    r01s_prg_rom_init(impl->prg, "U4");
    r01s_prg_rom_load(impl->prg, 0x0000, spin, (uint16_t)sizeof(spin));
    r01s_prg_rom_set_reset_vec(impl->prg, 0x8000);
    r01s_island_add_entity(island, r01s_w65c02s_entity(impl->cpu));
    r01s_island_add_entity(island, r01s_as6c62256_entity(impl->ram));
    r01s_island_add_entity(island, r01s_prg_rom_entity(impl->prg));
}

static const R01sIslandVTable ISLAND_A_VT = {island_a_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_B_VT = {island_b_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable ISLAND_C_VT = {island_c_init, NULL, NULL, NULL, NULL};

static void abc_shutdown(R01sIslandGroup *group) {
    R01sBringupAbc *abc = abc_from_group(group);
    int i;
    if (!abc) {
        return;
    }
    for (i = group->island_count - 1; i >= 0; i--) {
        r01s_island_shutdown(group->islands[i]);
    }
    group->island_count = 0;
}

static void abc_reset(R01sIslandGroup *group) {
    R01sBringupAbc *abc = abc_from_group(group);
    if (!abc) {
        return;
    }
    abc->reset_hold = R01S_ABC_RESET_HOLD;
    abc->cycles = 0;
    abc->phi2_prev = R01S_LVL_L;
    r01s_entity_reset(r01s_w65c02s_entity(abc->cpu_mem_impl.cpu));
    r01s_entity_reset(r01s_osc8m_entity(abc->clock_impl.osc));
    wire_power_clock_reset(abc, group);
    r01s_entity_eval(r01s_w65c02s_entity(abc->cpu_mem_impl.cpu));
    wire_memory(abc);
}

static void abc_eval_idle(R01sIslandGroup *group) {
    R01sBringupAbc *abc = abc_from_group(group);
    if (!abc) {
        return;
    }
    wire_power_clock_reset(abc, group);
    wire_memory(abc);
}

static void abc_step(R01sIslandGroup *group) {
    R01sBringupAbc *abc = abc_from_group(group);
    R01sEntity *osc;
    R01sEntity *cpu;
    R01sLevel phi2;
    if (!abc || !group->powered) {
        return;
    }
    osc = r01s_osc8m_entity(abc->clock_impl.osc);
    cpu = r01s_w65c02s_entity(abc->cpu_mem_impl.cpu);

    wire_power_clock_reset(abc, group);
    r01s_entity_tick(osc);
    wire_power_clock_reset(abc, group);

    phi2 = r01s_entity_sense(osc, "PHI2");
    if (phi2 == R01S_LVL_H && abc->phi2_prev != R01S_LVL_H) {
        if (abc->reset_hold > 0) {
            abc->reset_hold--;
            wire_power_clock_reset(abc, group);
            r01s_entity_eval(cpu);
        } else {
            wire_memory(abc);
            r01s_entity_tick(cpu);
            abc->cycles++;
            r01s_entity_eval(cpu);
            wire_memory(abc);
        }
    }
    abc->phi2_prev = phi2;
}

static void abc_status(R01sIslandGroup *group, char *buf, size_t buf_len) {
    R01sBringupAbc *abc = abc_from_group(group);
    R01sEntity *cpu;
    R01sEntity *pwr;
    R01sEntity *osc;
    if (!abc || !buf || buf_len == 0) {
        return;
    }
    cpu = r01s_w65c02s_entity(abc->cpu_mem_impl.cpu);
    pwr = r01s_pwr5v_entity(abc->power_impl.pwr);
    osc = r01s_osc8m_entity(abc->clock_impl.osc);
    snprintf(buf, buf_len,
             "%s  VDD=%c PHI2=%c RESB=%c  PC=%04X AB=%04X IR=%02X %s  cyc=%u  [SPC pause R reset . step]",
             group->running ? "RUN" : "PAUSE",
             r01s_level_is_high(r01s_entity_sense(pwr, "VDD")) ? 'H' : 'L',
             r01s_level_is_high(r01s_entity_sense(osc, "PHI2")) ? 'H' : 'L',
             r01s_level_is_low(r01s_entity_sense(cpu, "RESB")) ? 'L' : 'H',
             r01s_w65c02s_pc(abc->cpu_mem_impl.cpu),
             (unsigned)r01s_bus_read(cpu, "A", 16), abc->cpu.ir,
             phase_name(r01s_w65c02s_phase(abc->cpu_mem_impl.cpu)),
             (unsigned)abc->cycles);
}

static void abc_update_probes(R01sIslandGroup *group, int *probe_vdd, int *probe_phi2, int *probe_resb_low) {
    R01sBringupAbc *abc = abc_from_group(group);
    if (!abc) {
        return;
    }
    if (probe_vdd) {
        *probe_vdd = r01s_level_is_high(r01s_entity_sense(r01s_pwr5v_entity(abc->power_impl.pwr), "VDD"));
    }
    if (probe_phi2) {
        *probe_phi2 = r01s_level_is_high(r01s_entity_sense(r01s_osc8m_entity(abc->clock_impl.osc), "PHI2"));
    }
    if (probe_resb_low) {
        *probe_resb_low =
            r01s_level_is_low(r01s_entity_sense(r01s_w65c02s_entity(abc->cpu_mem_impl.cpu), "RESB"));
    }
}

static const R01sIslandGroupVTable ABC_GROUP_VT = {
    abc_shutdown,
    abc_reset,
    NULL,
    abc_step,
    abc_eval_idle,
    abc_status,
    abc_update_probes,
};

void r01s_bringup_abc_setup(R01sBringupAbc *abc, R01sIslandGroup *group) {
    if (!abc || !group) {
        return;
    }
    memset(abc, 0, sizeof(*abc));
    r01s_island_group_init(group);
    r01s_island_group_bind(group, &ABC_GROUP_VT, abc);

    abc->power_impl.pwr = &abc->pwr;
    abc->clock_impl.osc = &abc->osc;
    abc->clock_impl.hc14 = &abc->hc14;
    abc->cpu_mem_impl.cpu = &abc->cpu;
    abc->cpu_mem_impl.ram = &abc->ram;
    abc->cpu_mem_impl.prg = &abc->prg;

    r01s_island_setup(&abc->island_a, &ISLAND_A_VT, "ISLAND A  POWER", 40, 40, 220, 160, &abc->power_impl);
    r01s_island_setup(&abc->island_b, &ISLAND_B_VT, "ISLAND B  CLK RST", 40, 220, 220, 420, &abc->clock_impl);
    r01s_island_setup(&abc->island_c, &ISLAND_C_VT, "ISLAND C  CPU RAM PRG", 300, 40, 1000, 560,
                      &abc->cpu_mem_impl);

    r01s_island_init(&abc->island_a);
    r01s_island_init(&abc->island_b);
    r01s_island_init(&abc->island_c);

    r01s_island_group_add(group, &abc->island_a);
    r01s_island_group_add(group, &abc->island_b);
    r01s_island_group_add(group, &abc->island_c);

    r01s_island_group_reset(group);
}

void r01s_bringup_abc_mount(R01sBringupAbc *abc, R01sIslandGroup *group, R01sUi *ui) {
    if (!abc || !group || !ui) {
        return;
    }
    r01s_ui_bind_group(ui, group);

    r01s_entity_place(r01s_pwr5v_entity(&abc->pwr), 100, 90);
    r01s_entity_place(r01s_osc8m_entity(&abc->osc), 100, 280);
    r01s_entity_place(r01s_sn74hc14_entity(&abc->hc14), 90, 400);
    r01s_entity_place(r01s_w65c02s_entity(&abc->cpu), 380, 80);
    r01s_entity_place(r01s_as6c62256_entity(&abc->ram), 720, 80);
    r01s_entity_place(r01s_prg_rom_entity(&abc->prg), 1060, 80);

    r01s_ui_add_chip(ui, r01s_pwr5v_entity(&abc->pwr), R01S_ABC_ISLAND_A);
    r01s_ui_add_chip(ui, r01s_osc8m_entity(&abc->osc), R01S_ABC_ISLAND_B);
    r01s_ui_add_chip(ui, r01s_sn74hc14_entity(&abc->hc14), R01S_ABC_ISLAND_B);
    r01s_ui_add_chip(ui, r01s_w65c02s_entity(&abc->cpu), R01S_ABC_ISLAND_C);
    r01s_ui_add_chip(ui, r01s_as6c62256_entity(&abc->ram), R01S_ABC_ISLAND_C);
    r01s_ui_add_chip(ui, r01s_prg_rom_entity(&abc->prg), R01S_ABC_ISLAND_C);
}
