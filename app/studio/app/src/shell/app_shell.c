#include "shell/app_shell.h"
#include "ui/internal.h"

#include <stdio.h>
#include <string.h>

static void logic_from_window(const AppShell *app, int win_x, int win_y, int *lx, int *ly) {
    int ww, wh, draw_w, draw_h, ox, oy, scale;
    SDL_GetWindowSize(app->win, &ww, &wh);
    scale = app->scale > 0 ? app->scale : 1;
    draw_w = ui_logic_w(&app->ui) * scale;
    draw_h = ui_logic_h(&app->ui) * scale;
    ox = (ww - draw_w) / 2;
    oy = (wh - draw_h) / 2;
    *lx = (win_x - ox) / scale;
    *ly = (win_y - oy) / scale;
}

static int recreate_target(AppShell *app) {
    int lw = ui_logic_w(&app->ui);
    int lh = ui_logic_h(&app->ui);
    if (app->target) {
        SDL_DestroyTexture(app->target);
        app->target = NULL;
    }
    app->target = SDL_CreateTexture(app->ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, lw, lh);
    if (!app->target) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return -1;
    }
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(app->target, SDL_ScaleModeNearest);
#endif
    return 0;
}

void app_shell_set_render_scale(AppShell *app, int scale) {
    int lw;
    int lh;
    if (!app || !app->win) {
        return;
    }
    if (scale < 1) {
        scale = 1;
    }
    if (scale > 2) {
        scale = 2;
    }
    app->render_scale = scale;
    app->scale = scale;
    app->ui.scale = scale;
    lw = ui_logic_w(&app->ui);
    lh = ui_logic_h(&app->ui);
    SDL_SetWindowSize(app->win, lw * scale, lh * scale);
}

static void app_shell_set_render_scale_toast(AppShell *app, int scale) {
    app_shell_set_render_scale(app, scale);
    ui_toast(&app->ui, scale == 2 ? "scale 2x" : "scale 1x", 0);
}

void app_shell_apply_logic_scale(AppShell *app) {
    if (!app || !app->win || !app->ren) {
        return;
    }
    if (recreate_target(app) != 0) {
        return;
    }
    if (app->render_scale < 1) {
        app->render_scale = 2;
    }
    app_shell_set_render_scale(app, app->render_scale);
}

int app_shell_init(AppShell *app, int headless) {
    Uint32 flags;
    int lw;
    int lh;
    memset(app, 0, sizeof(*app));

    if (headless) {
        if (!getenv("SDL_VIDEODRIVER")) {
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen");
        }
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); /* nearest when stretching logic canvas */

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    if (ui_init(&app->ui) != 0) {
        SDL_Quit();
        return -1;
    }
    app->render_scale = 2;
    app->scale = 2;
    app->ui.scale = 2;
    lw = ui_logic_w(&app->ui);
    lh = ui_logic_h(&app->ui);

    flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

    app->win = SDL_CreateWindow("Retr01 Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, lw * app->render_scale,
                                lh * app->render_scale, flags);
    if (!app->win) {
        ui_shutdown(&app->ui);
        SDL_Quit();
        return -1;
    }
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    app->ren = SDL_CreateRenderer(app->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app->ren) {
        app->ren = SDL_CreateRenderer(app->win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!app->ren) {
        app_shell_shutdown(app);
        return -1;
    }
    SDL_SetRenderDrawBlendMode(app->ren, SDL_BLENDMODE_BLEND);

    if (recreate_target(app) != 0) {
        app_shell_shutdown(app);
        return -1;
    }
    app_shell_frame(app);
    if (!headless) {
        SDL_ShowWindow(app->win);
    }
    return 0;
}

void app_shell_shutdown(AppShell *app) {
    if (!app) {
        return;
    }
    if (app->target) {
        SDL_DestroyTexture(app->target);
    }
    if (app->ren) {
        SDL_DestroyRenderer(app->ren);
    }
    if (app->win) {
        SDL_DestroyWindow(app->win);
    }
    ui_shutdown(&app->ui);
    SDL_Quit();
}

void app_shell_draw(AppShell *app) {
    SDL_SetRenderTarget(app->ren, app->target);
    ui_draw(&app->ui, app->ren);
    SDL_SetRenderTarget(app->ren, NULL);
}

void app_shell_frame(AppShell *app) {
    int ww, wh, sx, sy, scale;
    SDL_Rect dst;
    SDL_GetWindowSize(app->win, &ww, &wh);
    sx = ww / ui_logic_w(&app->ui);
    sy = wh / ui_logic_h(&app->ui);
    scale = sx < sy ? sx : sy;
    if (scale < 1) {
        scale = 1;
    }
    app->scale = scale;
    app->ui.scale = scale;
    dst.w = ui_logic_w(&app->ui) * scale;
    dst.h = ui_logic_h(&app->ui) * scale;
    dst.x = (ww - dst.w) / 2;
    dst.y = (wh - dst.h) / 2;

    ui_tick(&app->ui);
    app_shell_draw(app);
    SDL_SetRenderDrawColor(app->ren, 0, 0, 0, 255);
    SDL_RenderClear(app->ren);
    SDL_RenderCopy(app->ren, app->target, NULL, &dst);
    SDL_RenderPresent(app->ren);
    if (app->ui.play.booting) {
        ui_play_boot_finish(&app->ui, app->ren);
    }
}

int app_shell_handle_event(AppShell *app, const SDL_Event *e) {
    int wx = 0, wy = 0, lx = 0, ly = 0, rc;
    if (e->type == SDL_KEYDOWN && (e->key.keysym.mod & KMOD_CTRL) && !(e->key.keysym.mod & KMOD_SHIFT) &&
        !(e->key.keysym.mod & KMOD_ALT)) {
        if (e->key.keysym.sym == SDLK_1) {
            app_shell_set_render_scale_toast(app, 1);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_2) {
            app_shell_set_render_scale_toast(app, 2);
            return 1;
        }
    }
    if (e->type == SDL_DROPFILE) {
        rc = ui_handle_drop_file(&app->ui, e->drop.file, 0, 0);
        SDL_free(e->drop.file);
        return rc;
    }
    if (e->type == SDL_MOUSEMOTION) {
        wx = e->motion.x;
        wy = e->motion.y;
    } else if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        wx = e->button.x;
        wy = e->button.y;
    }
    logic_from_window(app, wx, wy, &lx, &ly);
    rc = ui_handle_event(&app->ui, e, lx, ly);
    if (rc == 2) {
        Uint32 f = SDL_GetWindowFlags(app->win);
        SDL_SetWindowFullscreen(app->win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else if (rc == 4) {
        ui_toggle_logic_scale(&app->ui);
        app_shell_apply_logic_scale(app);
    }
    return rc;
}
