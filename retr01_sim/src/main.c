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


static int try_load_cart(R01sBoard *board, int argc, char **argv) {
    static const char *defaults[] = {"retr01_studio/project.retr01", "../retr01_studio/project.retr01",
                                     "../../retr01_studio/project.retr01",
                                     "retr01_studio/project_flash.bin", "../retr01_studio/project_flash.bin",
                                     NULL};
    int i;
    if (argc >= 2 && argv[1] && argv[1][0] != '-') {
        if (r01s_board_load_cart(board, argv[1]) == 0) {
            fprintf(stderr, "cart: loaded %s (bring-up PRG overlay applied)\n", argv[1]);
            return 0;
        }
        fprintf(stderr, "cart: failed to load %s — keeping synthetic\n", argv[1]);
        return -1;
    }
    for (i = 0; defaults[i]; i++) {
        if (r01s_board_load_cart(board, defaults[i]) == 0) {
            fprintf(stderr, "cart: loaded %s (bring-up PRG overlay applied)\n", defaults[i]);
            return 0;
        }
    }
    fprintf(stderr, "cart: using synthetic bring-up image\n");
    return 0;
}

static int setup_board(R01sApp *app, int argc, char **argv) {
    char title[96];
    if (!app) {
        return -1;
    }
    r01s_island_builder_init(&app->builder);
    if (r01s_board_build(&g_board, &app->builder) != 0) {
        return -1;
    }
    (void)try_load_cart(&g_board, argc, argv);
    r01s_app_mount_builder(app);
    snprintf(title, sizeof(title), "Retr01 Sim — %s", g_board.cart_label[0] ? g_board.cart_label : "cart");
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
