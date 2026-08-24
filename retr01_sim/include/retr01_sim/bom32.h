#ifndef RETR01_SIM_BOM32_H
#define RETR01_SIM_BOM32_H

/*
 * Retr01-A 32-IC BOM ([docs/06_hardware_v1_32ic.md]).
 * Counts are discrete silicon packages on the motherboard + cart save EEPROM.
 * Support parts (PWR, oscillators, LCD sink) are not in this tally.
 */
#define R01S_BOM_HC573_N 9
#define R01S_BOM_HC157_N 6
#define R01S_BOM_HC245_N 3
#define R01S_BOM_PLD_N   5
#define R01S_BOM_IC_N    32

/* Packed HC573 assignments (logical ports — bitfields TBD in docs/02). */
enum {
    R01S_LATCH_FE02 = 0, /* scroll X */
    R01S_LATCH_FE03,     /* scroll Y */
    R01S_LATCH_FE04,     /* raster compare Y */
    R01S_LATCH_FE10,     /* VRAM addr lo */
    R01S_LATCH_FE11,     /* VRAM addr hi */
    R01S_LATCH_FE90,     /* MAP seek lo */
    R01S_LATCH_FE91,     /* MAP seek mid */
    R01S_LATCH_FE92,     /* MAP seek hi */
    R01S_LATCH_FE08,     /* palette addr (low 5 bits used) */
};

enum {
    R01S_MUX157_VRAM0 = 0, /* VRAM CPU/PPU interleave nybble 0 */
    R01S_MUX157_VRAM1,
    R01S_MUX157_VRAM2,
    R01S_MUX157_LINEBUF0, /* linebuf MCU vs beam */
    R01S_MUX157_LINEBUF1,
    R01S_MUX157_LINEBUF2,
};

enum {
    R01S_BUS245_CPU = 0,
    R01S_BUS245_VIDEO,
    R01S_BUS245_CART_OAM,
};

enum {
    R01S_PLD_DECODE = 0,
    R01S_PLD_VRAM,
    R01S_PLD_BEAM_X,
    R01S_PLD_BEAM_Y,
    R01S_PLD_COMP,
};

#endif
