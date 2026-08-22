#ifndef RETR01_STUDIO_CART_H
#define RETR01_STUDIO_CART_H

#include "retr01_studio/types.h"

#include <stddef.h>
#include <stdint.h>

/* Build in-memory .retr01 image (variable length, not padded). Caller frees *out. */
int r01_cart_build(const R01Project *p, uint8_t **out, size_t *out_len, char *err_buf, size_t err_cap);

/* Write .retr01 (packed image). */
int r01_cart_write(const R01Project *p, const char *path, char *err_buf, size_t err_cap);

/* Write 512 KB SST39SF040 burn image (.retr01 padded with 0xFF). */
int r01_cart_write_flash(const R01Project *p, const char *path, char *err_buf, size_t err_cap);

/* 64-byte Color PROM image (kit swatches quantized to R3G3B2). */
void r01_prom_fill(uint8_t out64[R01_MASTER_COLORS]);
int r01_prom_write(const char *path, char *err_buf, size_t err_cap);

/*
 * Emit readable ca65 boot stub + constraint equates (Phase 5 IR-lite).
 * Does not require cc65 at Studio build time; binary stub is embedded in cart.
 */
int r01_prg_write_asm(const R01Project *p, const char *path, char *err_buf, size_t err_cap);

/* Fill 32 KB PRG image (code at $8000, vectors at $FFFx). */
void r01_prg_fill_stub(uint8_t prg[R01_PRG_BYTES], const R01Project *p);

/* Export bundle: path_stem.retr01, path_stem_prom.bin, path_stem_boot.s, path_stem_flash.bin */
int r01_export_bundle(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap);

/* Validate header for tests / tooling. */
int r01_cart_peek_header(const uint8_t *img, size_t len, int *world_count, uint32_t *prg_off,
                         uint32_t *prg_len);

#endif
