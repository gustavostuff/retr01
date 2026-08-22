#include "ui.h"
#include "font.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const SDL_Color GRAY[4] = {
    {20, 20, 24, 255},
    {80, 80, 88, 255},
    {160, 160, 168, 255},
    {230, 230, 236, 255},
};

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int left_scroll_max(void) {
    int max = UI_LEFT_CONTENT_H - UI_LOGIC_H;
    return max > 0 ? max : 0;
}

static void clamp_left_scroll(UiState *ui) {
    ui->left_scroll_y = clamp_i(ui->left_scroll_y, 0, left_scroll_max());
}

static int left_cy(const UiState *ui, int view_y) {
    return view_y + ui->left_scroll_y;
}

static int in_left_viewport(int lx, int ly) {
    return lx >= 0 && lx < UI_LEFT_W && ly >= 0 && ly < UI_LOGIC_H;
}

int ui_init(UiState *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->project = (R01Project *)calloc(1, sizeof(R01Project));
    if (!ui->project) {
        return -1;
    }
    r01_project_init(ui->project, "untitled");
    ui->scale = 2;
    ui->world_tab = 1;
    ui->bg_bank_tab = 0;
    ui->screen_zoom = 2;
    ui->left_scroll_y = 0;
    ui->edit_mode = UI_MODE_PIXEL;
    ui->attr_tx = 0;
    ui->attr_ty = 0;
    ui->pal_row_tab = 0;
    ui->pal_slot = 0;
    ui->show_grid = 1;
    snprintf(ui->status, sizeof(ui->status),
             "Tab=pix/attr | G=grid | Ctrl+G=gen | wheel=scroll left | S/O=save/load");
    strncpy(ui->project_path, "project.json", R01_PATH_MAX - 1);
    return 0;
}

void ui_shutdown(UiState *ui) {
    free(ui->project);
    ui->project = NULL;
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

static R01World *cur_world(UiState *ui) {
    int hw = r01_ui_world_to_hw(ui->world_tab);
    R01World *w = r01_project_world(ui->project, hw);
    if (w) {
        w->present = 1;
        ui->project->active_world = hw;
    }
    return w;
}

static R01PalRow *cur_pal_row(UiState *ui) {
    R01World *w = cur_world(ui);
    int tab = ui->pal_row_tab;
    int is_spr = tab >= 4;
    int row = tab & 3;
    if (w && w->use_world_pals) {
        return is_spr ? &w->pal_spr[row] : &w->pal_bg[row];
    }
    return is_spr ? &ui->project->global_pal_spr[row] : &ui->project->global_pal_bg[row];
}

static void draw_worlds(UiState *ui, SDL_Renderer *r) {
    R01World *w = cur_world(ui);
    int t, c, row;
    int oy = -ui->left_scroll_y;
    fill_rect(r, 0, UI_WORLDS_Y + oy, UI_LEFT_W, UI_WORLDS_H, 28, 32, 40);
    font_draw(r, 4, 4 + oy, "WORLDS", 200, 200, 210);
    for (t = 1; t <= 8; t++) {
        int x = 4 + (t - 1) * 24;
        int selected = (t == ui->world_tab);
        fill_rect(r, x, 16 + oy, 22, 12, selected ? 70 : 40, selected ? 90 : 48, selected ? 120 : 60);
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", t);
            font_draw(r, x + 8, 18 + oy, buf, 230, 230, 240);
        }
    }
    font_draw(r, 4, 28 + oy, "CTRL+CLICK TOGGLE", 120, 120, 130);
    for (row = 0; row < R01_GRID_SIZE; row++) {
        for (c = 0; c < R01_GRID_SIZE; c++) {
            int x = UI_WORLD_GRID_X + c * UI_WORLD_CELL;
            int y = UI_WORLD_GRID_Y + row * UI_WORLD_CELL + oy;
            int idx = w ? r01_world_find_screen(w, c, row) : -1;
            int active = (ui->project->active_screen >= 0 && idx == ui->project->active_screen);
            int cs = UI_WORLD_CELL - 1;
            fill_rect(r, x, y, cs, cs, idx >= 0 ? (active ? 180 : 100) : 45,
                      idx >= 0 ? (active ? 140 : 110) : 50, idx >= 0 ? (active ? 60 : 90) : 58);
        }
    }
}

static void draw_planes(UiState *ui, SDL_Renderer *r) {
    R01World *w = cur_world(ui);
    int p;
    int oy = -ui->left_scroll_y;
    fill_rect(r, 0, UI_PLANES_Y + oy, UI_LEFT_W, UI_PLANES_H, 26, 30, 36);
    font_draw(r, 4, UI_PLANES_Y + 4 + oy, "PLANES", 200, 200, 210);
    font_draw(r, 70, UI_PLANES_Y + 4 + oy, "CTRL+CLICK", 110, 110, 120);
    for (p = 0; p < R01_MAX_PARALLAX_PLANES; p++) {
        int x = 4 + p * 48;
        int y = UI_PLANES_Y + 18 + oy;
        int present = w && w->planes[p].present;
        int sel = (ui->project->active_plane == p);
        char buf[8];
        fill_rect(r, x, y, 44, 16, present ? (sel ? 70 : 50) : 36, present ? (sel ? 110 : 70) : 40,
                  present ? (sel ? 140 : 100) : 48);
        snprintf(buf, sizeof(buf), "P%d", p);
        font_draw(r, x + 14, y + 4, buf, 230, 230, 240);
    }
}

static void draw_chr_tile(SDL_Renderer *r, int x, int y, const uint8_t tile16[R01_TILE_BYTES]) {
    uint8_t pix[64];
    int py, px;
    r01_tile_to_pixels(tile16, pix);
    for (py = 0; py < 8; py++) {
        for (px = 0; px < 8; px++) {
            SDL_Color c = GRAY[pix[py * 8 + px] & 3];
            fill_rect(r, x + px, y + py, 1, 1, c.r, c.g, c.b);
        }
    }
}

static void draw_bg_banks(UiState *ui, SDL_Renderer *r) {
    R01World *w = cur_world(ui);
    int b, ty, tx;
    int oy = -ui->left_scroll_y;
    const int sheet_x = 4;
    const int sheet_y = UI_BG_Y + 22 + oy;
    fill_rect(r, 0, UI_BG_Y + oy, UI_LEFT_W, UI_BG_H, 24, 28, 34);
    font_draw(r, 4, UI_BG_Y + 2 + oy, "BG BANKS", 200, 200, 210);
    for (b = 0; b < 4; b++) {
        int x = 68 + b * 22;
        int sel = (b == ui->bg_bank_tab);
        fill_rect(r, x, UI_BG_Y + 2 + oy, 20, 12, sel ? 70 : 40, sel ? 90 : 48, sel ? 120 : 60);
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", b);
            font_draw(r, x + 7, UI_BG_Y + 4 + oy, buf, 230, 230, 240);
        }
    }
    if (w) {
        R01BgBank *bank = &w->bg_banks[ui->bg_bank_tab];
        char buf[24];
        for (ty = 0; ty < 16; ty++) {
            for (tx = 0; tx < 16; tx++) {
                int ti = ty * 16 + tx;
                int x = sheet_x + tx * 8;
                int y = sheet_y + ty * 8;
                if (ti >= bank->tile_count) {
                    fill_rect(r, x, y, 8, 8, 35, 38, 44);
                    continue;
                }
                draw_chr_tile(r, x, y, &bank->chr[ti * R01_TILE_BYTES]);
            }
        }
        snprintf(buf, sizeof(buf), "%d", bank->tile_count);
        font_draw(r, 168, UI_BG_Y + 4 + oy, buf, 160, 160, 170);
    }
}

static void draw_sprite_stub(UiState *ui, SDL_Renderer *r) {
    int oy = -ui->left_scroll_y;
    fill_rect(r, 0, UI_SPR_Y + oy, UI_LEFT_W, UI_SPR_H, 22, 24, 28);
    font_draw(r, 4, UI_SPR_Y + 4 + oy, "SPRITE BANKS", 90, 90, 100);
    font_draw(r, 4, UI_SPR_Y + 16 + oy, "PHASE 3", 70, 70, 80);
}

static void draw_palettes(UiState *ui, SDL_Renderer *r) {
    int oy = -ui->left_scroll_y;
    int t, s, m;
    R01PalRow *row;
    char buf[32];
    fill_rect(r, 0, UI_PAL_Y + oy, UI_LEFT_W, UI_PAL_H, 20, 22, 26);
    font_draw(r, 4, UI_PAL_Y + 4 + oy, "PALETTES", 200, 200, 210);
    for (t = 0; t < 8; t++) {
        int x = 4 + (t % 4) * 24;
        int y = UI_PAL_Y + 16 + (t / 4) * 14 + oy;
        int sel = (t == ui->pal_row_tab);
        fill_rect(r, x, y, 22, 12, sel ? 70 : 40, sel ? 90 : 48, sel ? 120 : 60);
        snprintf(buf, sizeof(buf), "%s%d", t < 4 ? "B" : "S", t & 3);
        font_draw(r, x + 4, y + 2, buf, 230, 230, 240);
    }
    row = cur_pal_row(ui);
    for (s = 0; s < 4; s++) {
        uint8_t cr, cg, cb;
        int x = 8 + s * 46;
        int y = UI_PAL_Y + 48 + oy;
        r01_kit_rgb(row->idx[s], &cr, &cg, &cb);
        fill_rect(r, x, y, 40, 20, cr, cg, cb);
        if (s == ui->pal_slot) {
            draw_rect(r, x, y, 40, 20, 255, 200, 80);
        }
        snprintf(buf, sizeof(buf), "%02d", row->idx[s]);
        font_draw(r, x + 10, y + 22, buf, 160, 160, 170);
    }
    font_draw(r, 4, UI_PAL_Y + 96 + oy, "MASTER 0-63  - =", 120, 120, 130);
    for (m = 0; m < 64; m++) {
        uint8_t cr, cg, cb;
        int x = 4 + (m % 16) * 12;
        int y = UI_PAL_Y + 110 + (m / 16) * 10 + oy;
        r01_kit_rgb(m, &cr, &cg, &cb);
        fill_rect(r, x, y, 11, 9, cr, cg, cb);
        if (m == row->idx[ui->pal_slot]) {
            draw_rect(r, x, y, 11, 9, 255, 255, 255);
        }
    }
}

static void draw_left_scrollbar(UiState *ui, SDL_Renderer *r) {
    int max = left_scroll_max();
    int track_x = UI_LEFT_W - UI_LEFT_SCROLLBAR_W;
    int thumb_h, thumb_y;
    fill_rect(r, track_x, 0, UI_LEFT_SCROLLBAR_W, UI_LOGIC_H, 16, 18, 22);
    if (max <= 0) {
        return;
    }
    thumb_h = (UI_LOGIC_H * UI_LOGIC_H) / UI_LEFT_CONTENT_H;
    if (thumb_h < 12) {
        thumb_h = 12;
    }
    thumb_y = (ui->left_scroll_y * (UI_LOGIC_H - thumb_h)) / max;
    fill_rect(r, track_x, thumb_y, UI_LEFT_SCROLLBAR_W, thumb_h, 90, 95, 110);
}

static void screen_to_pixel(UiState *ui, int lx, int ly, int *ox, int *oy) {
    int view_x = UI_LEFT_W + 8;
    int view_y = 28;
    int zx = ui->screen_zoom;
    *ox = (lx - view_x) / zx + ui->screen_pan_x;
    *oy = (ly - view_y) / zx + ui->screen_pan_y;
}

static void draw_screen(UiState *ui, SDL_Renderer *r) {
    R01World *w = cur_world(ui);
    R01EditSurface surf;
    int has_surf = r01_project_edit_surface(ui->project, &surf) == 0;
    R01Screen *grid = r01_project_active_screen(ui->project);
    int view_x = UI_LEFT_W + 8;
    int view_y = 28;
    int zx = ui->screen_zoom;
    int x, y;
    char buf[64];

    fill_rect(r, UI_LEFT_W, 0, UI_LOGIC_W - UI_LEFT_W, UI_LOGIC_H, 18, 20, 26);
    font_draw(r, UI_LEFT_W + 4, 4, "SCREEN", 200, 200, 210);

    {
        int mx = UI_LEFT_W + 52;
        fill_rect(r, mx, 2, 40, 12, ui->edit_mode == UI_MODE_PIXEL ? 80 : 40,
                  ui->edit_mode == UI_MODE_PIXEL ? 100 : 48, ui->edit_mode == UI_MODE_PIXEL ? 70 : 55);
        font_draw(r, mx + 4, 4, "PIX", 230, 230, 240);
        fill_rect(r, mx + 42, 2, 40, 12, ui->edit_mode == UI_MODE_ATTR ? 80 : 40,
                  ui->edit_mode == UI_MODE_ATTR ? 100 : 48, ui->edit_mode == UI_MODE_ATTR ? 70 : 55);
        font_draw(r, mx + 46, 4, "ATTR", 230, 230, 240);
    }

    if (ui->edit_mode == UI_MODE_PIXEL) {
        font_draw(r, UI_LEFT_W + 140, 4, "COLOR", 140, 140, 150);
        {
            int c;
            for (c = 0; c < 4; c++) {
                int bx = UI_LEFT_W + 180 + c * 18;
                SDL_Color col = GRAY[c];
                fill_rect(r, bx, 2, 16, 12, col.r, col.g, col.b);
                if (c == ui->project->paint_color) {
                    draw_rect(r, bx, 2, 16, 12, 255, 200, 80);
                }
            }
        }
    } else if (has_surf) {
        uint8_t a = r01_tilemap_get_attr(surf.attrs, ui->attr_tx, ui->attr_ty);
        snprintf(buf, sizeof(buf), "T%d.%d B%d P%d%s%s%s%s", ui->attr_tx, ui->attr_ty, r01_attr_bank(a),
                 r01_attr_pal(a), r01_attr_flip_h(a) ? " H" : "", r01_attr_flip_v(a) ? " V" : "",
                 r01_attr_solid(a) ? " S" : "", r01_attr_anim(a) ? " A" : "");
        font_draw(r, UI_LEFT_W + 140, 4, buf, 180, 200, 160);
    }

    font_draw(r, UI_LEFT_W + 320, 4, "BANK", 140, 140, 150);
    {
        int b;
        for (b = 0; b < 4; b++) {
            int bx = UI_LEFT_W + 350 + b * 16;
            int sel = (b == ui->project->generate_bank);
            fill_rect(r, bx, 2, 14, 12, sel ? 80 : 40, sel ? 100 : 48, sel ? 70 : 55);
            snprintf(buf, sizeof(buf), "%d", b);
            font_draw(r, bx + 4, 4, buf, 230, 230, 240);
        }
    }
    font_draw(r, UI_LEFT_W + 420, 4, "C-G", 140, 200, 140);

    fill_rect(r, view_x, view_y, R01_SCREEN_PX_W * zx, R01_SCREEN_PX_H * zx, 10, 10, 12);
    if (!has_surf) {
        font_draw(r, view_x + 20, view_y + 50, "NO TARGET SELECTED", 120, 120, 130);
        font_draw(r, view_x + 20, view_y + 62, "GRID OR PLANE P0/P1", 100, 100, 110);
    } else {
        for (y = 0; y < R01_SCREEN_PX_H; y++) {
            for (x = 0; x < R01_SCREEN_PX_W; x++) {
                uint8_t cr, cg, cb;
                r01_tilemap_pixel_rgb(ui->project, w, surf.pixels, surf.attrs, x, y, &cr, &cg, &cb);
                fill_rect(r, view_x + x * zx, view_y + y * zx, zx, zx, cr, cg, cb);
            }
        }
        if (ui->show_grid) {
            int gx, gy;
            SDL_SetRenderDrawColor(r, 55, 58, 68, 255);
            for (gx = 0; gx <= R01_SCREEN_TILES_X; gx++) {
                int lx = view_x + gx * 8 * zx;
                SDL_RenderDrawLine(r, lx, view_y, lx, view_y + R01_SCREEN_PX_H * zx);
            }
            for (gy = 0; gy <= R01_SCREEN_TILES_Y; gy++) {
                int ly = view_y + gy * 8 * zx;
                SDL_RenderDrawLine(r, view_x, ly, view_x + R01_SCREEN_PX_W * zx, ly);
            }
        }
        if (ui->edit_mode == UI_MODE_ATTR) {
            draw_rect(r, view_x + ui->attr_tx * 8 * zx, view_y + ui->attr_ty * 8 * zx, 8 * zx, 8 * zx, 255, 220,
                      80);
        }
        if (surf.is_plane) {
            snprintf(buf, sizeof(buf), "PLANE P%d  VRAM %d", surf.index, 4 + surf.index);
        } else if (grid) {
            snprintf(buf, sizeof(buf), "CELL %d.%d", grid->col, grid->row);
        } else {
            snprintf(buf, sizeof(buf), "SCREEN");
        }
        font_draw(r, view_x, view_y + R01_SCREEN_PX_H * zx + 4, buf, 160, 160, 170);
        if (ui->edit_mode == UI_MODE_ATTR) {
            font_draw(r, view_x, view_y + R01_SCREEN_PX_H * zx + 14, "B P H V O N ATTR", 110, 110, 120);
        }
    }

    font_draw(r, UI_LEFT_W + 4, UI_LOGIC_H - 12, ui->status, 130, 130, 140);
}

void ui_draw(UiState *ui, SDL_Renderer *r) {
    SDL_Rect clip = {0, 0, UI_LEFT_W, UI_LOGIC_H};
    clamp_left_scroll(ui);
    SDL_SetRenderDrawColor(r, 12, 14, 18, 255);
    SDL_RenderClear(r);

    SDL_RenderSetClipRect(r, &clip);
    draw_worlds(ui, r);
    draw_planes(ui, r);
    draw_bg_banks(ui, r);
    draw_sprite_stub(ui, r);
    draw_palettes(ui, r);
    SDL_RenderSetClipRect(r, NULL);

    draw_left_scrollbar(ui, r);
    draw_screen(ui, r);
}

static int hit(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void paint_at(UiState *ui, int lx, int ly) {
    R01EditSurface surf;
    int px, py;
    if (r01_project_edit_surface(ui->project, &surf) != 0 || ui->edit_mode != UI_MODE_PIXEL) {
        return;
    }
    screen_to_pixel(ui, lx, ly, &px, &py);
    r01_tilemap_plot(surf.pixels, px, py, (uint8_t)ui->project->paint_color);
}

static void toggle_attr_flag(UiState *ui, uint8_t flag) {
    R01EditSurface surf;
    uint8_t a;
    if (r01_project_edit_surface(ui->project, &surf) != 0) {
        return;
    }
    a = r01_tilemap_get_attr(surf.attrs, ui->attr_tx, ui->attr_ty);
    if (a & flag) {
        r01_tilemap_set_attr_bits(surf.attrs, ui->attr_tx, ui->attr_ty, 0, flag);
    } else {
        r01_tilemap_set_attr_bits(surf.attrs, ui->attr_tx, ui->attr_ty, flag, 0);
    }
}

static void cycle_attr_field(UiState *ui, int which) {
    R01EditSurface surf;
    uint8_t a;
    int bank, pal;
    if (r01_project_edit_surface(ui->project, &surf) != 0) {
        return;
    }
    a = r01_tilemap_get_attr(surf.attrs, ui->attr_tx, ui->attr_ty);
    bank = r01_attr_bank(a);
    pal = r01_attr_pal(a);
    if (which == 0) {
        bank = (bank + 1) & 3;
    } else {
        pal = (pal + 1) & 3;
    }
    surf.attrs[ui->attr_ty * R01_SCREEN_TILES_X + ui->attr_tx] =
        r01_attr_pack(bank, pal, r01_attr_flip_h(a), r01_attr_flip_v(a), r01_attr_solid(a), r01_attr_anim(a));
}

int ui_handle_event(UiState *ui, const SDL_Event *e, int logic_x, int logic_y) {
    R01World *w;
    if (e->type == SDL_MOUSEWHEEL) {
        if (in_left_viewport(logic_x, logic_y)) {
            ui->left_scroll_y -= e->wheel.y * 24;
            clamp_left_scroll(ui);
            return 1;
        }
        return 0;
    }

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        SDL_Keymod mod = e->key.keysym.mod;
        if (k == SDLK_TAB) {
            ui->edit_mode = ui->edit_mode == UI_MODE_PIXEL ? UI_MODE_ATTR : UI_MODE_PIXEL;
            snprintf(ui->status, sizeof(ui->status), ui->edit_mode == UI_MODE_ATTR ? "attr mode" : "pixel mode");
            return 1;
        }
        if (k == SDLK_s && (mod & KMOD_CTRL)) {
            char err[128];
            if (r01_project_save_json(ui->project, ui->project_path, err, sizeof(err)) == 0) {
                snprintf(ui->status, sizeof(ui->status), "saved");
            } else {
                snprintf(ui->status, sizeof(ui->status), "save failed");
            }
            return 1;
        }
        if (k == SDLK_o && (mod & KMOD_CTRL)) {
            char err[128];
            if (r01_project_load_json(ui->project, ui->project_path, err, sizeof(err)) == 0) {
                ui->world_tab = r01_hw_world_to_ui(ui->project->active_world);
                snprintf(ui->status, sizeof(ui->status), "loaded");
            } else {
                snprintf(ui->status, sizeof(ui->status), "load failed");
            }
            return 1;
        }
        if (k == SDLK_g && (mod & KMOD_CTRL)) {
            R01ChrPackStatus st;
            w = cur_world(ui);
            st = r01_chr_pack_world_bank(w, ui->project->generate_bank);
            if (st == R01_CHR_OK) {
                ui->bg_bank_tab = ui->project->generate_bank;
                snprintf(ui->status, sizeof(ui->status), "generated bank %d (%d tiles)",
                         ui->project->generate_bank, w->bg_banks[ui->project->generate_bank].tile_count);
            } else if (st == R01_CHR_TOO_MANY_TILES) {
                snprintf(ui->status, sizeof(ui->status), "generate failed: >256 unique tiles");
            } else {
                snprintf(ui->status, sizeof(ui->status), "generate failed");
            }
            return 1;
        }
        if (k == SDLK_g) {
            ui->show_grid = !ui->show_grid;
            snprintf(ui->status, sizeof(ui->status), ui->show_grid ? "grid on" : "grid off");
            return 1;
        }
        if (k == SDLK_f && (mod & KMOD_CTRL)) {
            ui->fullscreen = !ui->fullscreen;
            return 2;
        }
        if (ui->edit_mode == UI_MODE_ATTR) {
            if (k == SDLK_b) {
                cycle_attr_field(ui, 0);
                return 1;
            }
            if (k == SDLK_p) {
                cycle_attr_field(ui, 1);
                return 1;
            }
            if (k == SDLK_h) {
                toggle_attr_flag(ui, R01_ATTR_FLIP_H);
                return 1;
            }
            if (k == SDLK_v) {
                toggle_attr_flag(ui, R01_ATTR_FLIP_V);
                return 1;
            }
            if (k == SDLK_o) {
                toggle_attr_flag(ui, R01_ATTR_SOLID);
                return 1;
            }
            if (k == SDLK_n) {
                toggle_attr_flag(ui, R01_ATTR_ANIM);
                return 1;
            }
        }
        if (k == SDLK_MINUS || k == SDLK_EQUALS || k == SDLK_LEFTBRACKET || k == SDLK_RIGHTBRACKET) {
            R01PalRow *row = cur_pal_row(ui);
            int d = (k == SDLK_EQUALS || k == SDLK_RIGHTBRACKET) ? 1 : -1;
            row->idx[ui->pal_slot] = (uint8_t)((row->idx[ui->pal_slot] + d) & 63);
            return 1;
        }
        if (k == SDLK_1) {
            ui->project->paint_color = 0;
            return 1;
        }
        if (k == SDLK_2) {
            ui->project->paint_color = 1;
            return 1;
        }
        if (k == SDLK_3) {
            ui->project->paint_color = 2;
            return 1;
        }
        if (k == SDLK_4) {
            ui->project->paint_color = 3;
            return 1;
        }
        return 0;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ui->brush_down = 0;
        return 1;
    }

    if (e->type == SDL_MOUSEMOTION && ui->brush_down) {
        paint_at(ui, logic_x, logic_y);
        return 1;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        int t;
        int cy;

        if (in_left_viewport(logic_x, logic_y)) {
            cy = left_cy(ui, logic_y);

            for (t = 1; t <= 8; t++) {
                int x = 4 + (t - 1) * 24;
                if (hit(logic_x, cy, x, 16, 22, 12)) {
                    ui->world_tab = t;
                    ui->project->active_world = r01_ui_world_to_hw(t);
                    ui->project->active_screen = -1;
                    ui->project->active_plane = -1;
                    return 1;
                }
            }

            if (hit(logic_x, cy, UI_WORLD_GRID_X, UI_WORLD_GRID_Y, R01_GRID_SIZE * UI_WORLD_CELL,
                    R01_GRID_SIZE * UI_WORLD_CELL)) {
                int c = (logic_x - UI_WORLD_GRID_X) / UI_WORLD_CELL;
                int row = (cy - UI_WORLD_GRID_Y) / UI_WORLD_CELL;
                w = cur_world(ui);
                if (c >= 0 && c < 8 && row >= 0 && row < 8 && w) {
                    if (ctrl) {
                        if (r01_world_toggle_screen(w, c, row) != 0) {
                            snprintf(ui->status, sizeof(ui->status), "screen cap 32 reached");
                        } else {
                            int idx = r01_world_find_screen(w, c, row);
                            ui->project->active_screen = idx;
                            ui->project->active_plane = -1;
                            snprintf(ui->status, sizeof(ui->status), "toggled screen %d,%d", c, row);
                        }
                    } else {
                        int idx = r01_world_find_screen(w, c, row);
                        ui->project->active_screen = idx;
                        ui->project->active_plane = -1;
                    }
                }
                return 1;
            }

            /* parallax planes */
            for (t = 0; t < R01_MAX_PARALLAX_PLANES; t++) {
                int x = 4 + t * 48;
                int y = UI_PLANES_Y + 18;
                if (hit(logic_x, cy, x, y, 44, 16)) {
                    w = cur_world(ui);
                    if (!w) {
                        return 1;
                    }
                    if (ctrl) {
                        r01_world_toggle_plane(w, t);
                        if (w->planes[t].present) {
                            ui->project->active_plane = t;
                            ui->project->active_screen = -1;
                            snprintf(ui->status, sizeof(ui->status), "plane P%d on", t);
                        } else {
                            if (ui->project->active_plane == t) {
                                ui->project->active_plane = -1;
                            }
                            snprintf(ui->status, sizeof(ui->status), "plane P%d off", t);
                        }
                    } else if (w->planes[t].present) {
                        ui->project->active_plane = t;
                        ui->project->active_screen = -1;
                        snprintf(ui->status, sizeof(ui->status), "editing plane P%d", t);
                    }
                    return 1;
                }
            }

            {
                int b;
                for (b = 0; b < 4; b++) {
                    int x = 68 + b * 22;
                    if (hit(logic_x, cy, x, UI_BG_Y + 2, 20, 12)) {
                        if (r01_project_select_bg_bank(ui->project, b) == b) {
                            ui->bg_bank_tab = b;
                        }
                        return 1;
                    }
                }
            }

            /* palette row tabs */
            for (t = 0; t < 8; t++) {
                int x = 4 + (t % 4) * 24;
                int y = UI_PAL_Y + 16 + (t / 4) * 14;
                if (hit(logic_x, cy, x, y, 22, 12)) {
                    ui->pal_row_tab = t;
                    return 1;
                }
            }
            /* palette slots */
            for (t = 0; t < 4; t++) {
                int x = 8 + t * 46;
                int y = UI_PAL_Y + 48;
                if (hit(logic_x, cy, x, y, 40, 20)) {
                    ui->pal_slot = t;
                    return 1;
                }
            }
            /* master grid */
            if (hit(logic_x, cy, 4, UI_PAL_Y + 110, 16 * 12, 4 * 10)) {
                int mx = (logic_x - 4) / 12;
                int my = (cy - (UI_PAL_Y + 110)) / 10;
                int mi = my * 16 + mx;
                if (mi >= 0 && mi < 64) {
                    cur_pal_row(ui)->idx[ui->pal_slot] = (uint8_t)mi;
                }
                return 1;
            }
            return 1;
        }

        /* PIX / ATTR mode buttons */
        if (hit(logic_x, logic_y, UI_LEFT_W + 52, 2, 40, 12)) {
            ui->edit_mode = UI_MODE_PIXEL;
            return 1;
        }
        if (hit(logic_x, logic_y, UI_LEFT_W + 94, 2, 40, 12)) {
            ui->edit_mode = UI_MODE_ATTR;
            return 1;
        }

        if (ui->edit_mode == UI_MODE_PIXEL) {
            int c;
            for (c = 0; c < 4; c++) {
                int bx = UI_LEFT_W + 180 + c * 18;
                if (hit(logic_x, logic_y, bx, 2, 16, 12)) {
                    ui->project->paint_color = c;
                    return 1;
                }
            }
        }
        {
            int b;
            for (b = 0; b < 4; b++) {
                int bx = UI_LEFT_W + 350 + b * 16;
                if (hit(logic_x, logic_y, bx, 2, 14, 12)) {
                    if (r01_project_select_bg_bank(ui->project, b) == b) {
                        ui->bg_bank_tab = b;
                    }
                    return 1;
                }
            }
        }

        if (hit(logic_x, logic_y, UI_LEFT_W + 8, 28, R01_SCREEN_PX_W * ui->screen_zoom,
                R01_SCREEN_PX_H * ui->screen_zoom)) {
            if (ui->edit_mode == UI_MODE_ATTR) {
                int px, py;
                screen_to_pixel(ui, logic_x, logic_y, &px, &py);
                if (px >= 0 && py >= 0 && px < R01_SCREEN_PX_W && py < R01_SCREEN_PX_H) {
                    ui->attr_tx = px / 8;
                    ui->attr_ty = py / 8;
                }
            } else {
                ui->brush_down = 1;
                paint_at(ui, logic_x, logic_y);
            }
            return 1;
        }
    }
    return 0;
}
