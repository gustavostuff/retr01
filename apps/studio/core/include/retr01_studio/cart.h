#ifndef retr01_STUDIO_CART_H
#define retr01_STUDIO_CART_H

#include "retr01_studio/types.h"

/* Play player: type index, or 0xFF = CHR stub. Hitbox snapshot from that type's state 0 frame 0. */
#define R01_CART_ENTITY_FRAME_DELAY_DEFAULT 1

void r01_prom_fill(uint8_t out64[R01_MASTER_COLORS]);
int r01_prom_write(const char *path, char *err_buf, size_t err_cap);
int r01_prg_write_asm(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_cart_write(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_cart_write_flash(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_export_bundle(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap);

#endif
