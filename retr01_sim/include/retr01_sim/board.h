#ifndef RETR01_SIM_BOARD_H
#define RETR01_SIM_BOARD_H

#include "as6c62256.h"
#include "osc8m.h"
#include "pads.h"
#include "prg_rom.h"
#include "pwr5v.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/types.h"
#include "sn74hc14.h"
#include "sn74hc157.h"
#include "sn74hc573.h"
#include "w65c02s.h"

#include <stdint.h>

/* Combinatorial settle passes per wire/eval half-step (PLD/glue depth). */
#define R01S_SETTLE_PASSES 4

enum {
    R01S_ISLAND_POWER = 0,
    R01S_ISLAND_CLOCK = 1,
    R01S_ISLAND_CPU = 2,
    R01S_ISLAND_IO_LATCH = 3,
    R01S_ISLAND_PADS = 4,
    R01S_ISLAND_VRAM = 5,
};

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

typedef struct R01sIslandIoLatchImpl {
    R01sSn74hc573 *latch; /* $FE02 scroll X */
} R01sIslandIoLatchImpl;

typedef struct R01sIslandPadsImpl {
    R01sPads *pads; /* $FE60 / $FE61 */
} R01sIslandPadsImpl;

typedef struct R01sIslandVramImpl {
    R01sAs6c62256 *vram;   /* interleaved nametable SRAM */
    R01sSn74hc157 *mux;    /* PHI2 CPU vs PPU addr select (low 4 bits shown) */
} R01sIslandVramImpl;

/* Bring-up board: islands A–E + G (F deferred). */
typedef struct R01sBoard {
    R01sPwr5v pwr;
    R01sOsc8m osc;
    R01sSn74hc14 hc14;
    R01sW65C02S cpu;
    R01sAs6c62256 ram;
    R01sPrgRom prg;
    R01sSn74hc573 latch;
    R01sPads pads;
    R01sAs6c62256 vram;
    R01sSn74hc157 vram_mux;
    R01sIslandPowerImpl power_impl;
    R01sIslandClockImpl clock_impl;
    R01sIslandCpuMemImpl cpu_mem_impl;
    R01sIslandIoLatchImpl io_latch_impl;
    R01sIslandPadsImpl pads_impl;
    R01sIslandVramImpl vram_impl;
    /* Soft $FE10/$FE11 latch + $FE12 auto-inc (pre-full PLD). */
    uint16_t vram_addr;
    int vram_fe12_armed; /* set on FE12 access; auto-inc on next PHI2 rise */
    int reset_hold;
    uint32_t cycles;
    R01sLevel phi2_prev;
} R01sBoard;

/*
 * Bind group vtable, add islands A–E + G, mount chips, fit/arrange, finish.
 * builder must already be initialized. Returns 0 on success.
 */
int r01s_board_build(R01sBoard *board, R01sIslandBuilder *builder);

R01sBoard *r01s_board_from_group(R01sIslandGroup *group);

#endif
