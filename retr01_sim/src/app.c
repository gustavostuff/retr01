#include "app.h"

#include "retr01_sim/board.h"
#include "pads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void logic_from_window(const R01sApp *app, int win_x, int win_y, int *lx, int *ly) {
    int ww, wh, draw_w, draw_h, ox, oy, scale;
    SDL_GetWindowSize(app->win, &ww, &wh);
    scale = app->scale > 0 ? app->scale : 1;
    draw_w = R01S_LOGIC_W * scale;
    draw_h = R01S_LOGIC_H * scale;
    ox = (ww - draw_w) / 2;
    oy = (wh - draw_h) / 2;
    *lx = (win_x - ox) / scale;
    *ly = (win_y - oy) / scale;
}

void r01s_app_mount_builder(R01sApp *app) {
    int i;
    R01sIslandBuilder *b;
    if (!app) {
        return;
    }
    b = &app->builder;
    r01s_ui_bind_group(&app->ui, &b->group);
    for (i = 0; i < b->mount_count; i++) {
        r01s_ui_add_chip(&app->ui, b->mounts[i].entity, b->mounts[i].island_index);
    }
}

int r01s_app_init(R01sApp *app, int headless) {
    Uint32 flags;
    memset(app, 0, sizeof(*app));
    app->scale = 1;
    app->running = 1;

    if (headless) {
        if (!getenv("SDL_VIDEODRIVER")) {
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen");
        }
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    if (r01s_ui_init(&app->ui) != 0) {
        SDL_Quit();
        return -1;
    }

    flags = SDL_WINDOW_ALLOW_HIGHDPI;
    if (headless) {
        flags |= SDL_WINDOW_HIDDEN;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    app->win = SDL_CreateWindow("Retr01 Sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, R01S_LOGIC_W,
                                R01S_LOGIC_H, flags);
    if (!app->win) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        r01s_ui_shutdown(&app->ui);
        SDL_Quit();
        return -1;
    }

    app->ren = SDL_CreateRenderer(app->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app->ren) {
        app->ren = SDL_CreateRenderer(app->win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!app->ren) {
        fprintf(stderr, "renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(app->win);
        r01s_ui_shutdown(&app->ui);
        SDL_Quit();
        return -1;
    }

    app->target =
        SDL_CreateTexture(app->ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, R01S_LOGIC_W, R01S_LOGIC_H);
    if (!app->target) {
        fprintf(stderr, "texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(app->ren);
        SDL_DestroyWindow(app->win);
        r01s_ui_shutdown(&app->ui);
        SDL_Quit();
        return -1;
    }
    SDL_SetTextureScaleMode(app->target, SDL_ScaleModeNearest);
    return 0;
}

void r01s_app_shutdown(R01sApp *app) {
    if (!app) {
        return;
    }
    r01s_island_builder_shutdown(&app->builder);
    r01s_ui_shutdown(&app->ui);
    if (app->target) {
        SDL_DestroyTexture(app->target);
    }
    if (app->ren) {
        SDL_DestroyRenderer(app->ren);
    }
    if (app->win) {
        SDL_DestroyWindow(app->win);
    }
    SDL_Quit();
    memset(app, 0, sizeof(*app));
}

void r01s_app_frame(R01sApp *app) {
    int ww, wh, scale, draw_w, draw_h;
    SDL_Rect dst;
    R01sIslandGroup *group;
    R01sBoard *board;

    group = r01s_island_builder_group(&app->builder);
    r01s_ui_sync_gamepads(&app->ui);
    board = r01s_board_from_group(group);
    if (board) {
        r01s_pads_set(&board->pads, 0, r01s_ui_gamepad_port(&app->ui, 0));
        r01s_pads_set(&board->pads, 1, r01s_ui_gamepad_port(&app->ui, 1));
        r01s_pads_refresh_preview(&board->pads);
        app->ui.probe_pad_p1 = r01s_pads_get(&board->pads, 0);
        app->ui.probe_pad_p2 = r01s_pads_get(&board->pads, 1);
    }
    if (group) {
        r01s_island_group_frame(group);
        r01s_island_group_fill_status(group, app->ui.status, sizeof(app->ui.status));
        r01s_island_group_update_probes(group, &app->ui.probe_vdd, &app->ui.probe_phi2, &app->ui.probe_resb_low);
    }

    SDL_SetRenderTarget(app->ren, app->target);
    r01s_ui_draw(&app->ui, app->ren);
    SDL_SetRenderTarget(app->ren, NULL);

    SDL_GetWindowSize(app->win, &ww, &wh);
    {
        int sx = ww / R01S_LOGIC_W;
        int sy = wh / R01S_LOGIC_H;
        scale = sx < sy ? sx : sy;
        if (scale < 1) {
            scale = 1;
        }
        app->scale = scale;
    }
    draw_w = R01S_LOGIC_W * scale;
    draw_h = R01S_LOGIC_H * scale;
    dst.x = (ww - draw_w) / 2;
    dst.y = (wh - draw_h) / 2;
    dst.w = draw_w;
    dst.h = draw_h;

    SDL_SetRenderDrawColor(app->ren, 0, 0, 0, 255);
    SDL_RenderClear(app->ren);
    SDL_RenderCopy(app->ren, app->target, NULL, &dst);
    SDL_RenderPresent(app->ren);
}

void r01s_app_handle_event(R01sApp *app, const SDL_Event *e) {
    int lx, ly;
    R01sIslandGroup *group;
    if (!app || !e) {
        return;
    }
    group = r01s_island_builder_group(&app->builder);
    if (e->type == SDL_QUIT) {
        app->running = 0;
        return;
    }
    if (e->type == SDL_KEYDOWN && group) {
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE:
            app->running = 0;
            return;
        case SDLK_SPACE:
            group->running = !group->running;
            return;
        case SDLK_r:
            r01s_island_group_reset(group);
            return;
        case SDLK_PERIOD:
            if (!group->running) {
                r01s_island_group_step(group);
            }
            return;
        default:
            break;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEMOTION ||
        e->type == SDL_MOUSEWHEEL || e->type == SDL_KEYDOWN) {
        int mx = 0, my = 0;
        if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
            mx = e->button.x;
            my = e->button.y;
        } else if (e->type == SDL_MOUSEMOTION) {
            mx = e->motion.x;
            my = e->motion.y;
        } else {
            SDL_GetMouseState(&mx, &my);
        }
        logic_from_window(app, mx, my, &lx, &ly);
        r01s_ui_handle_event(&app->ui, e, lx, ly);
    }
}
