#ifndef retr01_SIM_TYPES_H
#define retr01_SIM_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Logical UI canvas (integer-scaled window). */
#define R01S_LOGIC_W 640
#define R01S_LOGIC_H 360

/* Scrollable board world (may exceed canvas). Tall enough for multi-row islands. */
#define R01S_BOARD_W 2800
#define R01S_BOARD_H 2400

/* Max pins on a single package we care about for now (40-pin DIP CPU). */
#define R01S_MAX_PINS 64

#define R01S_CART_FORMAT_VER 2
#define R01S_CART_HDR_BYTES 16u
#define R01S_CART_PTR_TABLE_BYTES 36u
#define R01S_CART_OTHER_MAX 48
#define R01S_CART_OTHER_CREDITS_FIRST 2
#define R01S_CART_CREDITS_MIN 0
#define R01S_CART_CREDITS_MAX (R01S_CART_OTHER_MAX - R01S_CART_OTHER_CREDITS_FIRST) /* 46 */
#define R01S_CART_OTHER_HDR_BYTES 4u
#define R01S_CART_OTHER_DIR_BYTES 8u
#define R01S_CART_OTHER_FLAG_RLE 0x01u
#define R01S_CART_SCREEN_PAYLOAD 480u

/* Cart layout caps (docs/graphics.md). */
#define R01S_MAX_WORLDS 8
#define R01S_MAX_PRESENT_SCREENS 32 /* 8 worlds x 32 present screens */
#define R01S_BG0_SCREENS_MAX 8 /* L0 / structured second BG (replaces old parallax) */
#define R01S_CART_PRG_BYTES 0x8000u /* fixed 32 KB PRG */

/* Pin digital level (tri-state aware). */
typedef enum R01sLevel {
    R01S_LVL_Z = 0, /* hi-Z / not driving */
    R01S_LVL_L = 1,
    R01S_LVL_H = 2,
    R01S_LVL_X = 3  /* unknown / bus fight */
} R01sLevel;

typedef enum R01sPinDir {
    R01S_PIN_IN = 0,
    R01S_PIN_OUT = 1,
    R01S_PIN_IO = 2,
    R01S_PIN_PWR = 3, /* VDD / VSS */
    R01S_PIN_NC = 4
} R01sPinDir;

#endif
