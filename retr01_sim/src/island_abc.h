#ifndef RETR01_SIM_ISLAND_ABC_H
#define RETR01_SIM_ISLAND_ABC_H

#include "as6c62256.h"
#include "osc8m.h"
#include "prg_rom.h"
#include "pwr5v.h"
#include "sn74hc14.h"
#include "ui.h"
#include "w65c02s.h"

/*
 * Protoboard islands A + B + C running together for the board view.
 * A: PWR5V  B: OSC8M + HC14  C: W65C02S + AS6C62256 + PRG_ROM
 */
typedef struct R01sIslandAbc {
    R01sPwr5v pwr;
    R01sOsc8m osc;
    R01sSn74hc14 hc14;
    R01sW65C02S cpu;
    R01sAs6c62256 ram;
    R01sPrgRom prg;

    int powered;     /* VIN present */
    int running;     /* sim advancing */
    int reset_hold;  /* cycles remaining with RESB low */
    uint32_t cycles; /* CPU PHI2 cycles since release */
    R01sLevel phi2_prev;
} R01sIslandAbc;

void r01s_island_abc_init(R01sIslandAbc *isl);
void r01s_island_abc_shutdown(R01sIslandAbc *isl);

/* Place chips on the UI board and register them. */
void r01s_island_abc_mount(R01sIslandAbc *isl, R01sUi *ui);

/* Hard reset: RESB hold, then run. */
void r01s_island_abc_reset(R01sIslandAbc *isl);

/* One visual frame: may run several half-cycles. */
void r01s_island_abc_frame(R01sIslandAbc *isl, R01sUi *ui);

/* Single PHI2 edge step (for SPACE-step when paused). */
void r01s_island_abc_step(R01sIslandAbc *isl);

#endif
