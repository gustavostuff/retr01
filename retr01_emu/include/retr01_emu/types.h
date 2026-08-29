#ifndef retr01_EMU_TYPES_H
#define retr01_EMU_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Cart / world layout -- docs/02_graphics_worlds_memory.md */
#define R01E_CART_MAGIC "retr01"
#define R01E_CART_FORMAT_VER 2
#define R01E_CART_PTR_TABLE_BYTES 36u
#define R01E_CART_OTHER_MAX 48
#define R01E_CART_OTHER_CREDITS_FIRST 2
#define R01E_CART_CREDITS_MIN 0
#define R01E_CART_CREDITS_MAX (R01E_CART_OTHER_MAX - R01E_CART_OTHER_CREDITS_FIRST) /* 46 */
#define R01E_CART_OTHER_HDR_BYTES 4u
#define R01E_CART_OTHER_DIR_BYTES 8u
#define R01E_CART_OTHER_FLAG_RLE 0x01u
#define R01E_CART_FLASH_BYTES (512u * 1024u)
#define R01E_PRG_BYTES 32768u /* fixed 32 KB window at $8000 (docs/02) */

#define R01E_SCREEN_TILES_X 16
#define R01E_SCREEN_TILES_Y 15
#define R01E_SCREEN_PX_W 128
#define R01E_SCREEN_PX_H 120
#define R01E_TILES_PER_SCREEN 240
#define R01E_ATTRS_PER_SCREEN 240
#define R01E_SCREEN_PAYLOAD 480
#define R01E_VRAM_SLOT_BYTES 512
#define R01E_VRAM_BYTES 0x4000u

#define R01E_MAX_WORLDS 8
#define R01E_MAX_PRESENT_SCREENS 32 /* cart cap: 8 worlds x 32 (docs/02) */
#define R01E_PARALLAX_MIN 0
#define R01E_PARALLAX_MAX 8 /* per world; live VRAM 4-5 */
#define R01E_PARALLAX_SLICE_MAX 120 /* max bands; variable thickness (docs/02) */
#define R01E_PHASE1_WORLDS 1 /* Studio Phase 1: world 0 only */
#define R01E_BG_BANKS 4
#define R01E_SPR_BANKS 4
#define R01E_TILE_BYTES 16
#define R01E_CHR_BANK_BYTES 4096u
#define R01E_MASTER_COLORS 64
#define R01E_PAL_COLORS 4
#define R01E_PALS_PER_ROW 4
#define R01E_PAL_ROWS 8
#define R01E_PAL_ROW_BYTES (R01E_PALS_PER_ROW * R01E_PAL_COLORS) /* 16 */
#define R01E_PAL_PLANE_BYTES (R01E_PAL_ROWS * R01E_PAL_ROW_BYTES) /* 128 */
#define R01E_ACTIVE_PAL_BYTES 32 /* 4 BG + 4 sprite pals loaded from one row */

#define R01E_OAM_ENTRIES 64
#define R01E_OAM_ENTRY_BYTES 4 /* Y, tile, attr, X */

#define R01E_DOTS_X 341
#define R01E_DOTS_Y 262
#define R01E_VISIBLE_W 256
#define R01E_VISIBLE_H 240
/* 2x2 camera workbench atlas: 2*128 x 2*120 = 256x240 (1:1, no scale). */
#define R01E_VRAM_ATLAS_W (R01E_SCREEN_PX_W * 2)
#define R01E_VRAM_ATLAS_H (R01E_SCREEN_PX_H * 2)
#define R01E_CPU_HZ 8000000u
#define R01E_DOT_HZ 5369318u
/* Nominal CPU cycles per CRT frame (8 MHz / ~60.098 Hz). Use 64-bit mul -- 8e6*1000 overflows u32. */
#define R01E_CYCLES_PER_FRAME ((uint64_t)R01E_CPU_HZ * 1000ull / 60098ull)
/* Soft max for game logic (docs/07): bars scale to this; red line = 100%. */
#define R01E_CPU_BUDGET_CYCLES 50000ull

#define R01E_ATTR_BANK_MASK 0x03u
#define R01E_ATTR_PAL_MASK 0x0Cu
#define R01E_ATTR_PAL_SHIFT 2
#define R01E_ATTR_FLIP_H 0x10u
#define R01E_ATTR_FLIP_V 0x20u
#define R01E_ATTR_SOLID 0x40u
#define R01E_OAM_PRIORITY 0x40u
#define R01E_OAM_SIZE_16 0x80u /* 0=8x8, 1=8x16 */
#define R01E_SPRITES_PER_LINE 16

/* Pads $FE60/$FE61 -- docs/02 */
#define R01E_PAD_RIGHT 0x01u
#define R01E_PAD_LEFT 0x02u
#define R01E_PAD_DOWN 0x04u
#define R01E_PAD_UP 0x08u
#define R01E_PAD_X 0x10u
#define R01E_PAD_Y 0x20u
#define R01E_PAD_COIN 0x40u
#define R01E_PAD_START 0x80u

#define R01E_PPUCTRL_BG_EN 0x01u
#define R01E_PPUCTRL_NMI_EN 0x80u
#define R01E_PPUSTATUS_VBLANK 0x80u
#define R01E_PPUSTATUS_HIT 0x40u

#endif
