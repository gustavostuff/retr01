#include "app.h"

#include "beam_xy.h"
#include "retr01_sim/board.h"
#include "retr01_sim/bom32.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/play.h"
#include "pads.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Step batches between UI tick handshakes with main (spinner advances). */
#define R01S_CATCHUP_BATCH_STEPS 32
/* Max time worker waits for main to ack the UI tick (ms). */
#define R01S_CATCHUP_UI_WAIT_MS 50

static void catchup_signal_ui_tick(R01sApp *app, R01sBoard *board) {
    Uint32 t0;
    if (!app) {
        return;
    }
    SDL_AtomicSet(&app->catchup_ui_req, 1);
    t0 = SDL_GetTicks();
    while (SDL_AtomicGet(&app->catchup_ui_req) != 0) {
        if (board && board->catchup_cancel) {
            SDL_AtomicSet(&app->catchup_ui_req, 0);
            return;
        }
        if ((SDL_GetTicks() - t0) >= (Uint32)R01S_CATCHUP_UI_WAIT_MS) {
            SDL_AtomicSet(&app->catchup_ui_req, 0);
            return;
        }
        SDL_Delay(1);
    }
}

/* Leave headroom for draw + vsync inside a ~16.7 ms frame. */
#define R01S_SIM_BUDGET_MS 3
#define R01S_SIM_BUDGET_MS_PLAY 10
#define R01S_SIM_MAX_STEPS_PER_FRAME 24

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

static void catchup_join(R01sApp *app) {
    if (!app || !app->catchup_th) {
        return;
    }
    SDL_WaitThread(app->catchup_th, NULL);
    app->catchup_th = NULL;
    SDL_AtomicSet(&app->catchup_active, 0);
    if (app->catchup_rc != 0) {
        fprintf(stderr, "cart: IC MAP stream catchup failed (LCD may stay blank)\n");
        snprintf(app->ui.status, sizeof(app->ui.status), "IC MAP stream failed");
    } else {
        snprintf(app->ui.status, sizeof(app->ui.status), "IC MAP stream ready");
        if (app->catchup_board) {
            (void)r01s_play_start(app->catchup_board);
        }
    }
}

/*
 * Yielding catchup: step under mutex one-at-a-time so the UI thread can paint.
 * Uses the same completion rules as r01s_board_catchup_bringup (non-softboot).
 */
static int catchup_thread_fn_yielding(void *userdata) {
    R01sApp *app = (R01sApp *)userdata;
    R01sIslandGroup *group;
    R01sBoard *board;
    uint32_t target;
    uint8_t expect0;
    int i;
    const char *want_soft;

    if (!app) {
        return -1;
    }
    group = r01s_island_builder_group(&app->builder);
    board = app->catchup_board;
    if (!group || !board) {
        SDL_AtomicSet(&app->catchup_active, 0);
        return -1;
    }

    want_soft = getenv("R01S_SOFTBOOT");
    if (want_soft && want_soft[0] != '\0' && strcmp(want_soft, "0") != 0) {
        SDL_LockMutex(app->board_mu);
        app->catchup_rc = r01s_board_catchup_bringup(board, group);
        SDL_UnlockMutex(app->board_mu);
        SDL_AtomicSet(&app->catchup_active, 0);
        return app->catchup_rc;
    }

    if (!board->cart_loaded || board->cart_off_map_screen0 == 0) {
        app->catchup_rc = 0;
        SDL_AtomicSet(&app->catchup_active, 0);
        return 0;
    }

    SDL_LockMutex(app->board_mu);
    board->catchup_cancel = 0;
    target = board->cart_off_map_screen0 + 480u;
    expect0 = r01s_sst39sf040_peek(&board->cart_flash, board->cart_off_map_screen0);
    SDL_UnlockMutex(app->board_mu);

    i = 0;
    while (i < 80000) {
        int done = 0;
        int batch;
        SDL_LockMutex(app->board_mu);
        for (batch = 0; batch < R01S_CATCHUP_BATCH_STEPS && i < 80000; batch++) {
            if (board->catchup_cancel) {
                app->catchup_rc = -1;
                SDL_UnlockMutex(app->board_mu);
                SDL_AtomicSet(&app->catchup_ui_req, 0);
                SDL_AtomicSet(&app->catchup_active, 0);
                return -1;
            }
            r01s_island_group_step(group);
            i++;
            if (board->map_addr >= target && r01s_as6c62256_peek(&board->vram, 0) == expect0) {
                done = 1;
                app->catchup_rc = 0;
                /* Same sticky marks as r01s_board_catchup_bringup (A==$AA is easy to miss). */
                board->health_saw_vram = 1;
                board->health_saw_vram_read = 1;
                board->health_saw_map = 1;
                r01s_board_catchup_finish(board);
                break;
            }
        }
        snprintf(app->ui.status, sizeof(app->ui.status),
                 "IC MAP stream… %d steps  map=%06X/%06X", i, (unsigned)board->map_addr,
                 (unsigned)target);
        if (done) {
            snprintf(app->ui.status, sizeof(app->ui.status), "IC MAP stream ready (%d steps)", i);
        }
        SDL_UnlockMutex(app->board_mu);
        if (done) {
            catchup_signal_ui_tick(app, board);
            SDL_AtomicSet(&app->catchup_active, 0);
            return 0;
        }
        /* Handshake: main advances boot spinner, then worker continues. */
        catchup_signal_ui_tick(app, board);
    }

    app->catchup_rc = -1;
    SDL_AtomicSet(&app->catchup_active, 0);
    return -1;
}

int r01s_app_catchup_active(const R01sApp *app) {
    return app && SDL_AtomicGet((SDL_atomic_t *)&app->catchup_active) != 0;
}

/* Draw + present boot UI; reveal window on first paint so setup never flashes empty. */
static void app_present_boot(R01sApp *app, int spin) {
    int ww, wh, scale, draw_w, draw_h;
    SDL_Rect dst;

    if (!app || !app->ren || !app->target || !app->win) {
        return;
    }

    SDL_SetRenderTarget(app->ren, app->target);
    r01s_ui_draw_boot(&app->ui, app->ren, spin);
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

    if (!(SDL_GetWindowFlags(app->win) & SDL_WINDOW_SHOWN)) {
        SDL_ShowWindow(app->win);
    }
}

void r01s_app_start_ic_catchup(R01sApp *app, struct R01sBoard *board) {
    R01sIslandGroup *group;
    if (!app || !board) {
        return;
    }
    if (app->catchup_th) {
        /* Previous worker still joining. */
        if (SDL_AtomicGet(&app->catchup_active)) {
            board->catchup_cancel = 1;
        }
        catchup_join(app);
    }
    if (!app->board_mu) {
        app->board_mu = SDL_CreateMutex();
        if (!app->board_mu) {
            fprintf(stderr, "catchup: mutex failed, running sync\n");
            group = r01s_island_builder_group(&app->builder);
            if (r01s_board_catchup_bringup(board, group) == 0) {
                (void)r01s_play_start(board);
            }
            return;
        }
    }
    app->catchup_board = board;
    app->catchup_rc = 0;
    board->catchup_cancel = 0;
    group = r01s_island_builder_group(&app->builder);
    if (group) {
        group->running = 0; /* don't double-step until stream done */
    }
    app->catchup_spin = 0;
    SDL_AtomicSet(&app->catchup_ui_req, 0);
    snprintf(app->ui.status, sizeof(app->ui.status), "Booting console…");
    SDL_AtomicSet(&app->catchup_active, 1);
    /* Paint boot before the worker races ahead of the first frame. */
    app_present_boot(app, app->catchup_spin);
    app->catchup_th = SDL_CreateThread(catchup_thread_fn_yielding, "r01s_catchup", app);
    if (!app->catchup_th) {
        fprintf(stderr, "catchup: thread failed (%s), running sync\n", SDL_GetError());
        SDL_AtomicSet(&app->catchup_active, 0);
        if (r01s_board_catchup_bringup(board, group) == 0) {
            (void)r01s_play_start(board);
        }
    }
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
        R01sEntity *e = b->mounts[i].entity;
        if (!e || e->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        if (r01s_ui_add_chip(&app->ui, e, b->mounts[i].island_index) != 0) {
            fprintf(stderr, "ui: dropped chip mount %d/%d (R01S_BOARD_MAX_CHIPS=%d)\n", i, b->mount_count,
                    R01S_BOARD_MAX_CHIPS);
        }
    }
    {
        int bom_ic = r01s_island_builder_count_visual(b, R01S_ENTITY_VIS_IC);
        if (bom_ic != R01S_BOM_IC_N) {
            fprintf(stderr, "ui: expected %d BOM IC visuals, mounted %d ui chips\n", R01S_BOM_IC_N, bom_ic);
        }
    }
    if (r01s_ui_layout_load(&app->ui) != 0) {
        /* No saved layout — keep builder defaults. */
    }
}

int r01s_app_init(R01sApp *app, int headless) {
    Uint32 flags;
    memset(app, 0, sizeof(*app));
    app->scale = headless ? 1 : 2;
    app->running = 1;
    SDL_AtomicSet(&app->catchup_active, 0);
    SDL_AtomicSet(&app->catchup_ui_req, 0);

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
    app->fps_last_ms = SDL_GetTicks();
    app->fps_frames = 0;

    if (r01s_ui_init(&app->ui) != 0) {
        SDL_Quit();
        return -1;
    }

    flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN;
    if (!headless) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    app->win = SDL_CreateWindow("retr01 Sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                R01S_LOGIC_W * app->scale, R01S_LOGIC_H * app->scale, flags);
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
    app->board_mu = SDL_CreateMutex();
    return 0;
}

void r01s_app_shutdown(R01sApp *app) {
    if (!app) {
        return;
    }
    if (app->catchup_board) {
        app->catchup_board->catchup_cancel = 1;
    }
    if (app->catchup_th) {
        catchup_join(app);
    }
    if (app->board_mu) {
        SDL_DestroyMutex(app->board_mu);
        app->board_mu = NULL;
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
    Uint32 now;
    int catching_up;

    group = r01s_island_builder_group(&app->builder);
    catching_up = r01s_app_catchup_active(app);
    if (!catching_up && app->catchup_th) {
        catchup_join(app);
        if (group) {
            group->running = 1;
        }
    }

    app->fps_frames++;
    now = SDL_GetTicks();
    if (now - app->fps_last_ms >= 1000u) {
        app->ui.fps = app->fps_frames;
        app->fps_frames = 0;
        app->fps_last_ms = now;
    }

    if (catching_up) {
        /*
         * Black boot screen. Worker signals catchup_ui_req after each batch;
         * main advances the spinner and acks — never blocks on board_mu.
         */
        if (SDL_AtomicGet(&app->catchup_ui_req)) {
            app->catchup_spin = (app->catchup_spin + 1) & 3;
            SDL_AtomicSet(&app->catchup_ui_req, 0);
        }
        SDL_SetRenderTarget(app->ren, app->target);
        r01s_ui_draw_boot(&app->ui, app->ren, app->catchup_spin);
        SDL_SetRenderTarget(app->ren, NULL);
    } else {
        if (app->board_mu) {
            SDL_LockMutex(app->board_mu);
        }

        r01s_ui_sync_gamepads(&app->ui);
        board = r01s_board_from_group(group);
        if (board) {
            uint8_t pad0 = r01s_ui_gamepad_port(&app->ui, 0);
            r01s_pads_set(&board->pads, 0, pad0);
            r01s_pads_set(&board->pads, 1, r01s_ui_gamepad_port(&app->ui, 1));
            r01s_pads_refresh_preview(&board->pads);
            app->ui.probe_pad_p1 = r01s_pads_get(&board->pads, 0);
            app->ui.probe_pad_p2 = r01s_pads_get(&board->pads, 1);
            r01s_play_tick(board, pad0);
        }
        if (group) {
            if (group->running) {
                int n = 0;
                if (board && board->play.enabled) {
                    Uint64 t0 = SDL_GetPerformanceCounter();
                    Uint64 freq = SDL_GetPerformanceFrequency();
                    Uint64 budget = (freq * (Uint64)R01S_SIM_BUDGET_MS_PLAY) / 1000u;
                    while (n < R01S_SIM_MAX_STEPS_PER_FRAME) {
                        r01s_island_group_step(group);
                        n++;
                        if ((SDL_GetPerformanceCounter() - t0) >= budget) {
                            break;
                        }
                    }
                } else {
                    Uint64 t0 = SDL_GetPerformanceCounter();
                    Uint64 freq = SDL_GetPerformanceFrequency();
                    Uint64 budget = (freq * (Uint64)R01S_SIM_BUDGET_MS) / 1000u;
                    while (n < R01S_SIM_MAX_STEPS_PER_FRAME) {
                        r01s_island_group_step(group);
                        n++;
                        if ((SDL_GetPerformanceCounter() - t0) >= budget) {
                            break;
                        }
                    }
                }
                app->ui.sim_steps = n;
                if (board && r01s_video_sink_render_mode(&board->video_sink) == R01S_VIDEO_RENDER_PHOSPHOR) {
                    int bx = r01s_beam_xy_x(board->beam_impl.beam_x);
                    int by = r01s_beam_xy_y(board->beam_impl.beam_x);
                    r01s_video_sink_display_tick(&board->video_sink, bx, by);
                }
            } else {
                r01s_island_group_eval_idle(group);
                app->ui.sim_steps = 0;
            }
            r01s_island_group_fill_status(group, app->ui.status, sizeof(app->ui.status));
            r01s_island_group_fill_health(group, &app->ui.health);
            r01s_island_group_update_probes(group, &app->ui.probe_vdd, &app->ui.probe_phi2,
                                           &app->ui.probe_resb_low);
        }

        SDL_SetRenderTarget(app->ren, app->target);
        r01s_ui_draw(&app->ui, app->ren);
        SDL_SetRenderTarget(app->ren, NULL);

        if (app->board_mu) {
            SDL_UnlockMutex(app->board_mu);
        }
    }

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

/* 1 = proceed with quit, 0 = stay open. */
static int app_confirm_quit(R01sApp *app) {
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Save"},
        {0, 1, "Don't Save"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel"},
    };
    SDL_MessageBoxData data;
    int button = 2;

    if (!app) {
        return 1;
    }
    if (!app->ui.layout_dirty) {
        return 1;
    }
    memset(&data, 0, sizeof(data));
    data.flags = SDL_MESSAGEBOX_WARNING;
    data.window = app->win;
    data.title = "Unsaved layout";
    data.message = "Layout has changed. Save layout before closing?";
    data.numbuttons = 3;
    data.buttons = buttons;
    if (SDL_ShowMessageBox(&data, &button) < 0) {
        fprintf(stderr, "layout: quit prompt failed (%s)\n", SDL_GetError());
        return 1;
    }
    if (button == 0) {
        if (r01s_ui_layout_save(&app->ui) != 0) {
            fprintf(stderr, "layout: save failed on quit\n");
        }
        return 1;
    }
    if (button == 1) {
        return 1;
    }
    return 0;
}

void r01s_app_handle_event(R01sApp *app, const SDL_Event *e) {
    int lx, ly;
    R01sIslandGroup *group;
    if (!app || !e) {
        return;
    }
    group = r01s_island_builder_group(&app->builder);
    if (e->type == SDL_QUIT) {
        if (!app_confirm_quit(app)) {
            return;
        }
        if (app->catchup_board) {
            app->catchup_board->catchup_cancel = 1;
        }
        app->running = 0;
        return;
    }
    if (e->type == SDL_KEYDOWN && group) {
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE:
            if (!app_confirm_quit(app)) {
                return;
            }
            if (app->catchup_board) {
                app->catchup_board->catchup_cancel = 1;
            }
            app->running = 0;
            return;
        case SDLK_SPACE:
            if (r01s_app_catchup_active(app)) {
                return;
            }
            group->running = !group->running;
            return;
        case SDLK_r:
            if (e->key.keysym.mod & KMOD_CTRL) {
                R01sBoard *b = r01s_board_from_group(group);
                if (r01s_app_catchup_active(app) && b) {
                    b->catchup_cancel = 1;
                    catchup_join(app);
                }
                if (app->board_mu) {
                    SDL_LockMutex(app->board_mu);
                }
                r01s_island_group_reset(group);
                if (app->board_mu) {
                    SDL_UnlockMutex(app->board_mu);
                }
                if (b) {
                    r01s_app_start_ic_catchup(app, b);
                }
                snprintf(app->ui.status, sizeof(app->ui.status), "simulation reset — IC stream…");
                return;
            }
            /* Bare R falls through to UI (rotate selected IC). */
            break;
        case SDLK_PERIOD:
            if (r01s_app_catchup_active(app)) {
                return;
            }
            if (!group->running) {
                if (app->board_mu) {
                    SDL_LockMutex(app->board_mu);
                }
                r01s_island_group_step(group);
                if (app->board_mu) {
                    SDL_UnlockMutex(app->board_mu);
                }
            }
            return;
        default:
            break;
        }
    }
    if (r01s_app_catchup_active(app)) {
        /* Ignore board interaction while worker owns the netlist. */
        return;
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
