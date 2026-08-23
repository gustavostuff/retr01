#ifndef RETR01_SIM_TYPES_H
#define RETR01_SIM_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Logical UI canvas (integer-scaled window). */
#define R01S_LOGIC_W 1280
#define R01S_LOGIC_H 720

/* Scrollable board world (may exceed canvas). Tall enough for multi-row islands. */
#define R01S_BOARD_W 1600
#define R01S_BOARD_H 2400

/* Max pins on a single package we care about for now (40-pin DIP CPU). */
#define R01S_MAX_PINS 64

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
