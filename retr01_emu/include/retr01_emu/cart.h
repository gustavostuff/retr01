#ifndef retr01_EMU_CART_H
#define retr01_EMU_CART_H

#include "retr01_emu/types.h"

#include <stddef.h>
#include <stdint.h>

typedef struct R01eCart {
    uint8_t *data;
    size_t len;
    uint8_t format_ver;
    uint8_t world_count;
    uint8_t flags;
    uint32_t off_prg;
    uint32_t len_prg;
    uint32_t off_pal_bg;
    uint32_t len_pal_bg;
    uint32_t off_pal_spr;
    uint32_t len_pal_spr;
    uint32_t off_world_table;
} R01eCart;

typedef struct R01eWorldView {
    int present;
    uint32_t base; /* absolute offset of world blob in cart */
    uint32_t len;
    uint8_t start_col;
    uint8_t start_row;
    uint8_t default_bg_bank;
    uint8_t default_pal_row;
    uint8_t screen_count;
    uint8_t parallax_count;
    uint32_t off_chr; /* relative to world base */
    uint32_t off_screen_dir;
    uint32_t off_parallax_dir;
} R01eWorldView;

/* Load packed .retr01 (or 512 KB flash image). Owns *out->data. */
int r01e_cart_load_path(R01eCart *out, const char *path, char *err, size_t err_cap);
int r01e_cart_load_mem(R01eCart *out, const uint8_t *img, size_t len, char *err, size_t err_cap);
void r01e_cart_free(R01eCart *c);

const uint8_t *r01e_cart_prg(const R01eCart *c);
int r01e_cart_world(const R01eCart *c, int index, R01eWorldView *out);

/* Return 1 if world has a screen at grid col,row. */
int r01e_cart_has_screen(const R01eCart *c, int world, int col, int row);

/* Absolute byte in cart image, or NULL if OOB. */
const uint8_t *r01e_cart_ptr(const R01eCart *c, uint32_t abs_off, size_t need);

uint8_t r01e_cart_read(const R01eCart *c, uint32_t abs_off);

#endif
