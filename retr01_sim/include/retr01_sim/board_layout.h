#ifndef RETR01_SIM_BOARD_LAYOUT_H
#define RETR01_SIM_BOARD_LAYOUT_H

/* Island frame insets (must match ui.c clamp/draw). */
#define R01S_ISLAND_PAD_X 16
#define R01S_ISLAND_HEADER_H 28
#define R01S_ISLAND_PAD_TOP (R01S_ISLAND_HEADER_H + 8)
#define R01S_ISLAND_PAD_BOTTOM 16
#define R01S_CHIP_PIN_OUT 12

/* Gap between chips inside an island, and between island frames. */
#define R01S_CHIP_GAP 28
#define R01S_ISLAND_GAP 32

/* Default wrap width for multi-row island packing (fits sim center viewport). */
#define R01S_ISLAND_ROW_MAX_W 780

/* UI: bottom-right resize grip. */
#define R01S_ISLAND_RESIZE_HANDLE 12
#define R01S_ISLAND_MIN_W 96
#define R01S_ISLAND_MIN_H 56

#endif
