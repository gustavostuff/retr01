#ifndef NETLIST_SIM_TYPES_H
#define NETLIST_SIM_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Logical UI canvas (integer-scaled window). */
#define NS_LOGIC_W 640
#define NS_LOGIC_H 360

/* Scrollable board world (may exceed canvas). */
#define NS_BOARD_W 2800
#define NS_BOARD_H 2400

/* Max pins on a single package (covers large DIPs). */
#define NS_MAX_PINS 64

/* Pin digital level (tri-state aware). */
typedef enum NsLevel {
    NS_LVL_Z = 0, /* hi-Z / not driving */
    NS_LVL_L = 1,
    NS_LVL_H = 2,
    NS_LVL_X = 3 /* unknown / bus fight */
} NsLevel;

typedef enum NsPinDir {
    NS_PIN_IN = 0,
    NS_PIN_OUT = 1,
    NS_PIN_IO = 2,
    NS_PIN_PWR = 3, /* VDD / VSS */
    NS_PIN_NC = 4
} NsPinDir;

#endif
