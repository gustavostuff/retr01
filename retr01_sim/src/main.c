#include "app.h"
#include "board_debug.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_fast.h"

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

static int is_fast_flag(const char *arg, const char **spec_out) {
    if (!arg) {
        return 0;
    }
    if (strcmp(arg, "--fast") == 0) {
        if (spec_out) {
            *spec_out = "boot";
        }
        return 1;
    }
    if (strncmp(arg, "--fast=", 7) == 0) {
        if (spec_out) {
            *spec_out = arg + 7;
        }
        return 1;
    }
    if (strcmp(arg, "--no-fast") == 0) {
        if (spec_out) {
            *spec_out = "none";
        }
        return 1;
    }
    return 0;
}

static void apply_fast_cli(int argc, char **argv) {
    int i;
    uint32_t mask = r01s_fast_glue_from_env();
    int saw_cli = 0;

    for (i = 1; i < argc; i++) {
        const char *spec = NULL;
        if (is_fast_flag(argv[i], &spec)) {
            uint32_t parsed = r01s_fast_glue_parse(spec);
            if (spec && (parsed || strcmp(spec, "none") == 0 || strcmp(spec, "off") == 0 ||
                         strcmp(spec, "0") == 0)) {
                mask = parsed;
                saw_cli = 1;
            } else {
                fprintf(stderr, "fast: bad --fast=%s (try boot, settle,video, none)\n", spec ? spec : "?");
            }
        }
    }
    if (saw_cli) {
        r01s_fast_glue_set(mask);
    } else if (mask) {
        /* env already applied in from_env */
    }
    if (mask) {
        fprintf(stderr, "fast: %s (settle=%d video=%d) — pin-level still available via --no-fast / F key\n",
                r01s_fast_glue_label(mask), r01s_fast_glue_enabled(R01S_FAST_GLUE_SETTLE),
                r01s_fast_glue_enabled(R01S_FAST_GLUE_VIDEO));
    }
}

static const char *first_cart_arg(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (is_fast_flag(argv[i], NULL)) {
                continue;
            }
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
    static const char *defaults[] = {"retr01_studio/project.retr01", "../retr01_studio/project.retr01",
                                     "../../retr01_studio/project.retr01",
                                     "retr01_studio/project_flash.bin", "../retr01_studio/project_flash.bin",
                                     NULL};
    int i;
    const char *path = first_cart_arg(argc, argv);
    if (path) {
        if (r01s_board_load_cart(board, path) == 0) {
            fprintf(stderr, "cart: loaded %s (bring-up PRG overlay applied)\n", path);
            return 0;
        }
        fprintf(stderr, "cart: failed to load %s — keeping synthetic\n", path);
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
    snprintf(title, sizeof(title), "Retr01 Sim — %s%s", g_board.cart_label[0] ? g_board.cart_label : "cart",
             r01s_fast_glue_mask() ? " [FAST]" : "");
    SDL_SetWindowTitle(app->win, title);
    return 0;
}

int main(int argc, char **argv) {
    R01sApp app;
    int debug = want_debug(argc, argv);

    apply_fast_cli(argc, argv);

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
