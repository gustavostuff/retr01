#ifndef retr01_STUDIO_UI_H
#define retr01_STUDIO_UI_H

#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <SDL.h>

#define UI_LOGIC_W 640
#define UI_LOGIC_H 360
#define UI_LEFT_W 200

#define UI_WORLD_GRID_X 24
#define UI_WORLD_GRID_Y 48
#define UI_WORLD_CELL 28

#define UI_SCREEN_X 220
#define UI_SCREEN_Y 48
#define UI_SCREEN_VIEW_W 400
#define UI_SCREEN_VIEW_H 280

#define UI_TOAST_MS 2800

#define UI_PLAY_BTN_X 556
#define UI_PLAY_BTN_Y 3
#define UI_PLAY_BTN_W 76
#define UI_PLAY_BTN_H 14

/* Active palette strip (bottom-right): 4 pals × 4 colors. */
#define UI_PAL_SWATCH 6
#define UI_PAL_GAP 2
#define UI_PAL_GROUP_GAP 4
#define UI_PAL_STRIP_W                                                                                     \
    (4 * (4 * UI_PAL_SWATCH + 3 * 1) + 3 * UI_PAL_GROUP_GAP)
#define UI_PAL_LABEL_W 22
#define UI_PAL_PANEL_W (UI_PAL_LABEL_W + UI_PAL_STRIP_W)
#define UI_PAL_PANEL_X (UI_LOGIC_W - UI_PAL_PANEL_W - 8)
#define UI_PAL_PANEL_Y (UI_LOGIC_H - 36)

typedef struct UiState {
    R01Project *project;
    R01PlayState play;
    char project_path[R01_PATH_MAX];
    char status[128];
    char toast[96];
    Uint32 toast_until;
    int toast_error;
    int scale;
    Uint32 play_last_tick;
    Uint8 keys[512];
} UiState;

int ui_init(UiState *ui);
void ui_shutdown(UiState *ui);
void ui_tick(UiState *ui);
void ui_draw(UiState *ui, SDL_Renderer *r);
int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly);
int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly);

#endif
