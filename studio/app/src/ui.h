#ifndef RETR01_STUDIO_UI_H
#define RETR01_STUDIO_UI_H

#include "retr01_studio/project.h"

#include <SDL.h>

#define UI_LOGIC_W 640
#define UI_LOGIC_H 360
#define UI_LEFT_W 200
#define UI_LEFT_SCROLLBAR_W 4

/*
 * Left column: fixed-width viewport (full logical height). Panels stack in
 * scrollable content taller than the viewport — no per-panel vertical squeeze.
 * Coordinates below are content-local (y=0 at top of stack).
 */
#define UI_WORLDS_Y 0
#define UI_WORLDS_H 110
#define UI_PLANES_Y 110
#define UI_PLANES_H 40
#define UI_BG_Y 150
#define UI_BG_H 150
#define UI_SPR_Y 300
#define UI_SPR_H 90
#define UI_PAL_Y 390
#define UI_PAL_H 160
#define UI_LEFT_CONTENT_H (UI_PAL_Y + UI_PAL_H)

#define UI_WORLD_CELL 9 /* square screen markers in worlds grid */
#define UI_WORLD_GRID_X 8
#define UI_WORLD_GRID_Y 38

#define UI_MODE_PIXEL 0
#define UI_MODE_ATTR 1

typedef struct UiState {
    R01Project *project;
    char project_path[R01_PATH_MAX];
    char status[128];
    int scale; /* window integer scale */
    int fullscreen;
    int brush_down;
    int world_tab; /* UI 1..8 */
    int bg_bank_tab; /* 0..3 view */
    int screen_zoom; /* 1 or 2 for paint view */
    int screen_pan_x;
    int screen_pan_y;
    int left_scroll_y; /* content pixels scrolled down */
    int edit_mode;     /* UI_MODE_PIXEL / UI_MODE_ATTR */
    int attr_tx;
    int attr_ty;
    int pal_row_tab; /* 0..7 = BG0-3 + SPR0-3 */
    int pal_slot;    /* 0..3 within row */
} UiState;

int ui_init(UiState *ui);
void ui_shutdown(UiState *ui);
void ui_draw(UiState *ui, SDL_Renderer *r);
int ui_handle_event(UiState *ui, const SDL_Event *e, int logic_x, int logic_y);

#endif
