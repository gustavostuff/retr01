#ifndef RETR01_EMU_TYPES_H
#define RETR01_EMU_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Cart / world layout — docs/02_graphics_worlds_memory.md */
#define R01E_CART_MAGIC "RETR01"
#define R01E_CART_FORMAT_VER 1
#define R01E_CART_FLASH_BYTES (512u * 1024u)
#define R01E_PRG_BYTES 32768u

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
#define R01E_PHASE1_WORLDS 1 /* Studio Phase 1: world 0 only */
#define R01E_BG_BANKS 4
#define R01E_SPR_BANKS 4
#define R01E_TILE_BYTES 16
#define R01E_CHR_BANK_BYTES 4096u
#define R01E_MASTER_COLORS 64
#define R01E_PAL_COLORS 4
#define R01E_PAL_ROWS 4
#define R01E_ACTIVE_PAL_BYTES 32

#define R01E_OAM_ENTRIES 64
#define R01E_OAM_ENTRY_BYTES 4 /* Y, tile, attr, X */

#define R01E_DOTS_X 341
#define R01E_DOTS_Y 262
#define R01E_VISIBLE_W 256
#define R01E_VISIBLE_H 240
#define R01E_CPU_HZ 8000000u
#define R01E_DOT_HZ 5369318u

#define R01E_ATTR_BANK_MASK 0x03u
#define R01E_ATTR_PAL_MASK 0x0Cu
#define R01E_ATTR_PAL_SHIFT 2
#define R01E_ATTR_FLIP_H 0x10u
#define R01E_ATTR_FLIP_V 0x20u

/* Pads $FE60/$FE61 — docs/02 */
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
