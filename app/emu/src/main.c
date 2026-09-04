#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/video.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef R01_DEFAULT_CART
#define R01E_DEFAULT_CART R01_DEFAULT_CART
#endif

#ifndef R01E_DEFAULT_CART
#define R01E_DEFAULT_CART "../../output/test.retr01"
#endif

/* Debug pane: VRAM atlas + world map + active BG/SPR palette rows + CPU budget. */
#define DBG_GAP 8
#define DBG_MAP_CELL 10
#define DBG_MAP_MAX_CELLS 16
#define DBG_MAP_W (DBG_MAP_CELL * DBG_MAP_MAX_CELLS)
#define DBG_MAP_H (DBG_MAP_CELL * DBG_MAP_MAX_CELLS)
#define DBG_PAL_SWATCH 8
#define DBG_PAL_GAP 2
#define DBG_PAL_GROUP_GAP 4
#define DBG_PAL_LABEL_W 28
#define DBG_PAL_H (DBG_PAL_SWATCH * 2 + DBG_PAL_GAP + 10)
#define DBG_CHART_BARS 20
#define DBG_CHART_HZ 2
#define DBG_CHART_H 72
#define DBG_CHART_PAD 6
#define DBG_CHART_LABEL_H 12
#define DBG_ATLAS_W R01E_VRAM_ATLAS_W
#define DBG_ATLAS_H R01E_VRAM_ATLAS_H
#define DBG_MASK_W R01E_SCREEN_PX_W
#define DBG_MASK_H R01E_SCREEN_PX_H
#define DBG_TOP_ROW_H DBG_ATLAS_H
#define DBG_WIN_W (DBG_ATLAS_W + DBG_GAP + DBG_ATLAS_W + DBG_GAP + DBG_MAP_W)
#define DBG_ROW2_Y (DBG_TOP_ROW_H + DBG_GAP)
#define DBG_WIN_H (DBG_ROW2_Y + DBG_MASK_H + DBG_GAP + DBG_PAL_H + DBG_GAP + DBG_CHART_H + DBG_CHART_LABEL_H)

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
        m->play.pad_prev = m->io.pad0;
        m->io.pad0 = (uint8_t)(m->io.pad0 & (uint8_t)~R01E_PAD_X);
    } else if (sym == SDLK_y) {
        m->io.pad0 = (uint8_t)(m->io.pad0 | R01E_PAD_Y);
        r01e_play_tick(m);
        r01e_video_render_frame(m);
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
            int col = R01E_CELL_COL(e[0]);
            int row = R01E_CELL_ROW(e[0]);
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
        cur_c = (m->play.player_x + R01E_PLAY_PLAYER_W / 2) / R01E_SCREEN_PX_W;
        cur_r = (m->play.player_y + R01E_PLAY_PLAYER_H / 2) / R01E_SCREEN_PX_H;
    } else {
        cur_c = m->video.cam_origin_col;
        cur_r = m->video.cam_origin_row;
    }

    /* Center the used bounding box in the map pane. */
    map_ox = ox + (DBG_MAP_W - DBG_MAP_CELL * cols) / 2;
    map_oy = oy + (DBG_ATLAS_H - DBG_MAP_CELL * rows) / 2;

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

/* 5x7 uppercase glyphs (MSB = leftmost). */
static const uint8_t DBG_GLYPH_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
static const uint8_t DBG_GLYPH_G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
static const uint8_t DBG_GLYPH_S[7] = {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E};
static const uint8_t DBG_GLYPH_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t DBG_GLYPH_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};

static void dbg_blit_glyph(SDL_Renderer *ren, int x, int y, const uint8_t rows[7], Uint8 R, Uint8 G,
                           Uint8 B) {
    int row, col;
    SDL_SetRenderDrawColor(ren, R, G, B, 255);
    for (row = 0; row < 7; row++) {
        for (col = 0; col < 5; col++) {
            if (rows[row] & (1u << (4 - col))) {
                SDL_RenderDrawPoint(ren, x + col, y + row);
            }
        }
    }
}

static void draw_pal_strip(SDL_Renderer *ren, int x, int y, const uint8_t *row16) {
    int pal, c;
    int cx = x;
    SDL_Rect cell;

    for (pal = 0; pal < R01E_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01E_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            uint8_t master = row16 ? (row16[pal * R01E_PAL_COLORS + c] & 63u) : 0;
            r01e_video_kit_rgb(master, &cr, &cg, &cb);
            cell.x = cx;
            cell.y = y;
            cell.w = DBG_PAL_SWATCH;
            cell.h = DBG_PAL_SWATCH;
            SDL_SetRenderDrawColor(ren, cr, cg, cb, 255);
            SDL_RenderFillRect(ren, &cell);
            SDL_SetRenderDrawColor(ren, 50, 55, 60, 255);
            SDL_RenderDrawRect(ren, &cell);
            cx += DBG_PAL_SWATCH + 1;
        }
        cx += DBG_PAL_GROUP_GAP - 1;
    }
}

static void draw_active_palettes(SDL_Renderer *ren, R01eMachine *m, int ox, int oy) {
    int y2 = oy + DBG_PAL_SWATCH + DBG_PAL_GAP;

    dbg_blit_glyph(ren, ox, oy, DBG_GLYPH_B, 160, 180, 160);
    dbg_blit_glyph(ren, ox + 6, oy, DBG_GLYPH_G, 160, 180, 160);
    draw_pal_strip(ren, ox + DBG_PAL_LABEL_W, oy, m->io.pal);

    dbg_blit_glyph(ren, ox, y2, DBG_GLYPH_S, 180, 160, 160);
    dbg_blit_glyph(ren, ox + 6, y2, DBG_GLYPH_P, 180, 160, 160);
    dbg_blit_glyph(ren, ox + 12, y2, DBG_GLYPH_R, 180, 160, 160);
    draw_pal_strip(ren, ox + DBG_PAL_LABEL_W, y2, m->io.pal + R01E_PAL_ROW_BYTES);
}

typedef struct DbgCpuChart {
    uint64_t active[DBG_CHART_BARS];
    uint64_t vblank[DBG_CHART_BARS];
    int count;
    int head; /* next write index */
    uint64_t peak_active;
    uint64_t peak_vblank;
    Uint32 last_sample_ms;
} DbgCpuChart;

static void dbg_chart_note_frame(DbgCpuChart *ch, const R01eMachine *m) {
    if (!ch || !m) {
        return;
    }
    if (m->prof_last_active > ch->peak_active) {
        ch->peak_active = m->prof_last_active;
    }
    if (m->prof_last_vblank > ch->peak_vblank) {
        ch->peak_vblank = m->prof_last_vblank;
    }
}

static void dbg_chart_maybe_sample(DbgCpuChart *ch, Uint32 now_ms) {
    int i;
    if (!ch) {
        return;
    }
    if (ch->last_sample_ms == 0) {
        ch->last_sample_ms = now_ms;
        return;
    }
    if ((int)(now_ms - ch->last_sample_ms) < (1000 / DBG_CHART_HZ)) {
        return;
    }
    i = ch->head;
    ch->active[i] = ch->peak_active;
    ch->vblank[i] = ch->peak_vblank;
    ch->head = (ch->head + 1) % DBG_CHART_BARS;
    if (ch->count < DBG_CHART_BARS) {
        ch->count++;
    }
    ch->peak_active = 0;
    ch->peak_vblank = 0;
    ch->last_sample_ms = now_ms;
}

static void draw_cpu_budget_chart(SDL_Renderer *ren, const DbgCpuChart *ch, int ox, int oy) {
    SDL_Rect frame;
    SDL_Rect bar;
    int plot_w = DBG_WIN_W - 2 * DBG_CHART_PAD;
    int plot_h = DBG_CHART_H - DBG_CHART_PAD;
    int slot_w;
    int budget_y;
    int n, slot;
    uint64_t budget = R01E_CPU_BUDGET_CYCLES;

    frame.x = ox;
    frame.y = oy;
    frame.w = DBG_WIN_W;
    frame.h = DBG_CHART_H + DBG_CHART_LABEL_H;
    SDL_SetRenderDrawColor(ren, 18, 20, 26, 255);
    SDL_RenderFillRect(ren, &frame);
    SDL_SetRenderDrawColor(ren, 40, 44, 52, 255);
    SDL_RenderDrawRect(ren, &frame);

    /* Legend swatches */
    bar.x = ox + DBG_CHART_PAD;
    bar.y = oy + 2;
    bar.w = 8;
    bar.h = 6;
    SDL_SetRenderDrawColor(ren, 70, 190, 200, 255);
    SDL_RenderFillRect(ren, &bar);
    bar.x += 12;
    SDL_SetRenderDrawColor(ren, 220, 140, 50, 255);
    SDL_RenderFillRect(ren, &bar);

    slot_w = plot_w / DBG_CHART_BARS;
    if (slot_w < 2) {
        slot_w = 2;
    }
    /* 100% budget line at top of plot (full frame CPU allotment). */
    budget_y = oy + DBG_CHART_LABEL_H;
    SDL_SetRenderDrawColor(ren, 120, 55, 55, 255);
    SDL_RenderDrawLine(ren, ox + DBG_CHART_PAD, budget_y, ox + DBG_CHART_PAD + plot_w - 1, budget_y);

    n = ch ? ch->count : 0;
    for (slot = 0; slot < n; slot++) {
        /* Oldest on the left */
        int idx = (ch->head - n + slot + DBG_CHART_BARS * 2) % DBG_CHART_BARS;
        uint64_t act = ch->active[idx];
        uint64_t vbl = ch->vblank[idx];
        uint64_t total = act + vbl;
        int h_tot, h_act, h_vbl;
        int bx = ox + DBG_CHART_PAD + slot * slot_w + 1;
        int bw = slot_w - 2;
        int base_y = oy + DBG_CHART_LABEL_H + plot_h;

        if (bw < 1) {
            bw = 1;
        }
        if (total > budget) {
            /* Scale so overflow still fits (clip at 150% visual). */
            uint64_t scale = (budget * 3u) / 2u;
            if (scale < total) {
                act = act * scale / total;
                vbl = vbl * scale / total;
                total = act + vbl;
            }
        }
        h_tot = (int)((total * (uint64_t)plot_h) / (budget ? budget : 1));
        h_act = (int)((act * (uint64_t)plot_h) / (budget ? budget : 1));
        h_vbl = (int)((vbl * (uint64_t)plot_h) / (budget ? budget : 1));
        if (h_tot > plot_h) {
            /* Preserve active:vblank ratio when clipping to plot. */
            if (total > 0) {
                h_act = (int)((act * (uint64_t)plot_h) / total);
                h_vbl = plot_h - h_act;
            } else {
                h_act = 0;
                h_vbl = 0;
            }
            h_tot = plot_h;
        } else {
            h_vbl = h_tot - h_act;
        }
        if (total > 0 && h_tot < 1) {
            h_tot = 1;
            h_act = (act >= vbl) ? 1 : 0;
            h_vbl = h_tot - h_act;
        }

        /* Stacked: active (bottom / cyan), vblank (top / orange) */
        if (h_act > 0) {
            bar.x = bx;
            bar.y = base_y - h_act;
            bar.w = bw;
            bar.h = h_act;
            SDL_SetRenderDrawColor(ren, 70, 190, 200, 255);
            SDL_RenderFillRect(ren, &bar);
        }
        if (h_vbl > 0) {
            bar.x = bx;
            bar.y = base_y - h_act - h_vbl;
            bar.w = bw;
            bar.h = h_vbl;
            SDL_SetRenderDrawColor(ren, 220, 140, 50, 255);
            SDL_RenderFillRect(ren, &bar);
        }
    }
}

static void present_debug_pane(SDL_Renderer *dbg_ren, SDL_Texture *vram_tex, SDL_Texture *bg0_tex,
                              SDL_Texture *mask_tex, R01eMachine *m, const DbgCpuChart *chart) {
    SDL_Rect dst;
    SDL_Rect vp;
    int chart_y;
    int bg0_x = DBG_ATLAS_W + DBG_GAP;
    int map_x = DBG_ATLAS_W + DBG_GAP + DBG_ATLAS_W + DBG_GAP;
    int l0_sx;
    int l0_sy;

    r01e_video_render_vram_atlas(m);
    r01e_video_render_bg0_atlas(m);
    r01e_video_render_l1_mask(m);
    SDL_UpdateTexture(vram_tex, NULL, m->video.vram_atlas, DBG_ATLAS_W * 3);
    SDL_UpdateTexture(bg0_tex, NULL, m->video.bg0_atlas, DBG_ATLAS_W * 3);
    SDL_UpdateTexture(mask_tex, NULL, m->video.l1_mask, DBG_MASK_W * 3);

    SDL_SetRenderDrawColor(dbg_ren, 12, 14, 18, 255);
    SDL_RenderClear(dbg_ren);

    dst.x = 0;
    dst.y = 0;
    dst.w = DBG_ATLAS_W;
    dst.h = DBG_ATLAS_H;
    SDL_RenderCopy(dbg_ren, vram_tex, NULL, &dst);

    dst.x = bg0_x;
    dst.y = 0;
    dst.w = DBG_ATLAS_W;
    dst.h = DBG_ATLAS_H;
    SDL_RenderCopy(dbg_ren, bg0_tex, NULL, &dst);

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

    l0_sx = m->video.l0_cam_x % R01E_SCREEN_PX_W;
    l0_sy = m->video.l0_cam_y % R01E_SCREEN_PX_H;
    vp.x = bg0_x + l0_sx;
    vp.y = l0_sy;
    vp.w = R01E_SCREEN_PX_W;
    vp.h = R01E_SCREEN_PX_H;
    SDL_SetRenderDrawColor(dbg_ren, 80, 220, 120, 255);
    SDL_RenderDrawRect(dbg_ren, &vp);
    vp.x += 1;
    vp.y += 1;
    vp.w -= 2;
    vp.h -= 2;
    if (vp.w > 0 && vp.h > 0) {
        SDL_RenderDrawRect(dbg_ren, &vp);
    }

    draw_world_map(dbg_ren, m, map_x, 0);

    dst.x = 0;
    dst.y = DBG_ROW2_Y;
    dst.w = DBG_MASK_W;
    dst.h = DBG_MASK_H;
    SDL_RenderCopy(dbg_ren, mask_tex, NULL, &dst);

    draw_active_palettes(dbg_ren, m, 4, DBG_ROW2_Y + DBG_MASK_H + DBG_GAP);
    chart_y = DBG_ROW2_Y + DBG_MASK_H + DBG_GAP + DBG_PAL_H + DBG_GAP;
    draw_cpu_budget_chart(dbg_ren, chart, 0, chart_y);
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
    SDL_Texture *bg0_tex = NULL;
    SDL_Texture *mask_tex = NULL;
    int scale = 3;
    int running = 1;
    int paused = 0;
    Uint32 last_ticks;
    int main_x = 0, main_y = 0, main_w = 0, main_h = 0;
    DbgCpuChart cpu_chart;

    memset(&cpu_chart, 0, sizeof(cpu_chart));

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

    /* Hidden until first frame is presented -- avoids empty-window flash. */
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

    /* One debug window: VRAM 2x2 + world map + pals + CPU budget chart. */
    SDL_GetWindowPosition(win, &main_x, &main_y);
    SDL_GetWindowSize(win, &main_w, &main_h);
    dbg_win = SDL_CreateWindow("Debug", main_x + main_w + 16, main_y, DBG_WIN_W, DBG_WIN_H,
                               SDL_WINDOW_HIDDEN);
    dbg_ren = dbg_win ? SDL_CreateRenderer(dbg_win, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (dbg_ren) {
        SDL_RenderSetLogicalSize(dbg_ren, DBG_WIN_W, DBG_WIN_H);
        vram_tex = SDL_CreateTexture(dbg_ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, DBG_ATLAS_W,
                                     DBG_ATLAS_H);
        bg0_tex = SDL_CreateTexture(dbg_ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, DBG_ATLAS_W,
                                    DBG_ATLAS_H);
        mask_tex = SDL_CreateTexture(dbg_ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, DBG_MASK_W,
                                     DBG_MASK_H);
    }
    if (!dbg_win || !dbg_ren || !vram_tex || !bg0_tex || !mask_tex) {
        fprintf(stderr, "retr01_emu: debug window unavailable (%s) -- continuing without it\n",
                SDL_GetError());
        if (mask_tex) {
            SDL_DestroyTexture(mask_tex);
            mask_tex = NULL;
        }
        if (bg0_tex) {
            SDL_DestroyTexture(bg0_tex);
            bg0_tex = NULL;
        }
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
    printf("Studio Play SoT: WASD/arrows move  |  X/Y warp  |  Space pause  |  R reset  |  Esc quit\n");
    if (dbg_win) {
        printf("Debug: BG1/BG0 2x2 + BG1 mask + world map + pals + CPU budget (2 Hz, 50k red line)\n");
    }

    /* Present boot frame while still hidden, then show. */
    SDL_UpdateTexture(tex, NULL, machine.video.fb, R01E_VISIBLE_W * 3);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    if (dbg_win && dbg_ren && vram_tex && bg0_tex && mask_tex) {
        present_debug_pane(dbg_ren, vram_tex, bg0_tex, mask_tex, &machine, &cpu_chart);
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
                    SDL_DestroyTexture(mask_tex);
                    SDL_DestroyTexture(bg0_tex);
                    SDL_DestroyTexture(vram_tex);
                    SDL_DestroyRenderer(dbg_ren);
                    SDL_DestroyWindow(dbg_win);
                    vram_tex = NULL;
                    bg0_tex = NULL;
                    mask_tex = NULL;
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

        {
            Uint32 now = SDL_GetTicks();
            if (!paused) {
                if ((int)(now - last_ticks) >= 16) {
                    (void)r01e_machine_frame(&machine);
                    dbg_chart_note_frame(&cpu_chart, &machine);
                    last_ticks = now;
                }
                dbg_chart_maybe_sample(&cpu_chart, now);
            } else {
                r01e_video_render_frame(&machine);
            }
        }

        SDL_UpdateTexture(tex, NULL, machine.video.fb, R01E_VISIBLE_W * 3);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        if (dbg_win && dbg_ren && vram_tex && bg0_tex && mask_tex) {
            present_debug_pane(dbg_ren, vram_tex, bg0_tex, mask_tex, &machine, &cpu_chart);
        }
    }

    if (mask_tex) {
        SDL_DestroyTexture(mask_tex);
    }
    if (bg0_tex) {
        SDL_DestroyTexture(bg0_tex);
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
