#ifndef RETR01_SIM_APP_H
#define RETR01_SIM_APP_H

#include "retr01_sim/island_builder.h"
#include "ui.h"

#include <SDL.h>

typedef struct R01sApp {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *target;
    int scale;
    int running;
    R01sUi ui;
    R01sIslandBuilder builder;
} R01sApp;

int r01s_app_init(R01sApp *app, int headless);
void r01s_app_shutdown(R01sApp *app);
void r01s_app_frame(R01sApp *app);
void r01s_app_handle_event(R01sApp *app, const SDL_Event *e);

/* Register builder chips on the UI (after board setup). */
void r01s_app_mount_builder(R01sApp *app);

#endif
