#include "r01_hw.h"

static uint8_t s_irqs;

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

uint32_t r01_map_read_u24(void) {
    uint32_t v = r01_map_read();
    v |= (uint32_t)r01_map_read() << 8;
    v |= (uint32_t)r01_map_read() << 16;
    return v;
}

static uint8_t s_oam_last;

void r01_oam_reset(void) {
    *R01_OAM_ADDR = 0;
}

void r01_oam_write(uint8_t y, uint8_t tile, uint8_t attr, uint8_t x) {
    *R01_OAM_DATA = y;
    *R01_OAM_DATA = tile;
    *R01_OAM_DATA = attr;
    *R01_OAM_DATA = x;
}

void r01_oam_boot_hide(void) {
    uint8_t i;
    r01_oam_reset();
    for (i = 0; i < 64u; i++) {
        r01_oam_write(0xF0u, 0, 0, 0);
    }
    s_oam_last = 0;
}

void r01_oam_hide_rest(uint8_t written) {
    uint8_t i;
    uint8_t end = s_oam_last;
    if (written > end) {
        end = written;
    }
    for (i = written; i < end; i++) {
        r01_oam_write(0xF0u, 0, 0, 0);
    }
    s_oam_last = written;
}

void r01_irq_enable(void) {
#ifndef R01_HOST_TEST
    *R01_PPUCTRL = (uint8_t)(R01_PPUCTRL_BOOT | R01_PPUCTRL_NMI_EN);
    s_irqs = 1;
    __asm__ volatile("cli");
#else
    s_irqs = 1;
#endif
}

void r01_map_lock(void) {
#ifndef R01_HOST_TEST
    if (s_irqs) {
        __asm__ volatile("sei");
    }
#else
    (void)s_irqs;
#endif
}

void r01_map_unlock(void) {
#ifndef R01_HOST_TEST
    if (s_irqs) {
        __asm__ volatile("cli");
    }
#endif
}
