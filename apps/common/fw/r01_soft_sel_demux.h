#ifndef R01_SOFT_SEL_DEMUX_H
#define R01_SOFT_SEL_DEMUX_H

#include <stdint.h>

/*
 * Soft $7Fxx port families for MCU-M SEL_SOFT* demux.
 * SoT: docs/general/hardware.md (Compositor packs families; FW demux with A[7:0]).
 * Port = CPU A[7:0] of the $7Fxx access (low byte of $7Fxx).
 */

static inline int r01_soft_port_in_soft0(uint8_t port) {
    return (port == 0x00u || port == 0x06u || port == 0x07u) ? 1 : 0;
}

static inline int r01_soft_port_in_soft1(uint8_t port) {
    return (port == 0x05u || port == 0x08u || port == 0x09u) ? 1 : 0;
}

static inline int r01_soft_port_in_soft2(uint8_t port) {
    if (port >= 0x40u && port <= 0x5Fu) {
        return 1;
    }
    switch (port) {
    case 0x20u:
    case 0x21u:
    case 0x22u:
    case 0x23u:
    case 0x24u:
    case 0x70u:
    case 0x71u:
    case 0x72u:
    case 0x90u:
    case 0x91u:
    case 0x92u:
    case 0x93u:
        return 1;
    default:
        return 0;
    }
}

/* sel_bit: 0=SOFT0, 1=SOFT1, 2=SOFT2. Returns port or 0xFF if A not in family. */
static inline uint8_t r01_soft_sel_demux(uint8_t sel_bit, uint8_t a_lo) {
    if (sel_bit == 0u && r01_soft_port_in_soft0(a_lo)) {
        return a_lo;
    }
    if (sel_bit == 1u && r01_soft_port_in_soft1(a_lo)) {
        return a_lo;
    }
    if (sel_bit == 2u && r01_soft_port_in_soft2(a_lo)) {
        return a_lo;
    }
    return 0xFFu;
}

#endif
