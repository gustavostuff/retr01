#ifndef RETR01_SIM_UI_H
#define RETR01_SIM_UI_H

#include "retr01_sim/entity.h"
#include "retr01_sim/traces.h"
#include "retr01_sim/types.h"

#include <SDL.h>

#define R01S_BOARD_MAX_CHIPS 32

typedef struct R01sUi {
    R01sEntity *chips[R01S_BOARD_MAX_CHIPS];
    int chip_count;
    int selected; /* index or -1 */
    int pan_x;
    int pan_y;
    int drag_pan; /* middle/right button pan */
    int drag_last_x;
    int drag_last_y;
    char status[192];
    int probe_vdd;
    int probe_phi2;
    int probe_resb_low;
    R01sTraceMap traces;
} R01sUi;

int r01s_ui_init(R01sUi *ui);
void r01s_ui_shutdown(R01sUi *ui);

int r01s_ui_add_chip(R01sUi *ui, R01sEntity *chip);

void r01s_ui_clamp_pan(R01sUi *ui);
void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r);
int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y);

#endif
