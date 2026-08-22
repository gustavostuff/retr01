#include "ui.h"

#include <SDL.h>
#include <stdio.h>

static void logic_from_window(int win_x, int win_y, int win_w, int win_h, int scale, int *lx, int *ly) {
    int draw_w = UI_LOGIC_W * scale;
    int draw_h = UI_LOGIC_H * scale;
    int ox = (win_w - draw_w) / 2;
    int oy = (win_h - draw_h) / 2;
    *lx = (win_x - ox) / scale;
    *ly = (win_y - oy) / scale;
}

int main(int argc, char **argv) {
    UiState ui;
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *target = NULL;
    int running = 1;
    Uint32 flags;

    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    ui_init(&ui);
    if (!ui.project) {
        fprintf(stderr, "oom\n");
        return 1;
    }

    flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    win = SDL_CreateWindow("Retr01 Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, UI_LOGIC_W * ui.scale,
                           UI_LOGIC_H * ui.scale, flags);
    if (!win) {
        fprintf(stderr, "window: %s\n", SDL_GetError());
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, 0);
    }
    if (!ren) {
        fprintf(stderr, "renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    target = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, UI_LOGIC_W, UI_LOGIC_H);
    if (!target) {
        fprintf(stderr, "texture: %s\n", SDL_GetError());
        return 1;
    }

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else {
                int wx = 0, wy = 0, ww, wh, lx, ly, rc;
                SDL_GetWindowSize(win, &ww, &wh);
                if (e.type == SDL_MOUSEMOTION) {
                    wx = e.motion.x;
                    wy = e.motion.y;
                } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
                    wx = e.button.x;
                    wy = e.button.y;
                } else if (e.type == SDL_MOUSEWHEEL) {
                    SDL_GetMouseState(&wx, &wy);
                }
                logic_from_window(wx, wy, ww, wh, ui.scale, &lx, &ly);
                rc = ui_handle_event(&ui, &e, lx, ly);
                if (rc == 2) {
                    Uint32 f = SDL_GetWindowFlags(win);
                    if (f & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        SDL_SetWindowFullscreen(win, 0);
                    } else {
                        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    /* keep integer scale fitting */
                    SDL_GetWindowSize(win, &ww, &wh);
                    {
                        int sx = ww / UI_LOGIC_W;
                        int sy = wh / UI_LOGIC_H;
                        ui.scale = sx < sy ? sx : sy;
                        if (ui.scale < 1) {
                            ui.scale = 1;
                        }
                    }
                }
            }
        }

        {
            int ww, wh, sx, sy, scale;
            SDL_Rect dst;
            SDL_GetWindowSize(win, &ww, &wh);
            sx = ww / UI_LOGIC_W;
            sy = wh / UI_LOGIC_H;
            scale = sx < sy ? sx : sy;
            if (scale < 1) {
                scale = 1;
            }
            ui.scale = scale;
            dst.w = UI_LOGIC_W * scale;
            dst.h = UI_LOGIC_H * scale;
            dst.x = (ww - dst.w) / 2;
            dst.y = (wh - dst.h) / 2;

            ui_tick(&ui);

            SDL_SetRenderTarget(ren, target);
            ui_draw(&ui, ren);
            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, target, NULL, &dst);
            SDL_RenderPresent(ren);
        }
    }

    SDL_DestroyTexture(target);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    ui_shutdown(&ui);
    SDL_Quit();
    return 0;
}
