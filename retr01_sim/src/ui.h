#ifndef RETR01_SIM_UI_H
#define RETR01_SIM_UI_H

#include "retr01_sim/entity.h"
#include "retr01_sim/gamepad.h"
#include "retr01_sim/island_group.h"
#include "retr01_sim/types.h"

#include <SDL.h>
#include <stdint.h>

#define R01S_BOARD_MAX_CHIPS 32
#define R01S_UI_GAMEPAD_COUNT 2

typedef struct R01sUi {
    R01sIslandGroup *group;
    R01sEntity *chips[R01S_BOARD_MAX_CHIPS];
    uint8_t chip_island[R01S_BOARD_MAX_CHIPS];
    int chip_count;
    int selected; /* index or -1 */
    int pan_x;
    int pan_y;
    int drag_pan; /* middle/right button pan */
    int drag_chip; /* chip index while left-dragging, else -1 */
    int drag_grab_bx;
    int drag_grab_by;
    int drag_last_x;
    int drag_last_y;
    char status[192];
    int probe_vdd;
    int probe_phi2;
    int probe_resb_low;
    uint8_t probe_pad_p1;
    uint8_t probe_pad_p2;
    R01sGamepadInput gamepad[R01S_UI_GAMEPAD_COUNT];
    int drag_stick; /* player index or -1 */
    int drag_btn;   /* player*4 + btn index, or -1 */
    int mouse_btn[R01S_UI_GAMEPAD_COUNT][4]; /* X Y COIN START held by mouse */
} R01sUi;

int r01s_ui_init(R01sUi *ui);
void r01s_ui_shutdown(R01sUi *ui);

void r01s_ui_bind_group(R01sUi *ui, R01sIslandGroup *group);
int r01s_ui_add_chip(R01sUi *ui, R01sEntity *chip, int island_index);

void r01s_ui_clamp_pan(R01sUi *ui);
void r01s_ui_sync_gamepads(R01sUi *ui);
uint8_t r01s_ui_gamepad_port(const R01sUi *ui, int player);
void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r);
int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y);

#endif
