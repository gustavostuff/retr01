#ifndef retr01_EMU_IO_H
#define retr01_EMU_IO_H

#include "retr01_emu/types.h"

#include <stdint.h>

struct R01eMachine;

/*
 * Logical $7F00-$7FFF register file (docs/general/memory.md, docs/general/video-graphics.md).
 * Soft contract for Emu / Studio Play: no DIP / Soft SEL / SPI mailbox.
 * Hard (SoT PLD/HC574): $7F02-$7F04. Soft0/1/2 families: see r01_soft_sel_demux.h.
 * $7F40-$7F5F = 8x4 voice window (r01_apu_window.h). Host Play fills via
 * NMI dual-stream tracker (r01_apu_tracker). PC speaker mixes that window.
 * $7F24 / $7F72 assert short CPU_RDY (R01_RDY_*_HOLDS). Raster IRQ still later.
 *
 * Soft fences (ic-comms-risks): scroll/palette apply in VBlank (or video off).
 * Pads latch once at VBlank enter. Host Play OAM/scroll publish in early VBlank.
 */
typedef struct R01eIo {
    uint8_t ctrl;        /* $7F00 Soft0 */
    uint8_t status;      /* $7F01 */
    uint8_t scroll_x;    /* $7F02 hard -- live (beam) */
    uint8_t scroll_y;    /* $7F03 hard -- live */
    uint8_t raster_y;    /* $7F04 hard -- compare (phase 8+) */
    uint8_t raster_ctrl; /* $7F05 Soft1 -- enable/ack (phase 8+) */
    uint8_t bg0_scroll_x; /* $7F06 Soft0 -- live */
    uint8_t bg0_scroll_y; /* $7F07 Soft0 -- live */

    uint8_t pal_row;  /* $7F08 Soft1 PAL_ROW -- live */
    uint8_t pal_addr; /* fill cursor for $7F09 (internal, resets on PAL_ROW write) */
    uint8_t pal[R01E_ACTIVE_PAL_BYTES];

    /* Mid-active pending; flushed at VBlank enter when scroll_pal_pending. */
    uint8_t scroll_x_next;
    uint8_t scroll_y_next;
    uint8_t bg0_scroll_x_next;
    uint8_t bg0_scroll_y_next;
    uint8_t pal_row_next;
    uint8_t pal_addr_next;
    uint8_t pal_next[R01E_ACTIVE_PAL_BYTES];
    uint8_t scroll_pal_pending;

    uint16_t vram_addr; /* $7F10/$7F11 */

    uint16_t oam_addr;             /* $7F20 Soft2; auto-inc wraps at OAM bytes */
    uint8_t oam[R01E_OAM_ENTRIES * R01E_OAM_ENTRY_BYTES];

    uint8_t world; /* $7F30 0..7 */
    uint8_t bank_helper[8]; /* $7F30-$7F37 helpers */

    uint8_t apu[0x20];    /* $7F40-$7F5F Soft2 8x4 window (R01_APU_REGS) */
    uint8_t pad0;         /* $7F60 -- latched (CPU / Host Play) */
    uint8_t pad1;         /* $7F61 -- latched */
    uint8_t pad0_host;    /* host staging; copied to pad0 at VBlank enter */
    uint8_t pad1_host;

    /* Cart save mailbox $7F22-$7F24 Soft2 (docs/general/memory.md). */
    uint8_t cartee_hi;
    uint8_t cartee_lo;
    uint8_t cartee_fe22_last;

    /* Machine EEPROM mailbox $7F70-$7F72 Soft2. */
    uint8_t meeprom_al;
    uint8_t meeprom_ah;

    uint32_t map_addr; /* $7F90-$7F92 Soft2 seek; $7F93 read auto-inc */

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
