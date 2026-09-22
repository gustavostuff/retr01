#ifndef R01_HW_H
#define R01_HW_H

#include "r01_hw_regs.h"

#include <stdint.h>

#ifdef R01_HOST_TEST
extern uint8_t r01_host_io[256];
#define R01_IO(off) ((volatile uint8_t *)&r01_host_io[(off)])
#define R01_PPUCTRL R01_IO(0x00)
#define R01_PPUSTATUS R01_IO(0x01)
#define R01_SCROLL_X R01_IO(0x02)
#define R01_SCROLL_Y R01_IO(0x03)
#define R01_BG0_SCROLL_X R01_IO(0x06)
#define R01_BG0_SCROLL_Y R01_IO(0x07)
#define R01_PAL_ROW R01_IO(0x08)
#define R01_PAL_DATA R01_IO(0x09)
#define R01_VRAM_ADDR_LO R01_IO(0x10)
#define R01_VRAM_ADDR_HI R01_IO(0x11)
#define R01_VRAM_DATA R01_IO(0x12)
#define R01_OAM_ADDR R01_IO(0x20)
#define R01_OAM_DATA R01_IO(0x21)
#define R01_WORLD_PORT R01_IO(0x30)
#define R01_PAD0 R01_IO(0x60)
#define R01_PAD1 R01_IO(0x61)
#define R01_MAP_LO R01_IO(0x90)
#define R01_MAP_MID R01_IO(0x91)
#define R01_MAP_HI R01_IO(0x92)
#define R01_MAP_DATA R01_IO(0x93)
#define R01_APU R01_IO(0x40)
#else
#define R01_PPUCTRL ((volatile uint8_t *)0x7F00u)
#define R01_PPUSTATUS ((volatile uint8_t *)0x7F01u)
#define R01_SCROLL_X ((volatile uint8_t *)0x7F02u)
#define R01_SCROLL_Y ((volatile uint8_t *)0x7F03u)
#define R01_BG0_SCROLL_X ((volatile uint8_t *)0x7F06u)
#define R01_BG0_SCROLL_Y ((volatile uint8_t *)0x7F07u)
#define R01_PAL_ROW ((volatile uint8_t *)0x7F08u)
#define R01_PAL_DATA ((volatile uint8_t *)0x7F09u)
#define R01_VRAM_ADDR_LO ((volatile uint8_t *)0x7F10u)
#define R01_VRAM_ADDR_HI ((volatile uint8_t *)0x7F11u)
#define R01_VRAM_DATA ((volatile uint8_t *)0x7F12u)
#define R01_OAM_ADDR ((volatile uint8_t *)0x7F20u)
#define R01_OAM_DATA ((volatile uint8_t *)0x7F21u)
#define R01_WORLD_PORT ((volatile uint8_t *)0x7F30u)
#define R01_PAD0 ((volatile uint8_t *)0x7F60u)
#define R01_PAD1 ((volatile uint8_t *)0x7F61u)
#define R01_MAP_LO ((volatile uint8_t *)0x7F90u)
#define R01_MAP_MID ((volatile uint8_t *)0x7F91u)
#define R01_MAP_HI ((volatile uint8_t *)0x7F92u)
#define R01_MAP_DATA ((volatile uint8_t *)0x7F93u)
#define R01_APU ((volatile uint8_t *)0x7F40u)
#endif

#define R01_NOINLINE __attribute__((noinline))

void R01_NOINLINE r01_ppu_wait_vblank(void);
void r01_map_seek(uint32_t off);
uint8_t r01_map_read(void);
uint32_t r01_map_read_u24(void);
uint32_t r01_boot_u24(unsigned off);
void r01_oam_reset(void);
void r01_oam_write(uint8_t y, uint8_t tile, uint8_t attr, uint8_t x);
void r01_oam_boot_hide(void);
void r01_oam_hide_rest(uint8_t written);
void r01_player_hit_get(int *dx, int *dy, uint8_t *w, uint8_t *h);
void r01_world_cache_boot(void);
void r01_boot_copy_solids(void);
void r01_boot_map_stream(void);
void r01_irq_enable(void);
void r01_map_lock(void);
void r01_map_unlock(void);
void r01_vram_copy_map(void);
void r01_vram_fill_zero(void);
void r01_oam_commit(uint8_t written);
extern uint8_t r01_oam_scratch[256];
void r01_map_load_window(uint16_t cam_x, uint16_t cam_y);
void r01_tracker_boot(void);
void r01_tracker_nmi(void);
void r01_pa_boot(void);

/* NMI trampoline in asm/nmi.s calls this. */
void r01_nmi(void);

#endif
