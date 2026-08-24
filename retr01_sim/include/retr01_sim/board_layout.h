#ifndef RETR01_SIM_BOARD_LAYOUT_H
#define RETR01_SIM_BOARD_LAYOUT_H

/* Island frame insets (must match ui.c clamp/draw). */
#define R01S_ISLAND_PAD_X 28
#define R01S_ISLAND_HEADER_H 28
#define R01S_ISLAND_PAD_TOP (R01S_ISLAND_HEADER_H + 20)
#define R01S_ISLAND_PAD_BOTTOM 28
#define R01S_CHIP_PIN_OUT 14

/* Board / island fill (matches workarea background in ui.c). */
#define R01S_BOARD_BG_R 18
#define R01S_BOARD_BG_G 42
#define R01S_BOARD_BG_B 28

/* Gap between chips inside an island, and between island frames. */
#define R01S_CHIP_GAP 28
#define R01S_ISLAND_GAP 32

/* Compact (PCB-like) packing: tighter gap, origin on the canvas. */
#define R01S_COMPACT_GAP 10
#define R01S_COMPACT_ORIGIN_X 24
#define R01S_COMPACT_ORIGIN_Y 24

/* Default wrap width for multi-row island packing (fits sim center viewport). */
#define R01S_ISLAND_ROW_MAX_W 780

/* UI: bottom-right resize grip. */
#define R01S_ISLAND_RESIZE_HANDLE 12
#define R01S_ISLAND_MIN_W 120
#define R01S_ISLAND_MIN_H 72

#endif
