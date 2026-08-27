#ifndef retr01_STUDIO_UI_H
#define retr01_STUDIO_UI_H

#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <SDL.h>

#define UI_LOGIC_W 640
#define UI_LOGIC_H 360

/* Phase 2 chrome: dark / darker gray, 8px grid. */
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
#define UI_SIDEBAR_W 128
#define UI_WORLD_BTN 16
#define UI_WORLD_CELL 16
#define UI_WORLD_VIEW 128
#define UI_PAL_SWATCH 8
#define UI_PAL_LABEL_W 32

#define UI_WORLDS_X 0
#define UI_WORLDS_Y 0
#define UI_WORLD_BTNS_Y (UI_WORLDS_Y + UI_BTN_H)
#define UI_WORLD_VIEW_Y (UI_WORLD_BTNS_Y + UI_WORLD_BTN)

#define UI_MAIN_W (UI_LOGIC_W - UI_SIDEBAR_W)
#define UI_SCREEN_SCALE 2
#define UI_SCREEN_W (R01_SCREEN_PX_W * UI_SCREEN_SCALE)
#define UI_SCREEN_H (R01_SCREEN_PX_H * UI_SCREEN_SCALE)
#define UI_SCREEN_X (UI_SIDEBAR_W + (UI_MAIN_W - UI_SCREEN_W) / 2)
#define UI_SCREEN_Y ((UI_LOGIC_H - UI_SCREEN_H) / 2)

#define UI_MODAL_W 288
#define UI_MODAL_H 160
#define UI_TILE_CANVAS 128

#define UI_TOAST_MS 2800

#define UI_MENU_MAX 8
#define UI_MENU_SUB_NONE 0
#define UI_MENU_SUB_BANK 1
#define UI_MENU_SUB_PAL 2

typedef struct UiMenu {
    int open;
    int submenu;
    int x, y;
    int item_count;
    char items[UI_MENU_MAX][24];
    int screen_tx, screen_ty; /* tile under cursor when opened */
} UiMenu;

typedef struct UiTileEdit {
    int open;
    int pal;   /* 0..3 within active pal row */
    int color; /* 0..3 within pal */
    int tile_id;
    int bank;
    int flip_h;
    int flip_v;
    int is_new;
    uint8_t chr[R01_TILE_BYTES];
    int paint_tx, paint_ty; /* screen tile that opened the editor (-1 if none) */
} UiTileEdit;

typedef struct UiBrush {
    int armed; /* after Save in tile editor */
    int bank;
    int tile_id;
    int pal;
    int flip_h;
    int flip_v;
    uint8_t chr[R01_TILE_BYTES];
} UiBrush;

typedef struct UiState {
    R01Project *project;
    R01PlayState play;
    char project_path[R01_PATH_MAX];
    char toast[96];
    Uint32 toast_until;
    int toast_error;
    int scale;
    Uint32 play_last_tick;
    Uint8 keys[512];
    int mouse_x;
    int mouse_y;
    UiMenu menu;
    UiTileEdit tile_edit;
    UiBrush brush;
    int sel_tx;
    int sel_ty;
    Uint32 last_click_ms;
    int last_click_col;
    int last_click_row;
} UiState;

int ui_init(UiState *ui);
void ui_shutdown(UiState *ui);
void ui_tick(UiState *ui);
void ui_draw(UiState *ui, SDL_Renderer *r);
int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly);
int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly);

#endif
