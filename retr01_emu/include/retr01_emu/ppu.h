#ifndef RETR01_EMU_PPU_H
#define RETR01_EMU_PPU_H

#include "retr01_emu/types.h"

#include <stdint.h>

struct R01eMachine;

typedef struct R01ePpu {
    uint8_t ctrl;    /* $FE00 */
    uint8_t status;  /* $FE01 */
    uint8_t scroll_x; /* $FE02 0..127 */
    uint8_t scroll_y; /* $FE03 0..119 */
    uint8_t raster_y; /* $FE04 */
    uint8_t raster_ctrl; /* $FE05 */
    uint8_t plane_lo; /* $FE06 */
    uint8_t plane_hi; /* $FE07 */

    uint8_t pal_addr; /* $FE08 */
    uint8_t pal[R01E_ACTIVE_PAL_BYTES];

    uint16_t vram_addr; /* $FE10/$FE11, 14-bit used */
    uint8_t vram[R01E_VRAM_BYTES];

    uint8_t oam_addr; /* $FE20 */
    uint8_t oam[256];

    uint8_t world; /* $FE30 */
    uint8_t bank_helper[8]; /* $FE31-$FE37 + $FE38 PAL_ROW */
    uint8_t pal_row; /* $FE38 */

    uint8_t apu[0x20]; /* $FE40-$FE5F stub */

    uint8_t pad0; /* $FE60 */
    uint8_t pad1; /* $FE61 */

    uint8_t eeprom[3]; /* $FE70-$FE72 stub */

    uint32_t map_addr; /* 24-bit $FE90-$FE92 */
    /* $FE93 reads cart[map_addr++] */

    /* Cached CHR for current world (BG0-3 then SPR0-3). */
    uint8_t chr[8][R01E_CHR_BANK_BYTES];
    int chr_loaded;

    /* NW corner of live 2x2 camera in world grid (docs/02). */
    int cam_origin_col;
    int cam_origin_row;
    /* World-space top-left of viewport (host atlas preview). */
    int cam_x;
    int cam_y;
    int cam_max_x;
    int cam_max_y;

    /* Beam */
    int dot_x;
    int dot_y;
    int frame;
    int nmi_line; /* 1 while NMI asserted this edge */

    /* RGB framebuffer: visible 256x240 (SCALE 2x of 128x120). */
    uint8_t fb[R01E_VISIBLE_W * R01E_VISIBLE_H * 3];
} R01ePpu;

void r01e_ppu_reset(R01ePpu *ppu);
uint8_t r01e_ppu_read(struct R01eMachine *m, uint16_t addr);
void r01e_ppu_write(struct R01eMachine *m, uint16_t addr, uint8_t v);

/* Soft-boot world assets into VRAM/CHR/pals (Studio stub never streams MAP). */
int r01e_ppu_boot_world(struct R01eMachine *m, int world);

/* Reload VRAM slots 0-3 from cart at cam_origin_*; set scroll from cam_x/y. */
int r01e_ppu_sync_camera(struct R01eMachine *m);

/*
 * Host atlas pan (Studio stub has no MAP streamer). dx/dy in logical pixels.
 * Shifts the 2x2 workbench when leaving the loaded window.
 */
int r01e_ppu_host_pan(struct R01eMachine *m, int dx, int dy);

/* Advance one dot; samples FB on visible dots when BG enable (ctrl bit0). */
void r01e_ppu_dot(struct R01eMachine *m);

/* Force full-frame BG composite (debug / catch-up). */
void r01e_ppu_render_frame(struct R01eMachine *m);

#endif
