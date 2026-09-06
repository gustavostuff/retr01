#ifndef RETR01_SIM_TIMING_H
#define RETR01_SIM_TIMING_H

#include <stdint.h>
#include <stdio.h>

/*
 * Propagation-delay budget for discrete glue (docs/passive_rf_etc.md).
 *
 * Default / DELAY=: pin netlist stays combinatorial (same settle as LE/D sample).
 * Pin-accurate multi-chip tpd inside one PHI2 half would need intra-half
 * micro-settle and currently breaks MAP catchup (missed STA $FExx).
 *
 * DELAY=typical|max (aliases FAST=/PROP=/TPD=/R01S_PROP_DELAY=) selects the
 * datasheet corner and prints path budget vs PHI2 half. Typ @ 5 V; max =
 * worst-case stress. Wall-clock UI FPS is not sim ns.
 */

#define R01S_PHI2_NS 125u       /* 8.000 MHz period */
#define R01S_PHI2_HALF_NS 62u   /* half-cycle quantum (floor of 62.5) */
#define R01S_DOT_NS 186u        /* ~5.369 MHz period (informational) */

/* Datasheet-ish ns (5 V). typ from hw/md; max for budget stress. */
#define R01S_TPD_HC245_TYP_NS 12u
#define R01S_TPD_HC245_MAX_NS 30u
#define R01S_TPD_HC157_TYP_NS 11u
#define R01S_TPD_HC157_MAX_NS 25u
#define R01S_TPD_ATF22_TYP_NS 5u
#define R01S_TPD_ATF22_MAX_NS 15u
#define R01S_TAA_SRAM_TYP_NS 35u
#define R01S_TAA_SRAM_MAX_NS 55u /* AS6C62256-55 */

typedef enum R01sTpdCorner {
    R01S_TPD_TYP = 0,
    R01S_TPD_MAX = 1
} R01sTpdCorner;

typedef enum R01sTpdPart {
    R01S_TPD_PART_HC245 = 0,
    R01S_TPD_PART_HC157,
    R01S_TPD_PART_ATF22,
    R01S_TPD_PART_SRAM_TAA
} R01sTpdPart;

/* Delayed uint8 bus (HC245 side, SRAM DQ, HC157 Y nibble, PLD SEL / reg). */
typedef struct R01sDelayU8 {
    uint8_t out;
    uint8_t next;
    uint8_t pending;
    uint64_t ready_ns;
} R01sDelayU8;

void r01s_timing_reset(void);
uint64_t r01s_timing_now_ns(void);
void r01s_timing_set_now_ns(uint64_t ns);
void r01s_timing_advance_ns(uint32_t delta_ns);

/* 1 when R01S_PROP_DELAY is set (or test override). */
int r01s_timing_prop_enabled(void);
R01sTpdCorner r01s_timing_corner(void);
/* Datasheet ns for the active corner (budget / docs). Always non-zero for known parts. */
uint32_t r01s_timing_tpd_ns(R01sTpdPart part);

/*
 * Pin-model tpd for chip eval. Always 0: board settle samples LE/D in one pass.
 * Use r01s_timing_tpd_ns + r01s_delay_u8_update in unit tests for delay math.
 */
uint32_t r01s_timing_pin_tpd_ns(R01sTpdPart part);

/* Test / harness: override=-1 uses env; 0 force off; 1 force on. */
void r01s_timing_set_prop_override(int enabled, R01sTpdCorner corner);

void r01s_delay_u8_reset(R01sDelayU8 *d, uint8_t v);
/*
 * ideal = combinatorial result this eval.
 * Returns value to drive *now* (previous until tpd elapses).
 */
uint8_t r01s_delay_u8_update(R01sDelayU8 *d, uint8_t ideal, uint32_t tpd_ns);

/* Hot path stack for budget notes: decode + HC245 + PLD reg (ns). */
uint32_t r01s_timing_path_decode_bus_reg_ns(void);

/* Print path budget vs PHI2 half (stderr if out is NULL). */
void r01s_timing_print_budget(FILE *out);

#endif
