#include "app.h"

#include "retr01_sim/board.h"

#include <stdio.h>

static R01sBoard g_board;

static int setup_board(R01sApp *app) {
    if (!app) {
        return -1;
    }
    r01s_island_builder_init(&app->builder);
    if (r01s_board_build(&g_board, &app->builder) != 0) {
        return -1;
    }
    r01s_app_mount_builder(app);
    SDL_SetWindowTitle(app->win, "Retr01 Sim — Islands A–E");
    return 0;
}

int main(int argc, char **argv) {
    R01sApp app;
    (void)argc;
    (void)argv;

    if (r01s_app_init(&app, 0) != 0) {
        return 1;
    }
    if (setup_board(&app) != 0) {
        fprintf(stderr, "board setup failed\n");
        r01s_app_shutdown(&app);
        return 1;
    }

    while (app.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            r01s_app_handle_event(&app, &e);
        }
        r01s_app_frame(&app);
    }

    r01s_app_shutdown(&app);
    return 0;
}
