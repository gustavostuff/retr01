#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/video.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R01E_DEFAULT_CART "../retr01_studio/test_game/test.retr01"

/* Debug pane: VRAM atlas (256x240) + world map beside it. */
#define DBG_GAP 8
#define DBG_MAP_CELL 20
#define DBG_MAP_MAX_CELLS 8
#define DBG_MAP_W (DBG_MAP_CELL * DBG_MAP_MAX_CELLS)
#define DBG_MAP_H (DBG_MAP_CELL * DBG_MAP_MAX_CELLS)
#define DBG_WIN_W (R01E_VRAM_ATLAS_W + DBG_GAP + DBG_MAP_W)
#define DBG_WIN_H R01E_VRAM_ATLAS_H

static uint8_t read_pads(const Uint8 *keys) {
    uint8_t b = 0;

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

static void draw_world_map(SDL_Renderer *ren, R01eMachine *m, int ox, int oy) {
    R01eWorldView wv;
    const uint8_t *dir;
    uint8_t present[DBG_MAP_MAX_CELLS][DBG_MAP_MAX_CELLS];
    int si, c, r;
    int cur_c, cur_r;
    int min_c, min_r, max_c, max_r;
    int cols, rows;
    int map_ox, map_oy;

    memset(present, 0, sizeof(present));
    min_c = min_r = DBG_MAP_MAX_CELLS;
    max_c = max_r = -1;
    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) != 0) {
        return;
    }
    dir = r01e_cart_ptr(&m->cart, wv.base + wv.off_screen_dir, (size_t)wv.screen_count * 12u);
    if (dir) {
        for (si = 0; si < wv.screen_count; si++) {
            const uint8_t *e = dir + (size_t)si * 12u;
            int col = (int)e[0];
            int row = (int)e[1];
            if (col >= 0 && col < DBG_MAP_MAX_CELLS && row >= 0 && row < DBG_MAP_MAX_CELLS) {
                present[row][col] = 1;
                if (col < min_c) {
                    min_c = col;
                }
                if (row < min_r) {
                    min_r = row;
                }
                if (col > max_c) {
                    max_c = col;
                }
                if (row > max_r) {
                    max_r = row;
                }
            }
        }
    }
    if (max_c < min_c || max_r < min_r) {
        return;
    }
    cols = max_c - min_c + 1;
    rows = max_r - min_r + 1;

    if (m->play.enabled) {
        cur_c = (m->play.player_x + R01E_PLAY_PLAYER_SIZE / 2) / R01E_SCREEN_PX_W;
        cur_r = (m->play.player_y + R01E_PLAY_PLAYER_SIZE / 2) / R01E_SCREEN_PX_H;
    } else {
        cur_c = m->video.cam_origin_col;
        cur_r = m->video.cam_origin_row;
    }

    /* Center the used bounding box in the map pane. */
    map_ox = ox + (DBG_MAP_W - DBG_MAP_CELL * cols) / 2;
    map_oy = oy + (DBG_WIN_H - DBG_MAP_CELL * rows) / 2;

    for (r = min_r; r <= max_r; r++) {
        for (c = min_c; c <= max_c; c++) {
            SDL_Rect cell;
            cell.x = map_ox + (c - min_c) * DBG_MAP_CELL + 1;
            cell.y = map_oy + (r - min_r) * DBG_MAP_CELL + 1;
            cell.w = DBG_MAP_CELL - 2;
            cell.h = DBG_MAP_CELL - 2;
            if (c == cur_c && r == cur_r && present[r][c]) {
                SDL_SetRenderDrawColor(ren, 255, 200, 40, 255); /* current screen */
            } else if (present[r][c]) {
                SDL_SetRenderDrawColor(ren, 70, 110, 150, 255); /* present */
            } else {
                SDL_SetRenderDrawColor(ren, 28, 30, 36, 255); /* hole in bbox */
            }
            SDL_RenderFillRect(ren, &cell);
            SDL_SetRenderDrawColor(ren, 18, 20, 24, 255);
            SDL_RenderDrawRect(ren, &cell);
        }
    }
}

/* Player in 2x2 VRAM atlas space (cam_origin-relative), clipped to the buffer. */
static void draw_vram_player(SDL_Renderer *ren, const R01eMachine *m) {
    SDL_Rect cell;
    int ax, ay, x0, y0, x1, y1;

    if (!m->play.enabled) {
        return;
    }
    ax = m->play.player_x - m->video.cam_origin_col * R01E_SCREEN_PX_W;
    ay = m->play.player_y - m->video.cam_origin_row * R01E_SCREEN_PX_H;
    x0 = ax;
    y0 = ay;
    x1 = ax + R01E_PLAY_PLAYER_SIZE;
    y1 = ay + R01E_PLAY_PLAYER_SIZE;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > R01E_VRAM_ATLAS_W) {
        x1 = R01E_VRAM_ATLAS_W;
    }
    if (y1 > R01E_VRAM_ATLAS_H) {
        y1 = R01E_VRAM_ATLAS_H;
    }
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    cell.x = x0;
    cell.y = y0;
    cell.w = x1 - x0;
    cell.h = y1 - y0;
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderFillRect(ren, &cell);
    SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
    SDL_RenderDrawRect(ren, &cell);
}

static void present_debug_pane(SDL_Renderer *dbg_ren, SDL_Texture *vram_tex, R01eMachine *m) {
    SDL_Rect dst;
    SDL_Rect vp;

    r01e_video_render_vram_atlas(m);
    SDL_UpdateTexture(vram_tex, NULL, m->video.vram_atlas, R01E_VRAM_ATLAS_W * 3);

    SDL_SetRenderDrawColor(dbg_ren, 12, 14, 18, 255);
    SDL_RenderClear(dbg_ren);

    dst.x = 0;
    dst.y = 0;
    dst.w = R01E_VRAM_ATLAS_W;
    dst.h = R01E_VRAM_ATLAS_H;
    SDL_RenderCopy(dbg_ren, vram_tex, NULL, &dst);

    vp.x = (int)m->io.scroll_x;
    vp.y = (int)m->io.scroll_y;
    vp.w = R01E_SCREEN_PX_W;
    vp.h = R01E_SCREEN_PX_H;
    SDL_SetRenderDrawColor(dbg_ren, 255, 40, 40, 255);
    SDL_RenderDrawRect(dbg_ren, &vp);
    vp.x += 1;
    vp.y += 1;
    vp.w -= 2;
    vp.h -= 2;
    if (vp.w > 0 && vp.h > 0) {
        SDL_RenderDrawRect(dbg_ren, &vp);
    }

    draw_vram_player(dbg_ren, m);
    draw_world_map(dbg_ren, m, R01E_VRAM_ATLAS_W + DBG_GAP, 0);
    SDL_RenderPresent(dbg_ren);
}

int main(int argc, char **argv) {
    const char *path = argc >= 2 ? argv[1] : R01E_DEFAULT_CART;
    char err[256];
    R01eMachine machine;
    SDL_Window *win = NULL;
    SDL_Window *dbg_win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Renderer *dbg_ren = NULL;
    SDL_Texture *tex = NULL;
    SDL_Texture *vram_tex = NULL;
    int scale = 3;
    int running = 1;
    int paused = 0;
    Uint32 last_ticks;
    int main_x = 0, main_y = 0, main_w = 0, main_h = 0;

    if (r01e_machine_init(&machine, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "retr01_emu: %s\n", err);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        return 1;
    }
    /* Before any renderer/texture: nearest-neighbor upscale. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    /* Hidden until first frame is presented — avoids empty-window flash. */
    win = SDL_CreateWindow("Retr01 Emulator (Phase 1)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           R01E_VISIBLE_W * scale, R01E_VISIBLE_H * scale,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(ren, R01E_VISIBLE_W, R01E_VISIBLE_H);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, R01E_VISIBLE_W,
                            R01E_VISIBLE_H);
    if (!tex) {
        fprintf(stderr, "SDL texture: %s\n", SDL_GetError());
        r01e_machine_shutdown(&machine);
        SDL_Quit();
        return 1;
    }

    /* One debug window: VRAM 2x2 (left) + world map (right). */
    SDL_GetWindowPosition(win, &main_x, &main_y);
    SDL_GetWindowSize(win, &main_w, &main_h);
    dbg_win = SDL_CreateWindow("VRAM + world map", main_x + main_w + 16, main_y, DBG_WIN_W, DBG_WIN_H,
                               SDL_WINDOW_HIDDEN);
    dbg_ren = dbg_win ? SDL_CreateRenderer(dbg_win, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (dbg_ren) {
        SDL_RenderSetLogicalSize(dbg_ren, DBG_WIN_W, DBG_WIN_H);
        vram_tex = SDL_CreateTexture(dbg_ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                     R01E_VRAM_ATLAS_W, R01E_VRAM_ATLAS_H);
    }
    if (!dbg_win || !dbg_ren || !vram_tex) {
        fprintf(stderr, "retr01_emu: debug window unavailable (%s) — continuing without it\n",
                SDL_GetError());
        if (vram_tex) {
            SDL_DestroyTexture(vram_tex);
            vram_tex = NULL;
        }
        if (dbg_ren) {
            SDL_DestroyRenderer(dbg_ren);
            dbg_ren = NULL;
        }
        if (dbg_win) {
            SDL_DestroyWindow(dbg_win);
            dbg_win = NULL;
        }
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
    if (dbg_win) {
        printf("Debug: VRAM 2x2 (red viewport) + world map (gold = current screen)\n");
    }

    /* Present boot frame while still hidden, then show. */
    SDL_UpdateTexture(tex, NULL, machine.video.fb, R01E_VISIBLE_W * 3);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    if (dbg_win && dbg_ren && vram_tex) {
        present_debug_pane(dbg_ren, vram_tex, &machine);
        SDL_ShowWindow(dbg_win);
    }
    SDL_ShowWindow(win);

    last_ticks = SDL_GetTicks();
    while (running) {
        SDL_Event ev;
        const Uint8 *keys;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (dbg_win && (Uint32)ev.window.windowID == SDL_GetWindowID(dbg_win)) {
                    SDL_DestroyTexture(vram_tex);
                    SDL_DestroyRenderer(dbg_ren);
                    SDL_DestroyWindow(dbg_win);
                    vram_tex = NULL;
                    dbg_ren = NULL;
                    dbg_win = NULL;
                } else {
                    running = 0;
                }
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
            if ((int)(now - last_ticks) >= 16) {
                (void)r01e_machine_frame(&machine);
                last_ticks = now;
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

        if (dbg_win && dbg_ren && vram_tex) {
            present_debug_pane(dbg_ren, vram_tex, &machine);
        }
    }

    if (vram_tex) {
        SDL_DestroyTexture(vram_tex);
    }
    if (dbg_ren) {
        SDL_DestroyRenderer(dbg_ren);
    }
    if (dbg_win) {
        SDL_DestroyWindow(dbg_win);
    }
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    r01e_machine_shutdown(&machine);
    return 0;
}
