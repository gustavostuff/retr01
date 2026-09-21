#ifndef RETR01_UI_METRICS_H
#define RETR01_UI_METRICS_H

/* Shared chrome metrics for Retr01 UI widgets (8px grid).
 *
 * Studio UI palette — surfaces, type, then accents. Keep new chrome on these.
 *
 * Surfaces (three dark grays):
 *   PANEL   26,26,30  sidebars / top chrome
 *   BG      34,34,38  window, modal panels, main pane
 *   WELL    63,63,74  inactive buttons, wells, open accordion headers
 *
 * Type:
 *   TEXT      240,240,240  labels and button glyphs
 *   TEXT_DIM  120,120,130  disabled / play-locked chrome
 *
 * Accents:
 *   ACTIVE    45,125,70    green — primary buttons, selected tabs
 *   DANGER    168,48,52    red — Stop / destructive
 *   PRESENT   55,130,220   blue — present-screen marks (world map)
 *   MARK      245,245,245  white — play-screen mark
 *
 * Checker (tile transparency):
 *   CHESS_A   58,58,66
 *   CHESS_B   50,50,58
 */

#define UI_COL_BG_R 34
#define UI_COL_BG_G 34
#define UI_COL_BG_B 38
#define UI_COL_PANEL_R 26
#define UI_COL_PANEL_G 26
#define UI_COL_PANEL_B 30
#define UI_COL_WELL_R 63
#define UI_COL_WELL_G 63
#define UI_COL_WELL_B 74
#define UI_COL_TEXT_R 240
#define UI_COL_TEXT_G 240
#define UI_COL_TEXT_B 240
#define UI_COL_TEXT_DIM_R 120
#define UI_COL_TEXT_DIM_G 120
#define UI_COL_TEXT_DIM_B 130
#define UI_COL_ACTIVE_R 45
#define UI_COL_ACTIVE_G 125
#define UI_COL_ACTIVE_B 70
#define UI_COL_DANGER_R 168
#define UI_COL_DANGER_G 48
#define UI_COL_DANGER_B 52
#define UI_COL_PRESENT_R 55
#define UI_COL_PRESENT_G 130
#define UI_COL_PRESENT_B 220
#define UI_COL_MARK_R 245
#define UI_COL_MARK_G 245
#define UI_COL_MARK_B 245
#define UI_COL_CHESS_A_R 58
#define UI_COL_CHESS_A_G 58
#define UI_COL_CHESS_A_B 66
#define UI_COL_CHESS_B_R 50
#define UI_COL_CHESS_B_G 50
#define UI_COL_CHESS_B_B 58

#define UI_UNIT 8
#define UI_BTN_H 16

/* Radio / checkbox glyph is 8x8; 8px pad on each horizontal side (glyph at dx+8). */
#define UI_TOGGLE_PAD_X 8
#define UI_TOGGLE_GLYPH 8
#define UI_TOGGLE_W (UI_TOGGLE_PAD_X + UI_TOGGLE_GLYPH + UI_TOGGLE_PAD_X)
static inline int ui_toggle_glyph_x(int dx) {
    return dx + UI_TOGGLE_PAD_X;
}
static inline int ui_toggle_label_x(int dx) {
    return dx + UI_TOGGLE_W;
}

#define UI_DOT_SIZE 8
#define UI_DOT_GAP 0
#define UI_DOT_STRIP_N 4

#define UI_TABS_MAX 16
#define UI_TABS_TAB_H 8
#define UI_TABS_STACK_H 16 /* inactive dual-view tab fill / active main+sub */
#define UI_TABS_SUB_W 16
#define UI_TABS_SUB_H 8
#define UI_TABS_DEFAULT_W 16 /* default tab width when caller passes 0 */

#define UI_TAB_PAGER_H UI_BTN_H
#define UI_TAB_PAGER_BTN_W (UI_UNIT * 2)
#define UI_TAB_PAGER_LABEL_W (UI_UNIT * 4)
#define UI_TAB_PAGER_PLANE_W (UI_UNIT * 8)
#define UI_TAB_PAGER_HIT_NONE 0
#define UI_TAB_PAGER_HIT_PREV 1
#define UI_TAB_PAGER_HIT_NEXT 2

#define UI_MULTI_STATE_MAX 8

#define UI_PANEL_CELL_MIN 16
#define UI_PANEL_CELLS_MAX 64
#define UI_PANEL_TRACKS_MAX 32

#ifndef UI_PANEL_DEBUG_GRID
#define UI_PANEL_DEBUG_GRID 0
#endif

#endif
