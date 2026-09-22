#include "r01_hw.h"

void r01_ppu_wait_vblank(void) {
    while ((*R01_PPUSTATUS & R01_PPUSTATUS_VBLANK) == 0u) {
    }
}

void r01_map_seek(uint32_t off) {
    *R01_MAP_LO = (uint8_t)(off & 0xFFu);
    *R01_MAP_MID = (uint8_t)((off >> 8) & 0xFFu);
    *R01_MAP_HI = (uint8_t)((off >> 16) & 0xFFu);
}

uint8_t r01_map_read(void) {
    return *R01_MAP_DATA;
}

void r01_oam_reset(void) {
    *R01_OAM_ADDR = 0;
}

void r01_oam_write(uint8_t y, uint8_t tile, uint8_t attr, uint8_t x) {
    *R01_OAM_DATA = y;
    *R01_OAM_DATA = tile;
    *R01_OAM_DATA = attr;
    *R01_OAM_DATA = x;
}
