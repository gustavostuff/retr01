#ifndef RETR01_UI_METRICS_H
#define RETR01_UI_METRICS_H

/* Shared chrome metrics for Retr01 UI widgets (8px grid). */

#define UI_COL_BG_R 34
#define UI_COL_BG_G 34
#define UI_COL_BG_B 38
#define UI_COL_PANEL_R 26
#define UI_COL_PANEL_G 26
#define UI_COL_PANEL_B 30
#define UI_COL_WELL_R 63
#define UI_COL_WELL_G 63
#define UI_COL_WELL_B 74
#define UI_COL_ACTIVE_R 45
#define UI_COL_ACTIVE_G 125
#define UI_COL_ACTIVE_B 70
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

#define UI_DOT_SIZE 8
#define UI_DOT_GAP 0
#define UI_DOT_STRIP_N 4

#define UI_TABS_MAX 16
#define UI_TABS_TAB_H 8
#define UI_TABS_STACK_H 16 /* inactive dual-view tab fill / active main+sub */
#define UI_TABS_SUB_W 16
#define UI_TABS_SUB_H 8
#define UI_TABS_DEFAULT_W 16 /* default tab width when caller passes 0 */

#define UI_MULTI_STATE_MAX 8

#define UI_PANEL_CELL_MIN 16
#define UI_PANEL_CELLS_MAX 64
#define UI_PANEL_TRACKS_MAX 32

#ifndef UI_PANEL_DEBUG_GRID
#define UI_PANEL_DEBUG_GRID 0
#endif

#endif
