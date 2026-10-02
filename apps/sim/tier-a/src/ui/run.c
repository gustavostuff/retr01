#include "r01a_ui.h"

#include "r01a_layout.h"
#include "ui_font.h"

#include <stdio.h>
#include <string.h>

static void sim_frame(R01aBoard *board) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 budget = (freq * (Uint64)R01A_SIM_BUDGET_MS) / 1000u;
    int n = 0;
    while (n < R01A_SIM_MAX_STEPS_PER_FRAME) {
        r01a_board_step_dots(board, (uint32_t)R01A_DOTS_PER_STEP);
        n++;
        if ((SDL_GetPerformanceCounter() - t0) >= budget) {
            break;
        }
    }
}

int r01a_ui_run(R01aBoard *board) {
    R01aUi ui;
    int quit = 0;
    int dummy_air = 1;
    if (!board) {
        return 1;
    }
    memset(&ui, 0, sizeof(ui));
    ui.selected = -1;
    ui.drag_chip = -1;
    ui.hover_chip = -1;
    ui.hover_pin = -1;
    ui.dest_chip = -1;
    ui.dest_pin = -1;
    ui.zoom = 1;
    ui.show_nets = 1;
    r01a_board_set_wire_mode(board, R01A_WIRE_AUTO);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    (void)r01a_font_init();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    ui.win = SDL_CreateWindow("Retr01 Tier A", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              NS_LOGIC_W * R01A_UI_SCALE, NS_LOGIC_H * R01A_UI_SCALE, SDL_WINDOW_RESIZABLE);
    if (!ui.win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        r01a_font_shutdown();
        SDL_Quit();
        return 1;
    }
    ui.rend = SDL_CreateRenderer(ui.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui.rend) {
        ui.rend = SDL_CreateRenderer(ui.win, -1, 0);
    }
    if (!ui.rend) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(ui.win);
        r01a_font_shutdown();
        SDL_Quit();
        return 1;
    }
    ui.target = SDL_CreateTexture(ui.rend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, NS_LOGIC_W,
                                  NS_LOGIC_H);
    if (!ui.target) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ui.rend);
        SDL_DestroyWindow(ui.win);
        r01a_font_shutdown();
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(ui.target, SDL_ScaleModeNearest);
    ui.scale = R01A_UI_SCALE;
    dummy_air = ui.show_nets;
    (void)r01a_layout_load(R01A_LAYOUT_FILE, board, &ui.pan_x, &ui.pan_y, &ui.zoom, &dummy_air);
    ui.show_nets = dummy_air ? 1 : 0;
    r01a_board_set_wire_mode(board, R01A_WIRE_AUTO);
    bind_chips(&ui, board);
    snap_placed_chips(&ui);
    pin_net_build(&ui);
    hist_init(&ui);

    while (!quit) {
        SDL_Event ev;
        update_scale(&ui);
        while (SDL_PollEvent(&ev)) {
            int lx = 0;
            int ly = 0;
            int rc;
            if (ev.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                logic_from_window(&ui, ev.button.x, ev.button.y, &lx, &ly);
            } else if (ev.type == SDL_MOUSEMOTION) {
                logic_from_window(&ui, ev.motion.x, ev.motion.y, &lx, &ly);
            } else {
                int mx = 0;
                int my = 0;
                SDL_GetMouseState(&mx, &my);
                logic_from_window(&ui, mx, my, &lx, &ly);
            }
            rc = handle_event(&ui, board, &ev, lx, ly);
            if (rc == 2) {
                quit = 1;
            }
        }
        if (board->running) {
            sim_frame(board);
        }
        draw_frame(&ui, board);
    }

    snap_placed_chips(&ui);
    (void)r01a_layout_save(R01A_LAYOUT_FILE, board, ui.pan_x, ui.pan_y, canvas_zoom(&ui),
                          ui.show_nets ? 1 : 0);
    if (ui.lcd_tex) {
        SDL_DestroyTexture(ui.lcd_tex);
    }
    if (ui.target) {
        SDL_DestroyTexture(ui.target);
    }
    SDL_DestroyRenderer(ui.rend);
    SDL_DestroyWindow(ui.win);
    r01a_font_shutdown();
    SDL_Quit();
    return 0;
}
