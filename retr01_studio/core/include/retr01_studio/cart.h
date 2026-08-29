#ifndef retr01_STUDIO_CART_H
#define retr01_STUDIO_CART_H

#include "retr01_studio/types.h"

/* Phase 3D: entity tables in world header reserved region (relative to world base). */
#define R01_CART_WHDR_TYPE_COUNT 17
#define R01_CART_WHDR_INST_COUNT 18
#define R01_CART_WHDR_OFF_TYPES 19
#define R01_CART_WHDR_OFF_INSTS 22
#define R01_CART_ENTITY_PARTS_MAX 4
#define R01_CART_ENTITY_TYPE_SIZE 20 /* origin_x,y + part_count + pad + 4x(tile,attr,dx,dy) */
#define R01_CART_INSTANCE_SIZE 6     /* type_id, pad, world_x u16 LE, world_y u16 LE */

void r01_prom_fill(uint8_t out64[R01_MASTER_COLORS]);
int r01_prom_write(const char *path, char *err_buf, size_t err_cap);
int r01_prg_write_asm(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_cart_write(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_cart_write_flash(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_export_bundle(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap);

#endif
