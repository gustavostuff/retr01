#ifndef DISCRETE_IC_BOARD_LAYOUT_H
#define DISCRETE_IC_BOARD_LAYOUT_H

/* Universal board grid (islands + chips). */
#define NS_GRID 5

/* Island frame insets (must match viz clamp/draw). */
#define NS_ISLAND_PAD_X 5
#define NS_ISLAND_HEADER_H 11
#define NS_ISLAND_PAD_TOP (NS_ISLAND_HEADER_H + NS_GRID)
#define NS_ISLAND_PAD_BOTTOM 5
#define NS_CHIP_PIN_OUT 5
#define NS_CHIP_PIN_THICK 3

/* Board / island fill. */
#define NS_BOARD_BG_R 22
#define NS_BOARD_BG_G 50
#define NS_BOARD_BG_B 34

/* Island chrome when OK. */
#define NS_ISLAND_OK_R 27
#define NS_ISLAND_OK_G 80
#define NS_ISLAND_OK_B 50

/* Gap between chips inside an island, and between island frames. */
#define NS_CHIP_GAP 5
#define NS_ISLAND_GAP 5

/* Compact packing. */
#define NS_COMPACT_GAP NS_GRID
#define NS_COMPACT_ORIGIN_X NS_GRID
#define NS_COMPACT_ORIGIN_Y NS_GRID

/* Default wrap width for multi-row island packing. */
#define NS_ISLAND_ROW_MAX_W 520

/* UI: corner resize grips. */
#define NS_ISLAND_RESIZE_HANDLE 5
#define NS_ISLAND_CORNER_BR 0
#define NS_ISLAND_CORNER_BL 1
#define NS_ISLAND_CORNER_TR 2
#define NS_ISLAND_CORNER_TL 3
#define NS_ISLAND_MIN_W 60
#define NS_ISLAND_MIN_H 40

static inline int ns_snap5(int v) {
    if (v >= 0) {
        return ((v + 2) / 5) * 5;
    }
    return -(((-v + 2) / 5) * 5);
}

static inline int ns_snap5_up(int v) {
    if (v <= 0) {
        return 5;
    }
    return ((v + 4) / 5) * 5;
}

static inline int ns_grid_snap(int v) {
    if (v >= 0) {
        return (v / NS_GRID) * NS_GRID;
    }
    return -(((-v) / NS_GRID) * NS_GRID);
}

static inline int ns_grid_snap_up(int v) {
    if (v <= 0) {
        return NS_GRID;
    }
    return ((v + NS_GRID - 1) / NS_GRID) * NS_GRID;
}

#endif
