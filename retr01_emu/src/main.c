#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/video.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>

#define R01E_DEFAULT_CART "../retr01_studio/test_game/test.retr01"

static uint8_t read_pads(const Uint8 *keys) {
    uint8_t b = 0;

    /* Match Studio Play: WASD / arrows move; X/Y warp buttons. */
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        b |= R01E_PAD_RIGHT;
    }
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
        b |= R01E_PAD_LEFT;
    }
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
        b |= R01E_PAD_DOWN;
    }
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
        b |= R01E_PAD_UP;
    }
    return b;
}

static void on_key_down(R01eMachine *m, SDL_Keycode sym) {
    /* Studio Play: X → screen (0,0), Y → screen (1,0) */
    if (sym == SDLK_x) {
        m->io.pad0 = (uint8_t)(m->io.pad0 | R01E_PAD_X);
        r01e_play_tick(m);
        r01e_video_render_frame(m);
        r01e_play_draw(m);
        m->play.pad_prev = m->io.pad0;
        m->io.pad0 = (uint8_t)(m->io.pad0 & (uint8_t)~R01E_PAD_X);
    } else if (sym == SDLK_y) {
        m->io.pad0 = (uint8_t)(m->io.pad0 | R01E_PAD_Y);
        r01e_play_tick(m);
        r01e_video_render_frame(m);
        r01e_play_draw(m);
        m->play.pad_prev = m->io.pad0;
        m->io.pad0 = (uint8_t)(m->io.pad0 & (uint8_t)~R01E_PAD_Y);
    }
}

int main(int argc, char **argv) {
    const char *path = argc >= 2 ? argv[1] : R01E_DEFAULT_CART;
    char err[256];
    R01eMachine machine;
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL;
    int scale = 3;
    int running = 1;
    int paused = 0;
    Uint32 last_ticks;

    if (r01e_machine_init(&machine, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "retr01_emu: %s\n", err);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        return 1;
    }

    win = SDL_CreateWindow("Retr01 Emulator (Phase 1)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           R01E_VISIBLE_W * scale, R01E_VISIBLE_H * scale, SDL_WINDOW_RESIZABLE);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetLogicalSize(ren, R01E_VISIBLE_W, R01E_VISIBLE_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, R01E_VISIBLE_W,
                            R01E_VISIBLE_H);
    if (!tex) {
        fprintf(stderr, "SDL texture: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        SDL_Quit();
        return 1;
    }

    printf("retr01_emu: %s (%zu bytes)\n", path, machine.cart.len);
    {
        R01eWorldView wv;
        if (r01e_cart_world(&machine.cart, 0, &wv) == 0) {
            printf("  world 0: %u present screens, start (%u,%u)\n", (unsigned)wv.screen_count,
                   (unsigned)wv.start_col, (unsigned)wv.start_row);
        }
    }
    printf("Studio Play SoT: WASD/arrows move · X/Y warp · Space pause · R reset · Esc quit\n");

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
                } else {
                    on_key_down(&machine, ev.key.keysym.sym);
                }
            }
        }

        keys = SDL_GetKeyboardState(NULL);
        r01e_machine_set_pad(&machine, 0, read_pads(keys));

        if (!paused) {
            Uint32 now = SDL_GetTicks();
            while ((int)(now - last_ticks) >= 16) {
                (void)r01e_machine_frame(&machine);
                last_ticks += 16;
                if ((int)(now - last_ticks) > 100) {
                    last_ticks = now;
                    break;
                }
            }
        } else {
            r01e_video_render_frame(&machine);
            r01e_play_draw(&machine);
        }

        SDL_UpdateTexture(tex, NULL, machine.video.fb, R01E_VISIBLE_W * 3);
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
