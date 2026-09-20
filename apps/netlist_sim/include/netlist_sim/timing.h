#ifndef NETLIST_SIM_TIMING_H
#define NETLIST_SIM_TIMING_H

#include <stdint.h>
#include <stdio.h>

/*
 * Propagation-delay budget for discrete glue (glue timing).
 *
 * Default / DELAY=: pin netlist stays combinatorial (same settle as LE/D sample).
 * Pin-accurate multi-chip tpd inside one PHI2 half would need intra-half
 * micro-settle and currently breaks MAP catchup (missed STA $FExx).
 *
 * DELAY=typical|max (aliases FAST=/PROP=/TPD=/NS_PROP_DELAY=) selects the
 * datasheet corner and prints path budget vs PHI2 half. Typ @ 5 V; max =
 * worst-case stress. Wall-clock UI FPS is not sim ns.
 */

#define NS_PHI2_NS 125u       /* 8.000 MHz period */
#define NS_PHI2_HALF_NS 62u   /* half-cycle quantum (floor of 62.5) */
#define NS_DOT_NS 186u        /* ~5.369 MHz period (informational) */

/* Datasheet-ish ns (5 V). typ from datasheet; max for budget stress. */
#define NS_TPD_HC245_TYP_NS 12u
#define NS_TPD_HC245_MAX_NS 30u
#define NS_TPD_HC157_TYP_NS 11u
#define NS_TPD_HC157_MAX_NS 25u
#define NS_TPD_ATF22_TYP_NS 5u
#define NS_TPD_ATF22_MAX_NS 15u
#define NS_TAA_SRAM_TYP_NS 35u
#define NS_TAA_SRAM_MAX_NS 55u /* AS6C62256-55 */

typedef enum NsTpdCorner {
    NS_TPD_TYP = 0,
    NS_TPD_MAX = 1
} NsTpdCorner;

typedef enum NsTpdPart {
    NS_TPD_PART_HC245 = 0,
    NS_TPD_PART_HC157,
    NS_TPD_PART_ATF22,
    NS_TPD_PART_SRAM_TAA
} NsTpdPart;

/* Delayed uint8 bus (HC245 side, SRAM DQ, HC157 Y nibble, PLD SEL / reg). */
typedef struct NsDelayU8 {
    uint8_t out;
    uint8_t next;
    uint8_t pending;
    uint64_t ready_ns;
} NsDelayU8;

void ns_timing_reset(void);
uint64_t ns_timing_now_ns(void);
void ns_timing_set_now_ns(uint64_t ns);
void ns_timing_advance_ns(uint32_t delta_ns);

/* 1 when NS_PROP_DELAY is set (or test override). */
int ns_timing_prop_enabled(void);
NsTpdCorner ns_timing_corner(void);
/* Datasheet ns for the active corner (budget / docs). Always non-zero for known parts. */
uint32_t ns_timing_tpd_ns(NsTpdPart part);

/*
 * Pin-model tpd for chip eval. Always 0: board settle samples LE/D in one pass.
 * Use ns_timing_tpd_ns + ns_delay_u8_update in unit tests for delay math.
 */
uint32_t ns_timing_pin_tpd_ns(NsTpdPart part);

/* Test / harness: override=-1 uses env; 0 force off; 1 force on. */
void ns_timing_set_prop_override(int enabled, NsTpdCorner corner);

void ns_delay_u8_reset(NsDelayU8 *d, uint8_t v);
/*
 * ideal = combinatorial result this eval.
 * Returns value to drive *now* (previous until tpd elapses).
 */
uint8_t ns_delay_u8_update(NsDelayU8 *d, uint8_t ideal, uint32_t tpd_ns);

/* Hot path stack for budget notes: decode + HC245 + PLD reg (ns). */
uint32_t ns_timing_path_decode_bus_reg_ns(void);

/* Print path budget vs PHI2 half (stderr if out is NULL). */
void ns_timing_print_budget(FILE *out);

#endif
