#ifndef retr01_SIM_BOARD_LAYOUT_H
#define retr01_SIM_BOARD_LAYOUT_H

/* Universal board grid (islands + chips, island and compact modes). */
#define R01S_GRID 5

/* Island frame insets (must match ui.c clamp/draw). */
#define R01S_ISLAND_PAD_X 5
#define R01S_ISLAND_HEADER_H 11
#define R01S_ISLAND_PAD_TOP (R01S_ISLAND_HEADER_H + R01S_GRID)
#define R01S_ISLAND_PAD_BOTTOM 5
#define R01S_CHIP_PIN_OUT 5
#define R01S_CHIP_PIN_THICK 3

/* Board / island fill (matches workarea background in ui.c). */
#define R01S_BOARD_BG_R 22
#define R01S_BOARD_BG_G 50
#define R01S_BOARD_BG_B 34

/* Island chrome (#1b5032 base when OK). */
#define R01S_ISLAND_OK_R 27
#define R01S_ISLAND_OK_G 80
#define R01S_ISLAND_OK_B 50

/* Gap between chips inside an island, and between island frames. */
#define R01S_CHIP_GAP 5
#define R01S_ISLAND_GAP 5

/* Compact (PCB-like) packing: same grid/gap, origin on the canvas. */
#define R01S_COMPACT_GAP R01S_GRID
#define R01S_COMPACT_ORIGIN_X R01S_GRID
#define R01S_COMPACT_ORIGIN_Y R01S_GRID

/* Default wrap width for multi-row island packing (fits sim center viewport). */
#define R01S_ISLAND_ROW_MAX_W 520

/* UI: corner resize grips. */
#define R01S_ISLAND_RESIZE_HANDLE 5
#define R01S_ISLAND_CORNER_BR 0
#define R01S_ISLAND_CORNER_BL 1
#define R01S_ISLAND_CORNER_TR 2
#define R01S_ISLAND_CORNER_TL 3
#define R01S_ISLAND_MIN_W 60
#define R01S_ISLAND_MIN_H 40

static inline int r01s_snap5(int v) {
    if (v >= 0) {
        return ((v + 2) / 5) * 5;
    }
    return -(((-v + 2) / 5) * 5);
}

static inline int r01s_snap5_up(int v) {
    if (v <= 0) {
        return 5;
    }
    return ((v + 4) / 5) * 5;
}

static inline int r01s_grid_snap(int v) {
    if (v >= 0) {
        return (v / R01S_GRID) * R01S_GRID;
    }
    return -(((-v) / R01S_GRID) * R01S_GRID);
}

static inline int r01s_grid_snap_up(int v) {
    if (v <= 0) {
        return R01S_GRID;
    }
    return ((v + R01S_GRID - 1) / R01S_GRID) * R01S_GRID;
}

#endif
