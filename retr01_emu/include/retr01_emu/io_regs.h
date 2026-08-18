#ifndef RETR01_EMU_IO_REGS_H
#define RETR01_EMU_IO_REGS_H

/*
 * E1 bring-up map for $FExx. Block families are locked in 08_memory_map.md;
 * exact bytes are still B2 — these offsets are the emulator default until
 * the hardware bitfields freeze.
 */

#define RETR01_IO_PPUCTRL 0x00    /* bit7 = NMI enable */
#define RETR01_IO_PPUSTATUS 0x01  /* bit7 = vblank (clears on read) */
#define RETR01_IO_SCROLL_X 0x02
#define RETR01_IO_SCROLL_Y 0x03
#define RETR01_IO_NT_ARRANGE 0x04
#define RETR01_IO_RASTER_Y 0x05
#define RETR01_IO_BEAM_Y 0x06 /* read-only */
#define RETR01_IO_RASTER_IRQ 0x07 /* bit0 enable; write 1 ack hit */

#define RETR01_IO_VADDR_LO 0x10
#define RETR01_IO_VADDR_HI 0x11
#define RETR01_IO_VDATA 0x12
#define RETR01_IO_VINC 0x13

#define RETR01_IO_OAM_ADDR 0x20
#define RETR01_IO_OAM_DATA 0x21
#define RETR01_IO_OAM_DMA 0x22

#define RETR01_IO_WORLD 0x30
#define RETR01_IO_BG_BANK 0x31
#define RETR01_IO_SPR_BANK 0x32

#define RETR01_IO_PAD0 0x60
#define RETR01_IO_PAD1 0x61
#define RETR01_IO_PAD2 0x62
#define RETR01_IO_PAD3 0x63

#define RETR01_IO_PRG_BANK 0x80

#define RETR01_IO_MAP_LO 0x90
#define RETR01_IO_MAP_MID 0x91
#define RETR01_IO_MAP_HI 0x92
#define RETR01_IO_MAP_DATA 0x93

#define RETR01_PPUCTRL_NMI 0x80
#define RETR01_PPUSTATUS_VBLANK 0x80
#define RETR01_PPUSTATUS_RASTER 0x40

#define RETR01_PAD_RIGHT 0x01
#define RETR01_PAD_LEFT 0x02
#define RETR01_PAD_DOWN 0x04
#define RETR01_PAD_UP 0x08
#define RETR01_PAD_A 0x10
#define RETR01_PAD_B 0x20
#define RETR01_PAD_SELECT 0x40
#define RETR01_PAD_START 0x80

#endif
