#ifndef RETR01_SIM_TYPES_H
#define RETR01_SIM_TYPES_H

#include "discrete_ic/types.h"
#include "r01_cart_caps.h"

#include <stddef.h>
#include <stdint.h>

/* Engine levels/dirs come from discrete_ic (see ns_compat.h for R01s* aliases). */

/* Cart image caps follow docs/general/memory.md via r01_cart_caps.h. */
#define R01S_CART_FORMAT_VER R01_CART_FORMAT_VER
#define R01S_CART_HDR_BYTES R01_CART_HDR_BYTES
#define R01S_CART_PTR_TABLE_BYTES R01_CART_PTR_TABLE_BYTES
#define R01S_CART_OTHER_MAX 48
#define R01S_CART_OTHER_CREDITS_FIRST 2
#define R01S_CART_CREDITS_MIN 0
#define R01S_CART_CREDITS_MAX (R01S_CART_OTHER_MAX - R01S_CART_OTHER_CREDITS_FIRST) /* 46 */
#define R01S_CART_OTHER_HDR_BYTES 4u
#define R01S_CART_OTHER_DIR_BYTES 8u
#define R01S_CART_OTHER_FLAG_RLE 0x01u
#define R01S_CART_SCREEN_PAYLOAD 480u

#define R01S_MAX_WORLDS 8
#define R01S_GRID_MAX 16
#define R01S_MAX_PRESENT_SCREENS 48
#define R01S_BG0_SCREENS_MAX 8
#define R01S_CELL_PACK(col, row) ((uint8_t)(((unsigned)(col)&0x0fu) | (((unsigned)(row)&0x0fu) << 4)))
#define R01S_CELL_COL(b) ((int)((unsigned)(b)&0x0fu))
#define R01S_CELL_ROW(b) ((int)(((unsigned)(b) >> 4) & 0x0fu))
#define R01S_CART_PRG_BYTES 0x8000u

#endif
