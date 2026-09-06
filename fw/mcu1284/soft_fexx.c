#include "soft_fexx.h"

void r01_soft_fexx_init(R01SoftFexx *s) {
    if (!s) {
        return;
    }
    s->ppuctrl = 0;
    s->raster_ctrl = 0;
    s->bg0_x = 0;
    s->bg0_y = 0;
    s->pal_addr = 0;
    s->map_lo = 0;
    s->map_mid = 0;
    s->map_hi = 0;
    s->map_addr = 0;
    s->cart_a14_18 = 0;
    s->last_strobe = 0xFF;
}

void r01_soft_fexx_sync_map(R01SoftFexx *s) {
    if (!s) {
        return;
    }
    s->map_addr = ((uint32_t)s->map_hi << 16) | ((uint32_t)s->map_mid << 8) | s->map_lo;
    s->cart_a14_18 = (uint8_t)((s->map_addr >> 14) & 0x1Fu);
}

int r01_soft_fexx_write(R01SoftFexx *s, uint8_t port, uint8_t data) {
    if (!s) {
        return 0;
    }
    switch (port) {
    case 0x00:
        s->ppuctrl = data;
        break;
    case 0x05:
        s->raster_ctrl = data;
        break;
    case 0x06:
        s->bg0_x = data;
        break;
    case 0x07:
        s->bg0_y = data;
        break;
    case 0x08:
        s->pal_addr = (uint8_t)(data & 0x1Fu);
        break;
    case 0x90:
        s->map_lo = data;
        r01_soft_fexx_sync_map(s);
        break;
    case 0x91:
        s->map_mid = data;
        r01_soft_fexx_sync_map(s);
        break;
    case 0x92:
        s->map_hi = data;
        r01_soft_fexx_sync_map(s);
        break;
    default:
        return 0;
    }
    s->last_strobe = port;
    return 1;
}

uint8_t r01_soft_fexx_read(const R01SoftFexx *s, uint8_t port) {
    if (!s) {
        return 0;
    }
    switch (port) {
    case 0x00:
        return s->ppuctrl;
    case 0x05:
        return s->raster_ctrl;
    case 0x06:
        return s->bg0_x;
    case 0x07:
        return s->bg0_y;
    case 0x08:
        return s->pal_addr;
    case 0x90:
        return s->map_lo;
    case 0x91:
        return s->map_mid;
    case 0x92:
        return s->map_hi;
    default:
        return 0;
    }
}
