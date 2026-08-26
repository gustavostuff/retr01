#include "app.h"
#include "board_debug.h"

#include "retr01_sim/board.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static R01sBoard g_board;

static int want_debug(int argc, char **argv) {
    int i;
    const char *env = getenv("R01S_DEBUG");
    if (env && env[0] != '\0' && strcmp(env, "0") != 0) {
        return 1;
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            return 1;
        }
    }
    return 0;
}

static const char *first_cart_arg(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
                continue;
            }
            continue;
        }
        return argv[i];
    }
    return NULL;
}

static int try_load_cart(R01sBoard *board, int argc, char **argv) {
    const char *path = first_cart_arg(argc, argv);
    if (!path) {
        return 0;
    }
    if (r01s_board_load_cart(board, path) != 0) {
        fprintf(stderr, "cart: failed to load %s (cwd-relative paths break after ./sim build-run)\n",
                path);
        return -1;
    }
    return 0;
}

static int setup_board(R01sApp *app, int argc, char **argv) {
    char title[96];
    if (!app) {
        return -1;
    }
    /*
     * Window starts hidden; first boot present (via catchup start) reveals it.
     * Board build can take a while — keep the window hidden so we never flash
     * an empty/uninitialized frame before "Booting console…".
     */
    r01s_island_builder_init(&app->builder);
    if (r01s_board_build(&g_board, &app->builder) != 0) {
        return -1;
    }
    if (try_load_cart(&g_board, argc, argv) != 0) {
        return -1;
    }
    r01s_app_mount_builder(app);
    {
        R01sIslandGroup *group = r01s_island_builder_group(&app->builder);
        r01s_island_group_reset(group);
        /* Non-blocking: worker thread runs IC MAP stream; UI stays responsive. */
        r01s_app_start_ic_catchup(app, &g_board);
    }
    snprintf(title, sizeof(title), "retr01 Sim — %s", g_board.cart_label[0] ? g_board.cart_label : "cart");
    SDL_SetWindowTitle(app->win, title);
    return 0;
}

int main(int argc, char **argv) {
    R01sApp app;
    int debug = want_debug(argc, argv);

    if (r01s_app_init(&app, 0) != 0) {
        return 1;
    }
    if (setup_board(&app, argc, argv) != 0) {
        fprintf(stderr, "board setup failed\n");
        if (app.win) {
            SDL_ShowWindow(app.win);
        }
        r01s_app_shutdown(&app);
        return 1;
    }
    r01s_board_debug_begin(&g_board, debug);

    while (app.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            r01s_app_handle_event(&app, &e);
        }
        r01s_app_frame(&app);
        if (debug) {
            r01s_board_debug_tick(&g_board, SDL_GetTicks());
        }
    }

    r01s_board_debug_end();
    r01s_app_shutdown(&app);
    return 0;
}
