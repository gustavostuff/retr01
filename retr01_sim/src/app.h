#ifndef RETR01_SIM_APP_H
#define RETR01_SIM_APP_H

#include "retr01_sim/island_builder.h"
#include "ui.h"

#include <SDL.h>

struct R01sBoard;

typedef struct R01sApp {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *target;
    int scale;
    int running;
    Uint32 fps_last_ms;
    int fps_frames;
    R01sUi ui;
    R01sIslandBuilder builder;
    /* IC MAP catchup worker (board steps only; SDL stays on main). */
    SDL_Thread *catchup_th;
    SDL_mutex *board_mu;
    SDL_atomic_t catchup_active;
    int catchup_rc;
    struct R01sBoard *catchup_board;
    /* Worker sets catchup_ui_req=1 after each batch; main advances spinner + clears. */
    SDL_atomic_t catchup_ui_req;
    int catchup_spin; /* 0..3 boot spinner phase */
} R01sApp;

int r01s_app_init(R01sApp *app, int headless);
void r01s_app_shutdown(R01sApp *app);
void r01s_app_frame(R01sApp *app);
void r01s_app_handle_event(R01sApp *app, const SDL_Event *e);

/* Register builder chips on the UI (after board setup). */
void r01s_app_mount_builder(R01sApp *app);

/* Start IC MAP stream catchup on a worker thread (non-blocking). */
void r01s_app_start_ic_catchup(R01sApp *app, struct R01sBoard *board);
int r01s_app_catchup_active(const R01sApp *app);

#endif
