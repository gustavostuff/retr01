#ifndef RETR01_EMU_PPU_H
#define RETR01_EMU_PPU_H

#include "retr01/palette.h"
#include "retr01/screen.h"
#include "retr01/types.h"

#define RETR01_FB_W 256
#define RETR01_FB_H 240

typedef struct retr01_ppu {
    retr01_screen_t screen;
    retr01_master_palette_t palette;
    const uint8_t *chr;
    size_t chr_size;
    uint8_t scroll_x;
    uint8_t scroll_y;
    uint8_t world;
    uint8_t bg_bank;
    uint8_t spr_bank;
} retr01_ppu_t;

void retr01_ppu_init(retr01_ppu_t *ppu);
void retr01_ppu_render_bg(const retr01_ppu_t *ppu, uint8_t *rgba_out);

#endif
