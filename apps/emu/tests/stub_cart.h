#ifndef R01E_TEST_STUB_CART_H
#define R01E_TEST_STUB_CART_H

#include "retr01_emu/types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Minimal in-memory cart for unit tests (no external ROM / project file). */
enum {
    R01E_STUB_HDR = 16,
    R01E_STUB_PTR = (int)R01E_CART_PTR_TABLE_BYTES,
    R01E_STUB_PAL = 128,
    R01E_STUB_PRG_OFF = R01E_STUB_HDR + R01E_STUB_PTR + 2 * R01E_STUB_PAL,
    R01E_STUB_CART_LEN = R01E_STUB_PRG_OFF + (int)R01E_PRG_BYTES
};

static void stub_put_u24(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
}

static void stub_put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

/* Fills out_len bytes into out. Returns 0 on success. */
static int r01e_test_stub_cart(uint8_t *out, size_t out_len) {
    uint8_t *ptrs;
    uint8_t *prg;
    if (!out || out_len < (size_t)R01E_STUB_CART_LEN) {
        return -1;
    }
    memset(out, 0, (size_t)R01E_STUB_CART_LEN);
    memcpy(out, R01E_CART_MAGIC, 6);
    out[6] = R01E_CART_FORMAT_VER;
    out[7] = 0; /* no worlds -- unit stub */
    ptrs = out + R01E_STUB_HDR;
    stub_put_u24(ptrs + 0, (uint32_t)R01E_STUB_PRG_OFF);
    stub_put_u24(ptrs + 3, R01E_PRG_BYTES);
    stub_put_u24(ptrs + 6, (uint32_t)(R01E_STUB_HDR + R01E_STUB_PTR));
    stub_put_u24(ptrs + 9, (uint32_t)R01E_STUB_PAL);
    stub_put_u24(ptrs + 12, (uint32_t)(R01E_STUB_HDR + R01E_STUB_PTR + R01E_STUB_PAL));
    stub_put_u24(ptrs + 15, (uint32_t)R01E_STUB_PAL);
    /* world/other left 0 */

    prg = out + R01E_STUB_PRG_OFF;
    memset(prg, 0xEA, R01E_PRG_BYTES);
    prg[0] = 0x78; /* SEI */
    prg[1] = 0x4C; /* JMP $8000 */
    prg[2] = 0x00;
    prg[3] = 0x80;
    stub_put_u16(prg + 0x7FFC, 0x8000u); /* RESET */
    stub_put_u16(prg + 0x7FFA, 0x8000u); /* NMI */
    stub_put_u16(prg + 0x7FFE, 0x8000u); /* IRQ */
    return 0;
}

#endif
