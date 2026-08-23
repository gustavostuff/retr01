#ifndef RETR01_SIM_BRINGUP_ABC_H
#define RETR01_SIM_BRINGUP_ABC_H

#include "as6c62256.h"
#include "osc8m.h"
#include "prg_rom.h"
#include "pwr5v.h"
#include "retr01_sim/island.h"
#include "retr01_sim/island_group.h"
#include "sn74hc14.h"
#include "w65c02s.h"

struct R01sUi;

/*
 * Bring-up islands A + B + C as one island group.
 * A: PWR5V  B: OSC8M + HC14  C: W65C02S + AS6C62256 + PRG_ROM
 */
typedef struct R01sIslandPowerImpl {
    R01sPwr5v *pwr;
} R01sIslandPowerImpl;

typedef struct R01sIslandClockImpl {
    R01sOsc8m *osc;
    R01sSn74hc14 *hc14;
} R01sIslandClockImpl;

typedef struct R01sIslandCpuMemImpl {
    R01sW65C02S *cpu;
    R01sAs6c62256 *ram;
    R01sPrgRom *prg;
} R01sIslandCpuMemImpl;

typedef struct R01sBringupAbc {
    R01sIsland island_a;
    R01sIsland island_b;
    R01sIsland island_c;
    R01sPwr5v pwr;
    R01sOsc8m osc;
    R01sSn74hc14 hc14;
    R01sW65C02S cpu;
    R01sAs6c62256 ram;
    R01sPrgRom prg;
    R01sIslandPowerImpl power_impl;
    R01sIslandClockImpl clock_impl;
    R01sIslandCpuMemImpl cpu_mem_impl;
    int reset_hold;
    uint32_t cycles;
    R01sLevel phi2_prev;
} R01sBringupAbc;

void r01s_bringup_abc_setup(R01sBringupAbc *abc, R01sIslandGroup *group);
void r01s_bringup_abc_mount(R01sBringupAbc *abc, R01sIslandGroup *group, struct R01sUi *ui);

#endif
