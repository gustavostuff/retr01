#ifndef retr01_EMU_IO_H
#define retr01_EMU_IO_H

#include "retr01_emu/types.h"

#include <stdint.h>

struct R01eMachine;

/*
 * Logical $FE00-$FEFF register file (docs/02).
 * Phase 1: scroll, world, VRAM, palette, pads, MAP read, beam tick.
 * Later: raster IRQ ($FE04/$FE05), APU ($FE40-$FE5F), EEPROM ($FE70-$FE72),
 *        OAM sprite pipeline ($FE20/$FE21), parallax plane band ($FE06/$FE07).
 */
typedef struct R01eIo {
    uint8_t ctrl;        /* $FE00 */
    uint8_t status;      /* $FE01 */
    uint8_t scroll_x;    /* $FE02 0..127 */
    uint8_t scroll_y;    /* $FE03 0..119 */
    uint8_t raster_y;    /* $FE04 — compare (phase 8+) */
    uint8_t raster_ctrl; /* $FE05 — enable/ack (phase 8+) */
    uint8_t plane_lo;    /* $FE06 — parallax band (phase 2+) */
    uint8_t plane_hi;    /* $FE07 */

    uint8_t pal_addr; /* $FE08 */
    uint8_t pal[R01E_ACTIVE_PAL_BYTES];

    uint16_t vram_addr; /* $FE10/$FE11 */

    uint8_t oam_addr;              /* $FE20 */
    uint8_t oam[R01E_OAM_ENTRIES * R01E_OAM_ENTRY_BYTES]; /* phase 6+ composite */

    uint8_t world; /* $FE30 0..7 */
    uint8_t bank_helper[8]; /* $FE31-$FE37 + $FE38 pal row hint */
    uint8_t pal_row;

    uint8_t apu[0x20];    /* $FE40-$FE5F — phase 9+ */
    uint8_t pad0;         /* $FE60 */
    uint8_t pad1;         /* $FE61 */
    uint8_t eeprom[3];    /* $FE70-$FE72 — phase 10+ save protocol */

    uint32_t map_addr; /* $FE90-$FE92 seek; $FE93 read auto-inc */

    int dot_x;
    int dot_y;
    int frame;
} R01eIo;

void r01e_io_reset(R01eIo *io);
uint8_t r01e_io_read(struct R01eMachine *m, uint16_t addr);
void r01e_io_write(struct R01eMachine *m, uint16_t addr, uint8_t v);

/* Advance CRT beam one dot; may set VBlank / NMI pending on machine. */
void r01e_io_dot(struct R01eMachine *m);

#endif
