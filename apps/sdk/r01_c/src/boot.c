#include "r01_engine.h"

/* Packer patches cart MAP offsets here (CPU $80E0). */
uint8_t r01_boot_map[9] __attribute__((section(".r01_bootmap"), used));

#ifndef R01_HOST_TEST
#define R01_CPU8(addr) (*(volatile uint8_t *)(uint16_t)(addr))

static uint32_t u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

void r01_boot_copy_solids(void) {
    uint8_t n = R01_CPU8(0x8700u);
    uint8_t i;
    uint8_t lim;
    R01_CPU8(0x0200u) = n;
    lim = (uint8_t)(n * 2u);
    for (i = 0; i < lim; i++) {
        R01_CPU8(0x0201u + i) = R01_CPU8(0x8701u + i);
    }
}

void r01_boot_map_stream(void) {
    uint16_t i;
    uint32_t off;

    off = u24(r01_boot_map);
    if (off == 0u && u24(r01_boot_map + 3) == 0u && u24(r01_boot_map + 6) == 0u) {
        return;
    }
    r01_map_seek(off);
    *R01_PAL_ROW = 0;
    for (i = 0; i < 16u; i++) {
        *R01_PAL_DATA = r01_map_read();
    }
    r01_map_seek(u24(r01_boot_map + 3));
    for (i = 0; i < 16u; i++) {
        *R01_PAL_DATA = r01_map_read();
    }
    r01_map_seek(u24(r01_boot_map + 6));
    *R01_VRAM_ADDR_LO = 0;
    *R01_VRAM_ADDR_HI = 0;
    for (i = 0; i < 480u; i++) {
        *R01_VRAM_DATA = r01_map_read();
    }
}
#else
void r01_boot_copy_solids(void) {
}
void r01_boot_map_stream(void) {
}
#endif
