#ifndef retr01_SIM_BOM32_H
#define retr01_SIM_BOM32_H

/*
 * Mounted VIS_IC parts, excluding pad ATtiny85s.
 * Mounted VIS_IC parts, excluding pad ATtiny85s.
 * Includes U04/U74 clock logic and U725 AD724.
 */
#define R01S_BOM_HC157_N 3
#define R01S_BOM_PLD_N   3
#define R01S_BOM_IC_N    21

enum {
    R01S_MUX157_VRAM0 = 0, /* VRAM CPU/PPU interleave nybble 0 */
    R01S_MUX157_VRAM1,
    R01S_MUX157_VRAM2,
};

enum {
    R01S_PLD_BEAM_X = 0,
    R01S_PLD_BEAM_Y,
    R01S_PLD_COMP,
    /* Non-BOM helpers still used by wire_* (folded decode / VRAM glue). */
    R01S_PLD_DECODE = 3,
    R01S_PLD_VRAM = 4,
};

#endif
