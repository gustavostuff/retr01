#ifndef RETR01_STUDIO_UI_H
#define RETR01_STUDIO_UI_H

#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <SDL.h>

#define UI_LOGIC_W 640
#define UI_LOGIC_H 360
#define UI_LEFT_W 200
#define UI_LEFT_SCROLLBAR_W 4

#define UI_WORLDS_Y 0
#define UI_WORLDS_H 110
#define UI_PLANES_Y 110
#define UI_PLANES_H 40
#define UI_BG_Y 150
#define UI_BG_H 150
#define UI_SPR_Y 300
#define UI_SPR_H 150
#define UI_PAL_Y 450
#define UI_PAL_H 160
#define UI_CONSTRAINTS_Y 610
#define UI_CONSTRAINTS_H 200
#define UI_LEFT_CONTENT_H (UI_CONSTRAINTS_Y + UI_CONSTRAINTS_H)

#define UI_WORLD_CELL 9
#define UI_WORLD_GRID_X 8
#define UI_WORLD_GRID_Y 38

#define UI_MODE_PIXEL 0
#define UI_MODE_ATTR 1

#define UI_LAYER_BG 0
#define UI_LAYER_SPR 1

#define UI_SPR_TOOL_PLACE 0
#define UI_SPR_TOOL_TILE 1

#define UI_TOAST_MAX 96
#define UI_TOAST_MS 2800

typedef struct UiState {
    R01Project *project;
    R01PlayState play;
    char project_path[R01_PATH_MAX];
    char status[128];
    char toast_text[UI_TOAST_MAX];
    Uint32 toast_until;
    int toast_error; /* 1 = error style */
    int scale;
    int fullscreen;
    int brush_down;
    int world_tab;
    int bg_bank_tab;
    int spr_bank_tab;
    int spr_tile;
    int spr_tool; /* place vs paint tile */
    int spr_size16; /* default SIZE for new OAM */
    int oam_sel;
    int meta_sel;
    int layer; /* BG or SPR */
    int screen_zoom;
    int screen_pan_x;
    int screen_pan_y;
    int left_scroll_y;
    int edit_mode;
    int attr_tx;
    int attr_ty;
    int pal_row_tab;
    int pal_slot;
    int show_grid;
    Uint32 play_last_tick;
} UiState;

int ui_init(UiState *ui);
void ui_shutdown(UiState *ui);
void ui_tick(UiState *ui);
void ui_draw(UiState *ui, SDL_Renderer *r);
int ui_handle_event(UiState *ui, const SDL_Event *e, int logic_x, int logic_y);

/* Drop / import helpers (Worlds panel). */
void ui_toast(UiState *ui, const char *msg, int is_error);
int ui_try_import_png(UiState *ui, const char *path);
int ui_handle_drop_file(UiState *ui, const char *path, int logic_x, int logic_y);

#endif
