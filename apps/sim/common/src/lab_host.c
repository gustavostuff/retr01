#include "ui.h"
#include "ui_internal.h"

#include "r01a_lab_sim.h"

#include "discrete_ic/board_layout.h"
#include "discrete_ic/entity.h"
#include "discrete_ic/island.h"
#include "discrete_ic/island_builder.h"
#include "discrete_ic/passive.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef R01A_LAB_TITLE
#define R01A_LAB_TITLE "Retr01 Lab"
#endif
#ifndef R01A_ISLAND_LAYOUT
#define R01A_ISLAND_LAYOUT "island_layout.json"
#endif

#define R01A_SIM_BUDGET_MS 8
#define R01A_SIM_MAX_STEPS_PER_FRAME 24
#define R01A_DOTS_PER_STEP 32

static int entity_box(const NsEntity *e, int *x0, int *y0, int *x1, int *y1) {
    int w;
    int h;
    if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_NONE) {
        return 0;
    }
    w = e->body_w > 0 ? e->body_w : 8;
    h = e->body_h > 0 ? e->body_h : 8;
    *x0 = e->board_x;
    *y0 = e->board_y;
    *x1 = e->board_x + w;
    *y1 = e->board_y + h;
    return 1;
}

static void shift_entity(NsEntity *e, int dx, int dy) {
    if (!e || (dx == 0 && dy == 0)) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        NsPassive *p = (NsPassive *)e;
        ns_passive_set_pivot(p, p->pivot_x + dx, p->pivot_y + dy);
        return;
    }
    ns_entity_place(e, e->board_x + dx, e->board_y + dy);
}

static int island_bounds(const NsIsland *island, int *x0, int *y0, int *x1, int *y1) {
    int i;
    int any = 0;
    if (!island) {
        return 0;
    }
    for (i = 0; i < island->entity_count; i++) {
        int a, b, c, d;
        if (!entity_box(island->entities[i], &a, &b, &c, &d)) {
            continue;
        }
        if (!any || a < *x0) {
            *x0 = a;
        }
        if (!any || b < *y0) {
            *y0 = b;
        }
        if (!any || c > *x1) {
            *x1 = c;
        }
        if (!any || d > *y1) {
            *y1 = d;
        }
        any = 1;
    }
    return any;
}

static void wrap_island(NsIsland *island) {
    int x0, y0, x1, y1;
    int pad_x = NS_ISLAND_PAD_X + NS_CHIP_PIN_OUT;
    if (!island_bounds(island, &x0, &y0, &x1, &y1)) {
        island->board_w = NS_ISLAND_MIN_W;
        island->board_h = NS_ISLAND_MIN_H;
        return;
    }
    island->board_x = ns_grid_snap(x0 - pad_x);
    if (island->board_x > x0 - pad_x) {
        island->board_x -= NS_GRID;
    }
    island->board_y = ns_grid_snap(y0 - NS_ISLAND_PAD_TOP);
    if (island->board_y > y0 - NS_ISLAND_PAD_TOP) {
        island->board_y -= NS_GRID;
    }
    island->board_w = ns_grid_snap_up((x1 - island->board_x) + pad_x);
    island->board_h = ns_grid_snap_up((y1 - island->board_y) + NS_ISLAND_PAD_BOTTOM);
    if (island->board_w < NS_ISLAND_MIN_W) {
        island->board_w = NS_ISLAND_MIN_W;
    }
    if (island->board_h < NS_ISLAND_MIN_H) {
        island->board_h = NS_ISLAND_MIN_H;
    }
}

/* Park the analog parts to the right of the digital cluster, then frame both. */
static void layout_two_islands(NsIslandBuilder *b) {
    NsIsland *dig;
    NsIsland *ana;
    int dx0, dy0, dx1, dy1;
    int ax0, ay0, ax1, ay1;
    int shift;
    int i;
    if (!b || b->island_count < 2) {
        return;
    }
    dig = &b->islands[0];
    ana = &b->islands[1];
    if (!island_bounds(dig, &dx0, &dy0, &dx1, &dy1) || !island_bounds(ana, &ax0, &ay0, &ax1, &ay1)) {
        wrap_island(dig);
        wrap_island(ana);
        return;
    }
    shift = (dx1 + 48) - ax0;
    if (shift < 24) {
        shift = 24;
    }
    for (i = 0; i < ana->entity_count; i++) {
        shift_entity(ana->entities[i], shift, 0);
    }
    wrap_island(dig);
    wrap_island(ana);
}

static void logic_from_window(SDL_Window *win, int scale, int win_x, int win_y, int *lx, int *ly) {
    int ww, wh, draw_w, draw_h, ox, oy;
    SDL_GetWindowSize(win, &ww, &wh);
    if (scale < 1) {
        scale = 1;
    }
    draw_w = R01S_LOGIC_W * scale;
    draw_h = R01S_LOGIC_H * scale;
    ox = (ww - draw_w) / 2;
    oy = (wh - draw_h) / 2;
    *lx = (win_x - ox) / scale;
    *ly = (win_y - oy) / scale;
}

static void present(SDL_Renderer *ren, SDL_Texture *target, SDL_Window *win, int *scale_io) {
    int ww, wh, scale, draw_w, draw_h;
    SDL_Rect dst;
    SDL_GetWindowSize(win, &ww, &wh);
    scale = ww / R01S_LOGIC_W;
    if (wh / R01S_LOGIC_H < scale) {
        scale = wh / R01S_LOGIC_H;
    }
    if (scale < 1) {
        scale = 1;
    }
    *scale_io = scale;
    draw_w = R01S_LOGIC_W * scale;
    draw_h = R01S_LOGIC_H * scale;
    dst.x = (ww - draw_w) / 2;
    dst.y = (wh - draw_h) / 2;
    dst.w = draw_w;
    dst.h = draw_h;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, target, NULL, &dst);
    SDL_RenderPresent(ren);
}

static void sim_frame(struct R01aBoard *board) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 budget = (freq * (Uint64)R01A_SIM_BUDGET_MS) / 1000u;
    int n = 0;
    while (n < R01A_SIM_MAX_STEPS_PER_FRAME) {
        r01a_lab_step_dots(board, (uint32_t)R01A_DOTS_PER_STEP);
        n++;
        if ((SDL_GetPerformanceCounter() - t0) >= budget) {
            break;
        }
    }
}

static void mount_chips(R01sUi *ui, NsIslandBuilder *b) {
    int ii;
    int ei;
    for (ii = 0; ii < b->island_count; ii++) {
        NsIsland *island = &b->islands[ii];
        for (ei = 0; ei < island->entity_count; ei++) {
            NsEntity *e = island->entities[ei];
            if (!e || e->visual == NS_ENTITY_VIS_NONE || e->visual == NS_ENTITY_VIS_BREADBOARD) {
                continue;
            }
            if (r01s_ui_add_chip(ui, e, ii) != 0) {
                fprintf(stderr, "ui: dropped %s\n", e->refdes ? e->refdes : "?");
                return;
            }
        }
    }
    r01s_ui_chip_z_init(ui);
    r01s_ui_island_z_init(ui);
}

int r01a_ui_run(struct R01aBoard *board) {
    R01sUi ui;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *target;
    NsIslandGroup *group;
    int scale = 2;
    int quit = 0;
    if (!board) {
        return 1;
    }
    r01a_lab_boot(board);
    setenv("R01S_LAYOUT", R01A_ISLAND_LAYOUT, 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    memset(&ui, 0, sizeof(ui));
    if (r01s_ui_init(&ui) != 0) {
        SDL_Quit();
        return 1;
    }
    ui.prefer_islands = 1;
    ui.air_green_only = 1;
    ui.layout_compact = 0;
    ui.floor_on = 0;
    ui.air_wires = R01S_AIR_VIEW_ALL;
    snprintf(ui.status, sizeof(ui.status),
             "SPACE wires: all, hidden. Hover a part when hidden. Ctrl+wheel zoom. P pause.");

    win = SDL_CreateWindow(R01A_LAB_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           R01S_LOGIC_W * scale, R01S_LOGIC_H * scale,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        r01s_ui_shutdown(&ui);
        SDL_Quit();
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren) {
        fprintf(stderr, "renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        r01s_ui_shutdown(&ui);
        SDL_Quit();
        return 1;
    }
    target = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, R01S_LOGIC_W, R01S_LOGIC_H);
    if (!target) {
        fprintf(stderr, "texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        r01s_ui_shutdown(&ui);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(target, SDL_ScaleModeNearest);

    group = r01a_lab_group(board);
    r01s_ui_bind_group(&ui, group);
    ui.pin_net = r01a_lab_pins(board);
    layout_two_islands(r01a_lab_builder(board));
    mount_chips(&ui, r01a_lab_builder(board));
    if (r01s_ui_layout_load(&ui) != 0) {
        ui.layout_compact = 0;
        ui.floor_on = 0;
    }
    ui.prefer_islands = 1;
    ui.air_green_only = 1;
    ui.layout_compact = 0;
    ui.floor_on = 0;
    if (ui.air_wires != R01S_AIR_VIEW_NONE) {
        ui.air_wires = R01S_AIR_VIEW_ALL;
    }

    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            int lx = 0;
            int ly = 0;
            int mx = 0;
            int my = 0;
            if (ev.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode key = ev.key.keysym.sym;
                SDL_Keymod mod = ev.key.keysym.mod;
                if (key == SDLK_ESCAPE) {
                    quit = 1;
                    break;
                }
                if (key == SDLK_SPACE) {
                    ui.air_wires = (ui.air_wires == R01S_AIR_VIEW_NONE) ? R01S_AIR_VIEW_ALL : R01S_AIR_VIEW_NONE;
                    ui.layout_dirty = 1;
                    snprintf(ui.status, sizeof(ui.status), "air wires: %s",
                             ui.air_wires == R01S_AIR_VIEW_NONE ? "hidden" : "all");
                    continue;
                }
                if (key == SDLK_p && !(mod & KMOD_CTRL)) {
                    r01a_lab_set_running(board, !r01a_lab_running(board));
                    continue;
                }
                if (key == SDLK_r && (mod & KMOD_CTRL)) {
                    r01a_lab_reset(board);
                    r01a_lab_set_running(board, 1);
                    continue;
                }
                if (key == SDLK_PERIOD && !(mod & KMOD_CTRL) && !r01a_lab_running(board)) {
                    r01a_lab_step(board);
                    continue;
                }
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                mx = ev.button.x;
                my = ev.button.y;
            } else if (ev.type == SDL_MOUSEMOTION) {
                mx = ev.motion.x;
                my = ev.motion.y;
            } else {
                SDL_GetMouseState(&mx, &my);
            }
            logic_from_window(win, scale, mx, my, &lx, &ly);
            r01s_ui_handle_event(&ui, &ev, lx, ly);
        }
        if (r01a_lab_running(board)) {
            sim_frame(board);
        }
        if (group) {
            char st[128];
            st[0] = '\0';
            ns_island_group_fill_status(group, st, sizeof(st));
            if (!ui.layout_dirty && st[0]) {
                snprintf(ui.status, sizeof(ui.status), "%s", st);
            }
        }
        SDL_SetRenderTarget(ren, target);
        r01s_ui_draw(&ui, ren);
        SDL_SetRenderTarget(ren, NULL);
        present(ren, target, win, &scale);
    }

    if (r01s_ui_layout_save(&ui) != 0) {
        fprintf(stderr, "layout: save failed\n");
    }
    SDL_DestroyTexture(target);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    r01s_ui_shutdown(&ui);
    SDL_Quit();
    return 0;
}
