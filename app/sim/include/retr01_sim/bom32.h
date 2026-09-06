#ifndef retr01_SIM_BOM32_H
#define retr01_SIM_BOM32_H

/*
 * retr01-A BOM ([docs/hardware.md]).
 * Counts are discrete silicon packages on the motherboard + cart save EEPROM.
 * Support parts (PWR, oscillators, LCD sink) are not in this tally.
 * 23-IC silicon. Soft $FExx on R01sBoard / 1284. Scroll/raster/MAP high in PLDs.
 */
#define R01S_BOM_HC157_N 6
#define R01S_BOM_HC245_N 3
#define R01S_BOM_PLD_N   5
#define R01S_BOM_IC_N    23

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
