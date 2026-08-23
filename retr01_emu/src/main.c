#include "retr01_emu/machine.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t read_pads(const Uint8 *keys, int p2) {
    uint8_t b = 0;
    if (!p2) {
        if (keys[SDL_SCANCODE_RIGHT]) {
            b |= R01E_PAD_RIGHT;
        }
        if (keys[SDL_SCANCODE_LEFT]) {
            b |= R01E_PAD_LEFT;
        }
        if (keys[SDL_SCANCODE_DOWN]) {
            b |= R01E_PAD_DOWN;
        }
        if (keys[SDL_SCANCODE_UP]) {
            b |= R01E_PAD_UP;
        }
        if (keys[SDL_SCANCODE_Z]) {
            b |= R01E_PAD_X;
        }
        if (keys[SDL_SCANCODE_X]) {
            b |= R01E_PAD_Y;
        }
        if (keys[SDL_SCANCODE_1]) {
            b |= R01E_PAD_COIN;
        }
        if (keys[SDL_SCANCODE_RETURN]) {
            b |= R01E_PAD_START;
        }
    } else {
        if (keys[SDL_SCANCODE_D]) {
            b |= R01E_PAD_RIGHT;
        }
        if (keys[SDL_SCANCODE_A]) {
            b |= R01E_PAD_LEFT;
        }
        if (keys[SDL_SCANCODE_S]) {
            b |= R01E_PAD_DOWN;
        }
        if (keys[SDL_SCANCODE_W]) {
            b |= R01E_PAD_UP;
        }
        if (keys[SDL_SCANCODE_N]) {
            b |= R01E_PAD_X;
        }
        if (keys[SDL_SCANCODE_M]) {
            b |= R01E_PAD_Y;
        }
        if (keys[SDL_SCANCODE_2]) {
            b |= R01E_PAD_COIN;
        }
        if (keys[SDL_SCANCODE_BACKSPACE]) {
            b |= R01E_PAD_START;
        }
    }
    return b;
}

int main(int argc, char **argv) {
    const char *path = NULL;
    char err[256];
    R01eMachine machine;
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL;
    int scale = 3;
    int running = 1;
    int paused = 0;
    Uint32 last_ticks;

    if (argc >= 2) {
        path = argv[1];
    } else {
        path = "../studio/project.retr01";
    }

    if (r01e_machine_init(&machine, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "retr01_emu: %s\n", err);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        return 1;
    }

    win = SDL_CreateWindow("Retr01 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           R01E_VISIBLE_W * scale, R01E_VISIBLE_H * scale, SDL_WINDOW_RESIZABLE);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(ren, R01E_VISIBLE_W, R01E_VISIBLE_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, R01E_VISIBLE_W,
                            R01E_VISIBLE_H);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        SDL_Quit();
        return 1;
    }

    printf("retr01_emu: loaded %s (%zu bytes, %u worlds)\n", path, machine.cart.len,
           (unsigned)machine.cart.world_count);
    printf("Controls: Arrows/ZX/1/Enter = P1 · WASD/NM/2/Bksp = P2 · Space pause · R reset · Esc quit\n");

    last_ticks = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        const Uint8 *keys;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                } else if (ev.key.keysym.sym == SDLK_SPACE) {
                    paused = !paused;
                } else if (ev.key.keysym.sym == SDLK_r) {
                    r01e_machine_reset(&machine);
                }
            }
        }

        keys = SDL_GetKeyboardState(NULL);
        r01e_machine_set_pad(&machine, 0, read_pads(keys, 0));
        r01e_machine_set_pad(&machine, 1, read_pads(keys, 1));

        if (!paused) {
            /* Catch up to ~60 Hz wall clock with machine frames. */
            Uint32 now = SDL_GetTicks();
            while ((int)(now - last_ticks) >= 16) {
                (void)r01e_machine_frame(&machine);
                last_ticks += 16;
                if ((int)(now - last_ticks) > 100) {
                    last_ticks = now;
                    break;
                }
            }
        }

        SDL_UpdateTexture(tex, NULL, machine.ppu.fb, R01E_VISIBLE_W * 3);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    r01e_machine_shutdown(&machine);
    return 0;
}
