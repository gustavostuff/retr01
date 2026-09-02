#include "app.h"
#include "board_debug.h"

#include "retr01_sim/board.h"
#include "retr01_sim/timing.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef R01S_DEFAULT_CART
#define R01S_DEFAULT_CART "../output/test_2.retr01"
#endif

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

/*
 * DELAY=typical|typ|max  -- print datasheet path budget (corner); pin model unchanged.
 * Aliases: FAST= / PROP= / TPD= / R01S_PROP_DELAY=
 * Returns 0 ok, -1 bad value (message already printed).
 */
static int apply_delay_args(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *val = NULL;
        if (strncmp(a, "DELAY=", 6) == 0) {
            val = a + 6;
        } else if (strncmp(a, "FAST=", 5) == 0) {
            val = a + 5;
        } else if (strncmp(a, "PROP=", 5) == 0) {
            val = a + 5;
        } else if (strncmp(a, "TPD=", 4) == 0) {
            val = a + 4;
        } else if (strncmp(a, "R01S_PROP_DELAY=", 16) == 0) {
            val = a + 16;
        } else {
            continue;
        }
        if (strcmp(val, "max") == 0 || strcmp(val, "MAX") == 0) {
            r01s_timing_set_prop_override(1, R01S_TPD_MAX);
            fprintf(stderr, "timing: prop delay ON (max corner)\n");
            r01s_timing_print_budget(stderr);
        } else if (strcmp(val, "typical") == 0 || strcmp(val, "typ") == 0 || strcmp(val, "TYP") == 0 ||
                   strcmp(val, "1") == 0) {
            r01s_timing_set_prop_override(1, R01S_TPD_TYP);
            fprintf(stderr, "timing: prop delay ON (typical corner)\n");
            r01s_timing_print_budget(stderr);
        } else if (strcmp(val, "0") == 0 || strcmp(val, "off") == 0 || strcmp(val, "OFF") == 0) {
            r01s_timing_set_prop_override(0, R01S_TPD_TYP);
            fprintf(stderr, "timing: prop delay OFF\n");
        } else {
            fprintf(stderr, "timing: bad %s (use DELAY=typical|max)\n", a);
            return -1;
        }
    }
    return 0;
}

static int is_option_arg(const char *a) {
    if (!a || !a[0]) {
        return 0;
    }
    if (a[0] == '-') {
        return 1;
    }
    if (strncmp(a, "DELAY=", 6) == 0 || strncmp(a, "FAST=", 5) == 0 || strncmp(a, "PROP=", 5) == 0 ||
        strncmp(a, "TPD=", 4) == 0 || strncmp(a, "R01S_PROP_DELAY=", 16) == 0) {
        return 1;
    }
    return 0;
}

static const char *first_cart_arg(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (is_option_arg(argv[i])) {
            continue;
        }
        return argv[i];
    }
    return NULL;
}

static int try_load_cart(R01sBoard *board, int argc, char **argv) {
    const char *path = first_cart_arg(argc, argv);
    if (!path) {
        path = R01S_DEFAULT_CART;
    }
    if (r01s_board_load_cart(board, path) != 0) {
        fprintf(stderr, "cart: failed to load %s into SST39SF040\n", path);
        return -1;
    }
    fprintf(stderr, "cart: loaded %s into SST39SF040 (label=%s prg_off=0x%06x prg_len=0x%06x)\n", path,
            board->cart_label, (unsigned)board->cart_off_prg, (unsigned)board->cart_len_prg);
    return 0;
}

static int setup_board(R01sApp *app, int argc, char **argv) {
    char title[96];
    if (!app) {
        return -1;
    }
    /*
     * Window starts hidden; first boot present (via catchup start) reveals it.
     * Board build can take a while -- keep the window hidden so we never flash
     * an empty/uninitialized frame before "Booting console...".
     */
    r01s_island_builder_init(&app->builder);
    if (r01s_board_build(&g_board, &app->builder) != 0) {
        return -1;
    }
    r01s_app_mount_builder(app);
    {
        R01sIslandGroup *group = r01s_island_builder_group(&app->builder);
        /* Reset pin/state only. Flash mem is preserved across entity reset. */
        r01s_island_group_reset(group);
        /* Argv .retr01 is the image copied into cart SST39SF040 (after reset). */
        if (try_load_cart(&g_board, argc, argv) != 0) {
            return -1;
        }
        /* Non-blocking: worker thread runs IC MAP stream; UI stays responsive. */
        r01s_app_start_ic_catchup(app, &g_board);
    }
    snprintf(title, sizeof(title), "Retr01 Sim -- %s", g_board.cart_label[0] ? g_board.cart_label : "cart");
    SDL_SetWindowTitle(app->win, title);
    return 0;
}

int main(int argc, char **argv) {
    R01sApp app;
    int debug = want_debug(argc, argv);

    if (apply_delay_args(argc, argv) != 0) {
        return 1;
    }

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
