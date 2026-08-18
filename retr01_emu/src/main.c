#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/palette.h"
#include "retr01/screen.h"
#include "retr01_emu/emu.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s --cart path.retr01 [--world N] [--col C] [--row R]\n"
            "          [--scale N] [--dump-fb out.raw] [--headless] [--frames N]\n"
            "          [--cpu | --view]\n",
            argv0);
}

static int load_palette(retr01_master_palette_t *pal)
{
    const char *paths[] = {
        "retr01_world_studio/retr01_palette_v_01.txt",
        "../retr01_world_studio/retr01_palette_v_01.txt",
        RETR01_PALETTE_V01_PATH,
        NULL,
    };
    size_t i;
    for (i = 0; paths[i]; i++) {
        if (retr01_palette_load_v01(paths[i], pal) == 0) {
            return 0;
        }
    }
    retr01_palette_set_defaults(pal);
    return -1;
}

static int show_screen(retr01_cart_t *cart, retr01_ppu_t *ppu, int world, int col, int row,
                       uint8_t *rgba)
{
    retr01_screen_t screen;

    if (retr01_map_load_screen(cart, world, (uint8_t)col, (uint8_t)row, &screen) != 0) {
        return -1;
    }
    ppu->screen = screen;
    retr01_ppu_render_bg(ppu, rgba);
    return 0;
}

static void print_map_index(const retr01_cart_t *cart)
{
    int w;
    int worlds = retr01_map_world_count(cart);
    if (worlds < 0) {
        worlds = 0;
    }
    printf("MAP: %d world(s)\n", worlds);
    for (w = 0; w < RETR01_MAX_WORLDS; w++) {
        retr01_map_cell_t cells[RETR01_MAX_SCREENS_PER_WORLD];
        int count = 0;
        int i;
        if (retr01_map_list_cells(cart, w, cells, RETR01_MAX_SCREENS_PER_WORLD, &count) != 0 ||
            count <= 0) {
            continue;
        }
        printf("  world %d:", w);
        for (i = 0; i < count; i++) {
            printf(" (%u,%u)", cells[i].col, cells[i].row);
        }
        printf("\n");
    }
    printf("Keys: N/P cycle rooms, arrows step grid, [/] world, Esc quit\n");
}

static uint8_t pad_from_keyboard(void)
{
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    uint8_t pad = 0;
    if (keys[SDL_SCANCODE_RIGHT]) {
        pad |= RETR01_PAD_RIGHT;
    }
    if (keys[SDL_SCANCODE_LEFT]) {
        pad |= RETR01_PAD_LEFT;
    }
    if (keys[SDL_SCANCODE_DOWN]) {
        pad |= RETR01_PAD_DOWN;
    }
    if (keys[SDL_SCANCODE_UP]) {
        pad |= RETR01_PAD_UP;
    }
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_PERIOD]) {
        pad |= RETR01_PAD_A;
    }
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_SLASH]) {
        pad |= RETR01_PAD_B;
    }
    if (keys[SDL_SCANCODE_RSHIFT] || keys[SDL_SCANCODE_A]) {
        pad |= RETR01_PAD_SELECT;
    }
    if (keys[SDL_SCANCODE_RETURN]) {
        pad |= RETR01_PAD_START;
    }
    return pad;
}

static int load_cells(const retr01_cart_t *cart, int world, retr01_map_cell_t *cells, int *count)
{
    return retr01_map_list_cells(cart, world, cells, RETR01_MAX_SCREENS_PER_WORLD, count);
}

static int index_of_cell(const retr01_map_cell_t *cells, int count, int col, int row)
{
    int i;
    for (i = 0; i < count; i++) {
        if (cells[i].col == (uint8_t)col && cells[i].row == (uint8_t)row) {
            return i;
        }
    }
    return -1;
}

static void set_title(SDL_Window *window, int world, int col, int row, int index, int count)
{
    char title[160];
    snprintf(title, sizeof(title),
             "retr01_emu  world %d  (%d,%d)  room %d/%d  — N/P rooms, arrows, [/] world", world,
             col, row, index >= 0 ? index + 1 : 0, count);
    SDL_SetWindowTitle(window, title);
}

static int run_window(retr01_cart_t *cart, retr01_ppu_t *ppu, uint8_t *rgba, int *world, int *col,
                      int *row, int scale)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *tex = NULL;
    retr01_map_cell_t cells[RETR01_MAX_SCREENS_PER_WORLD];
    int cell_count = 0;
    int running = 1;

    if (scale < 1) {
        scale = 1;
    }
    if (scale > 6) {
        scale = 6;
    }

    load_cells(cart, *world, cells, &cell_count);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    window = SDL_CreateWindow("retr01_emu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              RETR01_FB_W * scale, RETR01_FB_H * scale, SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, RETR01_FB_W, RETR01_FB_H);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                            RETR01_FB_W, RETR01_FB_H);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);

    set_title(window, *world, *col, *row, index_of_cell(cells, cell_count, *col, *row), cell_count);
    SDL_UpdateTexture(tex, NULL, rgba, RETR01_FB_W * 4);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            int nworld = *world;
            int ncol = *col;
            int nrow = *row;
            int cycle = 0;

            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                int idx = index_of_cell(cells, cell_count, *col, *row);
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:
                    running = 0;
                    break;
                case SDLK_LEFT:
                    ncol--;
                    break;
                case SDLK_RIGHT:
                    ncol++;
                    break;
                case SDLK_UP:
                    nrow--;
                    break;
                case SDLK_DOWN:
                    nrow++;
                    break;
                case SDLK_n:
                case SDLK_TAB:
                    cycle = 1;
                    if (cell_count > 0) {
                        idx = (idx < 0) ? 0 : (idx + 1) % cell_count;
                        ncol = cells[idx].col;
                        nrow = cells[idx].row;
                    }
                    break;
                case SDLK_p:
                    cycle = 1;
                    if (cell_count > 0) {
                        idx = (idx < 0) ? 0 : (idx - 1 + cell_count) % cell_count;
                        ncol = cells[idx].col;
                        nrow = cells[idx].row;
                    }
                    break;
                case SDLK_LEFTBRACKET:
                    nworld--;
                    break;
                case SDLK_RIGHTBRACKET:
                    nworld++;
                    break;
                default:
                    break;
                }
            }

            if (nworld < 0) {
                nworld = 0;
            }
            if (nworld >= RETR01_MAX_WORLDS) {
                nworld = RETR01_MAX_WORLDS - 1;
            }
            if (ncol < 0) {
                ncol = 0;
            }
            if (nrow < 0) {
                nrow = 0;
            }

            if (nworld != *world) {
                retr01_map_cell_t next_cells[RETR01_MAX_SCREENS_PER_WORLD];
                int next_count = 0;
                if (load_cells(cart, nworld, next_cells, &next_count) == 0 && next_count > 0) {
                    memcpy(cells, next_cells, sizeof(cells));
                    cell_count = next_count;
                    *world = nworld;
                    *col = cells[0].col;
                    *row = cells[0].row;
                    show_screen(cart, ppu, *world, *col, *row, rgba);
                    SDL_UpdateTexture(tex, NULL, rgba, RETR01_FB_W * 4);
                    set_title(window, *world, *col, *row, 0, cell_count);
                }
            } else if ((ncol != *col || nrow != *row || cycle) &&
                       show_screen(cart, ppu, nworld, ncol, nrow, rgba) == 0) {
                *col = ncol;
                *row = nrow;
                SDL_UpdateTexture(tex, NULL, rgba, RETR01_FB_W * 4);
                set_title(window, *world, *col, *row, index_of_cell(cells, cell_count, *col, *row),
                          cell_count);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

static int run_cpu_window(retr01_emu_t *e, uint8_t *rgba, int *world, int *col, int *row, int scale)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *tex = NULL;
    retr01_map_cell_t cells[RETR01_MAX_SCREENS_PER_WORLD];
    int cell_count = 0;
    int running = 1;
    int has_map = 0;

    if (scale < 1) {
        scale = 1;
    }
    if (scale > 6) {
        scale = 6;
    }

    has_map = load_cells(&e->cart, *world, cells, &cell_count) == 0 && cell_count > 0;
    if (has_map) {
        show_screen(&e->cart, &e->ppu, *world, *col, *row, rgba);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    window = SDL_CreateWindow("retr01_emu (cpu)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              RETR01_FB_W * scale, RETR01_FB_H * scale, SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, RETR01_FB_W, RETR01_FB_H);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                            RETR01_FB_W, RETR01_FB_H);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);

    printf("CPU: arrows/Z/X/Enter -> $FE60  N/P [/] still browse MAP  Esc quit\n");

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                int nworld = *world;
                int ncol = *col;
                int nrow = *row;
                int cycle = 0;
                int idx = index_of_cell(cells, cell_count, *col, *row);
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:
                    running = 0;
                    break;
                case SDLK_n:
                case SDLK_TAB:
                    cycle = 1;
                    if (cell_count > 0) {
                        idx = (idx < 0) ? 0 : (idx + 1) % cell_count;
                        ncol = cells[idx].col;
                        nrow = cells[idx].row;
                    }
                    break;
                case SDLK_p:
                    cycle = 1;
                    if (cell_count > 0) {
                        idx = (idx < 0) ? 0 : (idx - 1 + cell_count) % cell_count;
                        ncol = cells[idx].col;
                        nrow = cells[idx].row;
                    }
                    break;
                case SDLK_LEFTBRACKET:
                    nworld--;
                    break;
                case SDLK_RIGHTBRACKET:
                    nworld++;
                    break;
                default:
                    break;
                }
                if (nworld < 0) {
                    nworld = 0;
                }
                if (nworld >= RETR01_MAX_WORLDS) {
                    nworld = RETR01_MAX_WORLDS - 1;
                }
                if (nworld != *world) {
                    retr01_map_cell_t next_cells[RETR01_MAX_SCREENS_PER_WORLD];
                    int next_count = 0;
                    if (load_cells(&e->cart, nworld, next_cells, &next_count) == 0 &&
                        next_count > 0) {
                        memcpy(cells, next_cells, sizeof(cells));
                        cell_count = next_count;
                        *world = nworld;
                        *col = cells[0].col;
                        *row = cells[0].row;
                        e->vram_written = 0;
                    }
                } else if (cycle && show_screen(&e->cart, &e->ppu, nworld, ncol, nrow, rgba) == 0) {
                    *col = ncol;
                    *row = nrow;
                    e->vram_written = 0;
                }
            }
        }

        retr01_emu_set_pad(e, 0, pad_from_keyboard());
        retr01_emu_run_frame(e);
        if (!e->vram_written && has_map) {
            show_screen(&e->cart, &e->ppu, *world, *col, *row, rgba);
        } else {
            retr01_emu_sync_ppu(e);
            retr01_ppu_render_bg(&e->ppu, rgba);
        }
        SDL_UpdateTexture(tex, NULL, rgba, RETR01_FB_W * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int main(int argc, char **argv)
{
    const char *cart_path = NULL;
    const char *dump_path = NULL;
    int world = 0;
    int col = 0;
    int row = 0;
    int scale = 2;
    int headless = 0;
    int frames = -1;
    int force_cpu = 0;
    int force_view = 0;
    int use_cpu = 0;
    retr01_emu_t emu;
    uint8_t rgba[RETR01_FB_W * RETR01_FB_H * 4];
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cart") == 0 && i + 1 < argc) {
            cart_path = argv[++i];
        } else if (strcmp(argv[i], "--world") == 0 && i + 1 < argc) {
            world = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--col") == 0 && i + 1 < argc) {
            col = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--row") == 0 && i + 1 < argc) {
            row = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            scale = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dump-fb") == 0 && i + 1 < argc) {
            dump_path = argv[++i];
            headless = 1;
        } else if (strcmp(argv[i], "--headless") == 0) {
            headless = 1;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
            headless = 1;
        } else if (strcmp(argv[i], "--cpu") == 0) {
            force_cpu = 1;
        } else if (strcmp(argv[i], "--view") == 0) {
            force_view = 1;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (!cart_path) {
        usage(argv[0]);
        return 1;
    }

    retr01_emu_init(&emu);
    if (retr01_emu_load_cart(&emu, cart_path) != 0) {
        fprintf(stderr, "failed to load cart: %s\n", cart_path);
        retr01_emu_free(&emu);
        return 1;
    }

    load_palette(&emu.ppu.palette);
    emu.ppu.chr = emu.cart.chr;
    emu.ppu.chr_size = emu.cart.chr_size;

    use_cpu = force_cpu || (!force_view && retr01_emu_has_prg(&emu));
    if (force_cpu && !retr01_emu_has_prg(&emu)) {
        fprintf(stderr, "warning: PRG is too small for CPU mode (%zu bytes); need 32 KB\n",
                emu.cart.prg_size);
    }

    print_map_index(&emu.cart);

    if (use_cpu) {
        retr01_emu_reset(&emu);
        show_screen(&emu.cart, &emu.ppu, world, col, row, rgba);

        if (frames >= 0) {
            if (retr01_emu_run_frames(&emu, frames) != 0) {
                retr01_emu_free(&emu);
                return 1;
            }
            printf("cpu frames=%d pc=$%04X a=$%02X ram0200=$%02X ram0201=$%02X\n", frames,
                   emu.cpu.pc, emu.cpu.a, emu.ram[0x0200], emu.ram[0x0201]);
            retr01_emu_free(&emu);
            return 0;
        }

        if (headless) {
            retr01_emu_sync_ppu(&emu);
            retr01_ppu_render_bg(&emu.ppu, rgba);
            if (dump_path) {
                FILE *f = fopen(dump_path, "wb");
                if (!f || fwrite(rgba, 1, sizeof(rgba), f) != sizeof(rgba)) {
                    fprintf(stderr, "failed to write %s\n", dump_path);
                    retr01_emu_free(&emu);
                    return 1;
                }
                fclose(f);
                printf("wrote %s (%zu bytes)\n", dump_path, sizeof(rgba));
            }
            printf("cpu pc=$%04X\n", emu.cpu.pc);
            retr01_emu_free(&emu);
            return 0;
        }

        i = run_cpu_window(&emu, rgba, &world, &col, &row, scale);
        retr01_emu_free(&emu);
        return i;
    }

    if (show_screen(&emu.cart, &emu.ppu, world, col, row, rgba) != 0) {
        fprintf(stderr, "load_screen failed world=%d col=%d row=%d\n", world, col, row);
        retr01_emu_free(&emu);
        return 1;
    }

    if (dump_path) {
        FILE *f = fopen(dump_path, "wb");
        if (!f || fwrite(rgba, 1, sizeof(rgba), f) != sizeof(rgba)) {
            fprintf(stderr, "failed to write %s\n", dump_path);
            retr01_emu_free(&emu);
            return 1;
        }
        fclose(f);
        printf("wrote %s (%zu bytes)\n", dump_path, sizeof(rgba));
    }

    if (!headless) {
        i = run_window(&emu.cart, &emu.ppu, rgba, &world, &col, &row, scale);
        retr01_emu_free(&emu);
        return i;
    }

    printf("rendered %dx%d\n", RETR01_FB_W, RETR01_FB_H);
    retr01_emu_free(&emu);
    return 0;
}
