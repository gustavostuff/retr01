#include "ui.h"
#include "font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/play.h"
#include "retr01_studio/spr_pack.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

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

static R01World *cur_world(UiState *ui);

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
    ui->spr_bank_tab = 0;
    ui->spr_tile = 0;
    ui->spr_tool = UI_SPR_TOOL_PLACE;
    ui->spr_size16 = 0;
    ui->oam_sel = -1;
    ui->meta_sel = -1;
    ui->layer = UI_LAYER_BG;
    ui->screen_zoom = 2;
    ui->left_scroll_y = 0;
    ui->edit_mode = UI_MODE_PIXEL;
    ui->attr_tx = 0;
    ui->attr_ty = 0;
    ui->pal_row_tab = 0;
    ui->pal_slot = 0;
    ui->show_grid = 1;
    ui->play_last_tick = 0;
    ui->toast_text[0] = 0;
    ui->toast_until = 0;
    ui->toast_error = 0;
    memset(&ui->play, 0, sizeof(ui->play));
    snprintf(ui->status, sizeof(ui->status),
             "Drop PNG on Worlds | Space=Play | Ctrl+E=export");
    strncpy(ui->project_path, "project.json", R01_PATH_MAX - 1);
    return 0;
}

void ui_toast(UiState *ui, const char *msg, int is_error) {
    if (!ui) {
        return;
    }
    strncpy(ui->toast_text, msg ? msg : "", UI_TOAST_MAX - 1);
    ui->toast_text[UI_TOAST_MAX - 1] = 0;
    ui->toast_error = is_error ? 1 : 0;
    ui->toast_until = SDL_GetTicks() + UI_TOAST_MS;
}

static int path_is_png(const char *path) {
    size_t n;
    if (!path) {
        return 0;
    }
    n = strlen(path);
    if (n < 4) {
        return 0;
    }
    return strcasecmp(path + n - 4, ".png") == 0;
}

static void normalize_drop_path(const char *in, char *out, size_t cap) {
    const char *src = in;
    size_t oi = 0;
    if (!in || !out || cap == 0) {
        return;
    }
    if (strncmp(src, "file://", 7) == 0) {
        src += 7;
        if (strncmp(src, "localhost", 9) == 0) {
            src += 9;
        }
        /* file:///path → ///path; keep a single leading slash */
        while (src[0] == '/' && src[1] == '/') {
            src++;
        }
    }
    while (*src && oi + 1 < cap) {
        if (src[0] == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char h[3] = {src[1], src[2], 0};
            unsigned int v = 0;
            if (sscanf(h, "%x", &v) == 1) {
                out[oi++] = (char)v;
                src += 3;
                continue;
            }
        }
        out[oi++] = *src++;
    }
    out[oi] = 0;
}

int ui_try_import_png(UiState *ui, const char *path) {
    R01World *w;
    char err[128];
    char norm[R01_PATH_MAX];
    if (!ui || !path) {
        return -1;
    }
    normalize_drop_path(path, norm, sizeof(norm));
    if (!path_is_png(norm)) {
        ui_toast(ui, "not a .png file", 1);
        return -1;
    }
    w = cur_world(ui);
    if (!w) {
        ui_toast(ui, "no world selected", 1);
        return -1;
    }
    if (r01_world_import_png(w, norm, err, sizeof(err)) != 0) {
        ui_toast(ui, err[0] ? err : "import failed", 1);
        return -1;
    }
    ui->project->active_screen = w->screen_count > 0 ? 0 : -1;
    ui->project->active_plane = -1;
    ui->left_scroll_y = 0;
    ui->bg_bank_tab = 0;
    {
        char ok[UI_TOAST_MAX];
        int bi, tiles = 0;
        for (bi = 0; bi < R01_BG_BANKS; bi++) {
            tiles += w->bg_banks[bi].tile_count;
        }
        snprintf(ok, sizeof(ok), "imported %dx%d · %d scr · %d tiles", w->grid_cols, w->grid_rows,
                 w->screen_count, tiles);
        ui_toast(ui, ok, 0);
    }
    snprintf(ui->status, sizeof(ui->status), "imported %dx%d (%d scr)", w->grid_cols, w->grid_rows,
             w->screen_count);
    return 0;
}

static int in_worlds_panel(const UiState *ui, int lx, int ly) {
    int cy;
    if (!in_left_viewport(lx, ly)) {
        return 0;
    }
    cy = left_cy(ui, ly);
    return cy >= UI_WORLDS_Y && cy < UI_WORLDS_Y + UI_WORLDS_H;
}

int ui_handle_drop_file(UiState *ui, const char *path, int logic_x, int logic_y) {
    if (!ui || !path) {
        return 0;
    }
    if (!in_worlds_panel(ui, logic_x, logic_y)) {
        ui_toast(ui, "drop PNG on Worlds panel", 1);
        return 1;
    }
    ui_try_import_png(ui, path);
    return 1;
}

void ui_shutdown(UiState *ui) {
    free(ui->project);
    ui->project = NULL;
}

static R01Constraints *edit_constraints(UiState *ui) {
    R01World *w = &ui->project->worlds[ui->project->active_world];
    if (w->present && w->use_constraints) {
        return &w->constraints;
    }
    return &ui->project->constraints;
}

static const char *scroll_mode_name(int mode) {
    switch (mode) {
    case R01_SCROLL_DEADZONE:
        return "SMOOTH DZ";
    case R01_SCROLL_INSTANT:
        return "SCR SNAP";
    case R01_SCROLL_HYBRID:
        return "DZ+WARP";
    default:
        return "CAM LOCK";
    }
}

static const char *scroll_mode_hint(int mode) {
    switch (mode) {
    case R01_SCROLL_DEADZONE:
        return "cam follows at dead-zone edge";
    case R01_SCROLL_INSTANT:
        return "cam snaps per screen cell";
    case R01_SCROLL_HYBRID:
        return "smooth in-screen; Enter/Shift warp";
    default:
        return "cam locked on player center";
    }
}

void ui_tick(UiState *ui) {
    const Uint8 *keys;
    Uint32 now;
    int dx = 0, dy = 0;
    if (!ui->play.active) {
        return;
    }
    now = SDL_GetTicks();
    if (ui->play_last_tick == 0) {
        ui->play_last_tick = now;
        return;
    }
    /* ~60 Hz soft tick */
    if (now - ui->play_last_tick < 16) {
        return;
    }
    ui->play_last_tick = now;
    keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
        dx = -1;
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        dx = 1;
    }
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
        dy = -1;
    }
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
        dy = 1;
    }
    r01_play_tick(&ui->play, ui->project, dx, dy);
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
    font_draw(r, 4, 28 + oy, "DROP PNG HERE", 140, 180, 140);
    {
        int gc = (w && w->grid_cols > 0) ? w->grid_cols : R01_GRID_SIZE;
        int gr = (w && w->grid_rows > 0) ? w->grid_rows : R01_GRID_SIZE;
        char gbuf[24];
        snprintf(gbuf, sizeof(gbuf), "%dx%d", gc, gr);
        font_draw(r, 140, 28 + oy, gbuf, 140, 160, 140);
        for (row = 0; row < gr; row++) {
            for (c = 0; c < gc; c++) {
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
    const int sheet_y = UI_BG_Y + 34 + oy;
    fill_rect(r, 0, UI_BG_Y + oy, UI_LEFT_W, UI_BG_H, 24, 28, 34);
    font_draw(r, 4, UI_BG_Y + 2 + oy, "BG CHR BANKS", 200, 200, 210);
    font_draw(r, 4, UI_BG_Y + 14 + oy, "BANK", 120, 120, 130);
    for (b = 0; b < 4; b++) {
        int x = 36 + b * 22;
        int sel = (b == ui->bg_bank_tab);
        fill_rect(r, x, UI_BG_Y + 12 + oy, 20, 12, sel ? 70 : 40, sel ? 90 : 48, sel ? 120 : 60);
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", b);
            font_draw(r, x + 7, UI_BG_Y + 14 + oy, buf, 230, 230, 240);
        }
    }
    if (w) {
        R01BgBank *bank = &w->bg_banks[ui->bg_bank_tab];
        char buf[32];
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
        snprintf(buf, sizeof(buf), "%d/256 TILES", bank->tile_count);
        font_draw(r, 130, UI_BG_Y + 14 + oy, buf, 160, 160, 170);
    }
}

static void draw_sprite_banks(UiState *ui, SDL_Renderer *r) {
    R01World *w = cur_world(ui);
    int b, ty, tx;
    int oy = -ui->left_scroll_y;
    const int sheet_x = 4;
    const int sheet_y = UI_SPR_Y + 48 + oy;
    fill_rect(r, 0, UI_SPR_Y + oy, UI_LEFT_W, UI_SPR_H, 22, 24, 28);
    font_draw(r, 4, UI_SPR_Y + 2 + oy, "SPRITE CHR", 200, 200, 210);
    font_draw(r, 4, UI_SPR_Y + 14 + oy, "BANK", 120, 120, 130);
    for (b = 0; b < 4; b++) {
        int x = 36 + b * 22;
        int sel = (b == ui->spr_bank_tab);
        fill_rect(r, x, UI_SPR_Y + 12 + oy, 20, 12, sel ? 70 : 40, sel ? 90 : 48, sel ? 120 : 60);
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", b);
            font_draw(r, x + 7, UI_SPR_Y + 14 + oy, buf, 230, 230, 240);
        }
    }
    {
        int x = 132;
        fill_rect(r, x, UI_SPR_Y + 12 + oy, 60, 12, ui->spr_size16 ? 80 : 40, ui->spr_size16 ? 100 : 48,
                  ui->spr_size16 ? 70 : 55);
        font_draw(r, x + 4, UI_SPR_Y + 14 + oy, ui->spr_size16 ? "OAM 8X16" : "OAM 8X8", 230, 230, 240);
    }
    if (w) {
        R01SprBank *bank = &w->spr_banks[ui->spr_bank_tab];
        char buf[40];
        for (ty = 0; ty < 8; ty++) {
            for (tx = 0; tx < 16; tx++) {
                int ti = ty * 16 + tx;
                int x = sheet_x + tx * 8;
                int y = sheet_y + ty * 8;
                if (ti >= bank->tile_count) {
                    fill_rect(r, x, y, 8, 8, 32, 34, 40);
                } else {
                    draw_chr_tile(r, x, y, &bank->chr[ti * R01_TILE_BYTES]);
                }
                if (ti == ui->spr_tile) {
                    draw_rect(r, x, y, 8, 8, 255, 200, 80);
                }
            }
        }
        snprintf(buf, sizeof(buf), "TILE %d  USED %d/256", ui->spr_tile, bank->tile_count);
        font_draw(r, 4, UI_SPR_Y + 28 + oy, buf, 160, 170, 160);
        font_draw(r, 4, UI_SPR_Y + 38 + oy, "CLICK TILE: EDIT   CTRL+G: PACK", 100, 110, 100);
    }
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

static void draw_constraints(UiState *ui, SDL_Renderer *r) {
    int oy = -ui->left_scroll_y;
    R01World *w = cur_world(ui);
    R01Constraints *c = edit_constraints(ui);
    char buf[64];
    int y0 = UI_CONSTRAINTS_Y + oy;
    int free_w = R01_SCREEN_PX_W - 2 * c->deadzone_x;
    int free_h = R01_SCREEN_PX_H - 2 * c->deadzone_y;

    fill_rect(r, 0, y0, UI_LEFT_W, UI_CONSTRAINTS_H, 18, 20, 24);
    font_draw(r, 4, y0 + 4, "PLAY / CONSTRAINTS", 200, 200, 210);
    fill_rect(r, 148, y0 + 2, 44, 12, ui->play.active ? 90 : 40, ui->play.active ? 120 : 48,
              ui->play.active ? 70 : 55);
    font_draw(r, 152, y0 + 4, ui->play.active ? "STOP" : "PLAY", 230, 230, 240);

    font_draw(r, 4, y0 + 18, "SCOPE", 120, 120, 130);
    fill_rect(r, 44, y0 + 16, 72, 12, (w && w->use_constraints) ? 80 : 40,
              (w && w->use_constraints) ? 100 : 48, (w && w->use_constraints) ? 70 : 55);
    font_draw(r, 48, y0 + 18, w && w->use_constraints ? "THIS WORLD" : "PROJECT", 230, 230, 240);

    font_draw(r, 4, y0 + 34, "SCROLL", 120, 120, 130);
    fill_rect(r, 48, y0 + 32, 84, 12, 40, 48, 60);
    font_draw(r, 52, y0 + 34, scroll_mode_name(c->scroll_mode), 230, 230, 240);
    fill_rect(r, 136, y0 + 32, 56, 12, 40, 48, 60);
    font_draw(r, 140, y0 + 34, c->transition == R01_XITION_FADE ? "FADE-BLK" : "CUT", 230, 230, 240);
    font_draw(r, 4, y0 + 48, scroll_mode_hint(c->scroll_mode), 100, 110, 100);

    snprintf(buf, sizeof(buf), "DEAD ZONE  free %dx%d  inset %d,%d", free_w > 0 ? free_w : 0,
             free_h > 0 ? free_h : 0, c->deadzone_x, c->deadzone_y);
    font_draw(r, 4, y0 + 62, buf, 160, 160, 170);
    fill_rect(r, 4, y0 + 74, 44, 12, 40, 48, 60);
    font_draw(r, 14, y0 + 76, "X-", 230, 230, 240);
    fill_rect(r, 52, y0 + 74, 44, 12, 40, 48, 60);
    font_draw(r, 62, y0 + 76, "X+", 230, 230, 240);
    fill_rect(r, 100, y0 + 74, 44, 12, 40, 48, 60);
    font_draw(r, 110, y0 + 76, "Y-", 230, 230, 240);
    fill_rect(r, 148, y0 + 74, 44, 12, 40, 48, 60);
    font_draw(r, 158, y0 + 76, "Y+", 230, 230, 240);

    snprintf(buf, sizeof(buf), "BG ANIM %d   ENEMY ANIM %d", c->anim_rate, c->enemy_anim_rate);
    font_draw(r, 4, y0 + 92, buf, 160, 160, 170);
    fill_rect(r, 4, y0 + 104, 28, 12, 40, 48, 60);
    font_draw(r, 8, y0 + 106, "BG-", 230, 230, 240);
    fill_rect(r, 36, y0 + 104, 28, 12, 40, 48, 60);
    font_draw(r, 40, y0 + 106, "BG+", 230, 230, 240);
    fill_rect(r, 68, y0 + 104, 28, 12, 40, 48, 60);
    font_draw(r, 72, y0 + 106, "EN-", 230, 230, 240);
    fill_rect(r, 100, y0 + 104, 28, 12, 40, 48, 60);
    font_draw(r, 104, y0 + 106, "EN+", 230, 230, 240);

    snprintf(buf, sizeof(buf), "PLAYER META %d", c->player_meta);
    font_draw(r, 4, y0 + 122, buf, 160, 160, 170);
    fill_rect(r, 100, y0 + 120, 28, 12, 40, 48, 60);
    font_draw(r, 104, y0 + 122, "M-", 230, 230, 240);
    fill_rect(r, 132, y0 + 120, 28, 12, 40, 48, 60);
    font_draw(r, 136, y0 + 122, "M+", 230, 230, 240);
    fill_rect(r, 4, y0 + 136, 88, 12, ui->project->has_cart_save ? 80 : 40,
              ui->project->has_cart_save ? 100 : 48, ui->project->has_cart_save ? 70 : 55);
    font_draw(r, 8, y0 + 138, "I2C CART SAVE", 230, 230, 240);
    font_draw(r, 100, y0 + 138, "Ctrl+E export", 110, 110, 120);

    font_draw(r, 4, y0 + 154, "MOVE WASD/ARROWS", 110, 120, 110);
    font_draw(r, 4, y0 + 166, "Z=X  X=Y  SHIFT=COIN  ENTER=START", 110, 120, 110);
    font_draw(r, 4, y0 + 178, "SPACE play/stop", 100, 100, 110);
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

static void draw_oam_sprite(UiState *ui, SDL_Renderer *r, R01World *w, const R01Oam *o, int view_x, int view_y,
                            int zx, int selected) {
    int row, h = r01_oam_size_16(o->attr) ? 2 : 1;
    int bank = r01_attr_bank(o->attr);
    for (row = 0; row < h; row++) {
        int ty, tx;
        uint8_t tile = (uint8_t)(o->tile + row);
        if (r01_oam_size_16(o->attr)) {
            tile = (uint8_t)((o->tile & 0xFE) + row);
        }
        for (ty = 0; ty < 8; ty++) {
            for (tx = 0; tx < 8; tx++) {
                uint8_t cr, cg, cb;
                int opaque = 0;
                r01_spr_chr_rgb(ui->project, w, bank, tile, o->attr, tx, ty, &cr, &cg, &cb, &opaque);
                if (!opaque) {
                    continue;
                }
                fill_rect(r, view_x + (o->x + tx) * zx, view_y + (o->y + row * 8 + ty) * zx, zx, zx, cr, cg, cb);
            }
        }
    }
    if (selected) {
        int hh = h * 8 * zx;
        draw_rect(r, view_x + o->x * zx, view_y + o->y * zx, 8 * zx, hh, 255, 220, 80);
    }
}

static void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int view_x = UI_LEFT_W + 8;
    int view_y = 28;
    int zx = ui->screen_zoom;
    int x, y;
    char buf[64];
    const R01Constraints *c = r01_project_constraints(ui->project);

    fill_rect(r, UI_LEFT_W, 0, UI_LOGIC_W - UI_LEFT_W, UI_LOGIC_H, 18, 20, 26);
    font_draw(r, UI_LEFT_W + 4, 4, "PLAY", 200, 255, 180);
    fill_rect(r, UI_LEFT_W + 52, 2, 40, 12, 90, 120, 70);
    font_draw(r, UI_LEFT_W + 56, 4, "STOP", 230, 230, 240);
    snprintf(buf, sizeof(buf), "%s  CAM %d,%d", scroll_mode_name(c ? c->scroll_mode : 0), ui->play.cam_x,
             ui->play.cam_y);
    font_draw(r, UI_LEFT_W + 100, 4, buf, 160, 200, 160);

    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t cr, cg, cb;
            r01_play_sample(ui->project, &ui->play, x, y, &cr, &cg, &cb);
            fill_rect(r, view_x + x * zx, view_y + y * zx, zx, zx, cr, cg, cb);
        }
    }
    {
        int px = ui->play.player_x - ui->play.cam_x;
        int py = ui->play.player_y - ui->play.cam_y;
        int dzx = c ? c->deadzone_x : R01_PLAY_DZ_INSET_X;
        int dzy = c ? c->deadzone_y : R01_PLAY_DZ_INSET_Y;
        int free_w = R01_SCREEN_PX_W - 2 * dzx;
        int free_h = R01_SCREEN_PX_H - 2 * dzy;
        if (free_w > 0 && free_h > 0) {
            int mode = c ? c->scroll_mode : R01_SCROLL_DEADZONE;
            if (mode == R01_SCROLL_DEADZONE || mode == R01_SCROLL_HYBRID) {
                draw_rect(r, view_x + dzx * zx, view_y + dzy * zx, free_w * zx, free_h * zx, 60, 90, 70);
            }
        }
        if (px + R01_PLAY_PLAYER_SIZE > 0 && py + R01_PLAY_PLAYER_SIZE > 0 && px < R01_SCREEN_PX_W &&
            py < R01_SCREEN_PX_H) {
            /* Cyan player, faded via kit-nearest like BG/SPR slots. */
            int t = ui->play.fade_step;
            int den = R01_PLAY_FADE_FRAMES;
            int lr = 80, lg = 220, lb = 255;
            uint8_t pr, pg, pb;
            int ki;
            if (ui->play.fade_phase == R01_PLAY_FADE_OUT) {
                lr = (80 * (den - t)) / den;
                lg = (220 * (den - t)) / den;
                lb = (255 * (den - t)) / den;
            } else if (ui->play.fade_phase == R01_PLAY_FADE_IN) {
                lr = (80 * t) / den;
                lg = (220 * t) / den;
                lb = (255 * t) / den;
            }
            ki = r01_nearest_kit_index((uint8_t)lr, (uint8_t)lg, (uint8_t)lb);
            r01_kit_rgb(ki, &pr, &pg, &pb);
            fill_rect(r, view_x + px * zx, view_y + py * zx, R01_PLAY_PLAYER_SIZE * zx,
                      R01_PLAY_PLAYER_SIZE * zx, pr, pg, pb);
            if (ki != 0) {
                draw_rect(r, view_x + px * zx, view_y + py * zx, R01_PLAY_PLAYER_SIZE * zx,
                          R01_PLAY_PLAYER_SIZE * zx, 255, 255, 255);
            }
        }
    }
    {
        const char *fp = "IDLE";
        if (ui->play.fade_phase == R01_PLAY_FADE_OUT) {
            fp = "OUT";
        } else if (ui->play.fade_phase == R01_PLAY_FADE_IN) {
            fp = "IN";
        }
        snprintf(buf, sizeof(buf), "PLAYER %d,%d  HOME %d,%d  FADE %s %d/%d", ui->play.player_x,
                 ui->play.player_y, ui->play.home_col, ui->play.home_row, fp, ui->play.fade_step,
                 R01_PLAY_FADE_FRAMES);
    }
    font_draw(r, view_x, view_y + R01_SCREEN_PX_H * zx + 4, buf, 160, 200, 160);
    font_draw(r, UI_LEFT_W + 4, UI_LOGIC_H - 12, ui->status, 130, 130, 140);
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
    int spr_tile_edit = (ui->layer == UI_LAYER_SPR && ui->spr_tool == UI_SPR_TOOL_TILE);

    if (ui->play.active) {
        draw_play_view(ui, r);
        return;
    }

    fill_rect(r, UI_LEFT_W, 0, UI_LOGIC_W - UI_LEFT_W, UI_LOGIC_H, 18, 20, 26);
    font_draw(r, UI_LEFT_W + 4, 4, "SCREEN", 200, 200, 210);

    {
        int mx = UI_LEFT_W + 52;
        fill_rect(r, mx, 2, 28, 12, ui->layer == UI_LAYER_BG ? 80 : 40, ui->layer == UI_LAYER_BG ? 100 : 48,
                  ui->layer == UI_LAYER_BG ? 70 : 55);
        font_draw(r, mx + 4, 4, "BG", 230, 230, 240);
        fill_rect(r, mx + 30, 2, 40, 12, ui->layer == UI_LAYER_SPR ? 80 : 40, ui->layer == UI_LAYER_SPR ? 100 : 48,
                  ui->layer == UI_LAYER_SPR ? 70 : 55);
        font_draw(r, mx + 34, 4, "SPR", 230, 230, 240);
        fill_rect(r, mx + 74, 2, 44, 12, ui->edit_mode == UI_MODE_PIXEL ? 80 : 40,
                  ui->edit_mode == UI_MODE_PIXEL ? 100 : 48, ui->edit_mode == UI_MODE_PIXEL ? 70 : 55);
        font_draw(r, mx + 78, 4, "PIXEL", 230, 230, 240);
        fill_rect(r, mx + 122, 2, 36, 12, ui->edit_mode == UI_MODE_ATTR ? 80 : 40,
                  ui->edit_mode == UI_MODE_ATTR ? 100 : 48, ui->edit_mode == UI_MODE_ATTR ? 70 : 55);
        font_draw(r, mx + 126, 4, "ATTR", 230, 230, 240);
    }

    if (ui->layer == UI_LAYER_SPR) {
        int mx = UI_LEFT_W + 200;
        fill_rect(r, mx, 2, 40, 12, ui->spr_tool == UI_SPR_TOOL_PLACE ? 80 : 40,
                  ui->spr_tool == UI_SPR_TOOL_PLACE ? 100 : 48, ui->spr_tool == UI_SPR_TOOL_PLACE ? 70 : 55);
        font_draw(r, mx + 4, 4, "PLACE", 230, 230, 240);
        fill_rect(r, mx + 44, 2, 40, 12, ui->spr_tool == UI_SPR_TOOL_TILE ? 80 : 40,
                  ui->spr_tool == UI_SPR_TOOL_TILE ? 100 : 48, ui->spr_tool == UI_SPR_TOOL_TILE ? 70 : 55);
        font_draw(r, mx + 50, 4, "TILE", 230, 230, 240);
        if (ui->oam_sel >= 0 && grid && ui->oam_sel < grid->oam_count) {
            uint8_t a = grid->oam[ui->oam_sel].attr;
            snprintf(buf, sizeof(buf), "OAM B%d P%d%s%s%s%s", r01_attr_bank(a), r01_attr_pal(a),
                     r01_attr_flip_h(a) ? " H" : "", r01_attr_flip_v(a) ? " V" : "",
                     r01_oam_priority(a) ? " PR" : "", r01_oam_size_16(a) ? " 16" : "");
            font_draw(r, UI_LEFT_W + 290, 4, buf, 180, 200, 160);
        }
    } else if (ui->edit_mode == UI_MODE_PIXEL) {
        font_draw(r, UI_LEFT_W + 200, 4, "COLOR", 140, 140, 150);
        {
            int c;
            for (c = 0; c < 4; c++) {
                int bx = UI_LEFT_W + 240 + c * 18;
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
        font_draw(r, UI_LEFT_W + 200, 4, buf, 180, 200, 160);
    }

    font_draw(r, UI_LEFT_W + 400, 4, "BK", 140, 140, 150);
    {
        int b;
        for (b = 0; b < 4; b++) {
            int bx = UI_LEFT_W + 418 + b * 14;
            int sel = (b == ui->project->generate_bank);
            fill_rect(r, bx, 2, 12, 12, sel ? 80 : 40, sel ? 100 : 48, sel ? 70 : 55);
            snprintf(buf, sizeof(buf), "%d", b);
            font_draw(r, bx + 3, 4, buf, 230, 230, 240);
        }
    }

    fill_rect(r, view_x, view_y, R01_SCREEN_PX_W * zx, R01_SCREEN_PX_H * zx, 10, 10, 12);

    if (spr_tile_edit && w) {
        int scale = 12;
        int ox = view_x + 16;
        int oy = view_y + 16;
        font_draw(r, view_x + 4, view_y + 4, "SPR TILE EDIT", 160, 160, 170);
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                uint8_t c = r01_spr_tile_get_pixel(w, ui->spr_bank_tab, ui->spr_tile, x, y);
                SDL_Color col = GRAY[c & 3];
                fill_rect(r, ox + x * scale, oy + y * scale, scale - 1, scale - 1, col.r, col.g, col.b);
            }
        }
        draw_rect(r, ox - 1, oy - 1, 8 * scale + 1, 8 * scale + 1, 90, 95, 110);
        snprintf(buf, sizeof(buf), "BANK %d TILE %d", ui->spr_bank_tab, ui->spr_tile);
        font_draw(r, ox, oy + 8 * scale + 6, buf, 160, 160, 170);
    } else if (!has_surf) {
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
        if (grid && !surf.is_plane) {
            int oi;
            for (oi = 0; oi < grid->oam_count; oi++) {
                draw_oam_sprite(ui, r, w, &grid->oam[oi], view_x, view_y, zx, oi == ui->oam_sel);
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
        if (ui->layer == UI_LAYER_BG && ui->edit_mode == UI_MODE_ATTR) {
            draw_rect(r, view_x + ui->attr_tx * 8 * zx, view_y + ui->attr_ty * 8 * zx, 8 * zx, 8 * zx, 255, 220,
                      80);
        }
        if (surf.is_plane) {
            snprintf(buf, sizeof(buf), "PLANE P%d", surf.index);
        } else if (grid) {
            snprintf(buf, sizeof(buf), "CELL %d.%d  OAM %d", grid->col, grid->row, grid->oam_count);
        } else {
            snprintf(buf, sizeof(buf), "SCREEN");
        }
        font_draw(r, view_x, view_y + R01_SCREEN_PX_H * zx + 4, buf, 160, 160, 170);
    }

    font_draw(r, UI_LEFT_W + 4, UI_LOGIC_H - 12, ui->status, 130, 130, 140);
}

static void draw_toast(UiState *ui, SDL_Renderer *r) {
    int tw, th, x, y;
    if (!ui->toast_text[0]) {
        return;
    }
    if (SDL_GetTicks() >= ui->toast_until) {
        ui->toast_text[0] = 0;
        return;
    }
    tw = (int)strlen(ui->toast_text) * 6 + 16;
    if (tw > UI_LOGIC_W - UI_LEFT_W - 16) {
        tw = UI_LOGIC_W - UI_LEFT_W - 16;
    }
    th = 20;
    x = UI_LEFT_W + (UI_LOGIC_W - UI_LEFT_W - tw) / 2;
    y = UI_LOGIC_H - 40;
    if (ui->toast_error) {
        fill_rect(r, x, y, tw, th, 90, 28, 32);
        draw_rect(r, x, y, tw, th, 200, 80, 80);
        font_draw(r, x + 8, y + 6, ui->toast_text, 255, 200, 200);
    } else {
        fill_rect(r, x, y, tw, th, 28, 70, 40);
        draw_rect(r, x, y, tw, th, 80, 180, 100);
        font_draw(r, x + 8, y + 6, ui->toast_text, 220, 255, 220);
    }
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
    draw_sprite_banks(ui, r);
    draw_palettes(ui, r);
    draw_constraints(ui, r);
    SDL_RenderSetClipRect(r, NULL);

    draw_left_scrollbar(ui, r);
    draw_screen(ui, r);
    draw_toast(ui, r);
}

static int hit(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void paint_at(UiState *ui, int lx, int ly) {
    R01EditSurface surf;
    int px, py;
    if (ui->layer != UI_LAYER_BG || ui->edit_mode != UI_MODE_PIXEL) {
        return;
    }
    if (r01_project_edit_surface(ui->project, &surf) != 0) {
        return;
    }
    screen_to_pixel(ui, lx, ly, &px, &py);
    r01_tilemap_plot(surf.pixels, px, py, (uint8_t)ui->project->paint_color);
}

static void paint_spr_tile_at(UiState *ui, int lx, int ly) {
    R01World *w = cur_world(ui);
    int view_x = UI_LEFT_W + 8;
    int view_y = 28;
    int scale = 12;
    int ox = view_x + 16;
    int oy = view_y + 16;
    int px, py;
    if (!w || ui->spr_tool != UI_SPR_TOOL_TILE) {
        return;
    }
    px = (lx - ox) / scale;
    py = (ly - oy) / scale;
    if (px < 0 || py < 0 || px >= 8 || py >= 8) {
        return;
    }
    r01_spr_tile_plot(w, ui->spr_bank_tab, ui->spr_tile, px, py, (uint8_t)ui->project->paint_color);
    ui->project->generate_bank = ui->spr_bank_tab;
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
        if (k == SDLK_SPACE) {
            if (ui->play.active) {
                r01_play_stop(&ui->play);
                ui->play_last_tick = 0;
                snprintf(ui->status, sizeof(ui->status), "play stopped");
            } else {
                r01_play_start(&ui->play, ui->project);
                ui->play_last_tick = 0;
                snprintf(ui->status, sizeof(ui->status),
                         "play — WASD move; Z/X; Shift=coin Enter=start");
            }
            return 1;
        }
        if (ui->play.active) {
            if (k == SDLK_ESCAPE) {
                r01_play_stop(&ui->play);
                ui->play_last_tick = 0;
                snprintf(ui->status, sizeof(ui->status), "play stopped");
                return 1;
            }
            /* Pad: Z=X, X=Y, Shift=coin, Enter=start */
            if (k == SDLK_z) {
                r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_X);
                snprintf(ui->status, sizeof(ui->status), "pad X (Z)");
                return 1;
            }
            if (k == SDLK_x) {
                r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_Y);
                snprintf(ui->status, sizeof(ui->status), "pad Y (X)");
                return 1;
            }
            if (k == SDLK_LSHIFT || k == SDLK_RSHIFT) {
                if (r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_COIN)) {
                    snprintf(ui->status, sizeof(ui->status), "coin → warp screen");
                } else {
                    snprintf(ui->status, sizeof(ui->status), "coin (DZ+WARP to change screen)");
                }
                return 1;
            }
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                if (r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_START)) {
                    snprintf(ui->status, sizeof(ui->status), "start → warp screen");
                } else {
                    snprintf(ui->status, sizeof(ui->status), "start (DZ+WARP to change screen)");
                }
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
            if (k == SDLK_e && (mod & KMOD_CTRL)) {
                char err[128];
                char stem[R01_PATH_MAX];
                strncpy(stem, ui->project_path, R01_PATH_MAX - 1);
                stem[R01_PATH_MAX - 1] = 0;
                {
                    char *dot = strrchr(stem, '.');
                    if (dot && (strcmp(dot, ".json") == 0)) {
                        *dot = 0;
                    }
                }
                if (r01_export_bundle(ui->project, stem, err, sizeof(err)) == 0) {
                    snprintf(ui->status, sizeof(ui->status), "exported .retr01");
                } else {
                    snprintf(ui->status, sizeof(ui->status), "export failed");
                }
                return 1;
            }
            if (k == SDLK_o && (mod & KMOD_CTRL)) {
                char err[128];
                if (r01_project_load_json(ui->project, ui->project_path, err, sizeof(err)) == 0) {
                    ui->world_tab = r01_hw_world_to_ui(ui->project->active_world);
                    r01_play_stop(&ui->play);
                    snprintf(ui->status, sizeof(ui->status), "loaded");
                } else {
                    snprintf(ui->status, sizeof(ui->status), "load failed");
                }
                return 1;
            }
            return 1;
        }
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
        if (k == SDLK_e && (mod & KMOD_CTRL)) {
            char err[128];
            char stem[R01_PATH_MAX];
            strncpy(stem, ui->project_path, R01_PATH_MAX - 1);
            stem[R01_PATH_MAX - 1] = 0;
            {
                char *dot = strrchr(stem, '.');
                if (dot && (strcmp(dot, ".json") == 0)) {
                    *dot = 0;
                }
            }
            if (r01_export_bundle(ui->project, stem, err, sizeof(err)) == 0) {
                snprintf(ui->status, sizeof(ui->status), "exported .retr01");
            } else {
                snprintf(ui->status, sizeof(ui->status), "export failed");
            }
            return 1;
        }
        if (k == SDLK_i && (mod & KMOD_CTRL)) {
            char png_path[R01_PATH_MAX];
            char stem[R01_PATH_MAX];
            strncpy(stem, ui->project_path, R01_PATH_MAX - 1);
            stem[R01_PATH_MAX - 1] = 0;
            {
                char *dot = strrchr(stem, '.');
                if (dot && (strcmp(dot, ".json") == 0)) {
                    *dot = 0;
                }
            }
            {
                size_t n = strlen(stem);
                if (n + 4 < sizeof(png_path)) {
                    memcpy(png_path, stem, n);
                    memcpy(png_path + n, ".png", 5);
                } else {
                    strncpy(png_path, "import.png", sizeof(png_path) - 1);
                    png_path[sizeof(png_path) - 1] = 0;
                }
            }
            if (access(png_path, R_OK) != 0) {
                strncpy(png_path, "import.png", sizeof(png_path) - 1);
                png_path[sizeof(png_path) - 1] = 0;
            }
            if (access(png_path, R_OK) != 0) {
                ui_toast(ui, "no project.png / import.png", 1);
            } else {
                ui_try_import_png(ui, png_path);
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
            if (ui->layer == UI_LAYER_SPR) {
                st = r01_spr_pack_world_bank(w, ui->project->generate_bank);
                if (st == R01_CHR_OK) {
                    ui->spr_bank_tab = ui->project->generate_bank;
                    snprintf(ui->status, sizeof(ui->status), "spr bank %d (%d tiles)",
                             ui->project->generate_bank, w->spr_banks[ui->project->generate_bank].tile_count);
                } else {
                    snprintf(ui->status, sizeof(ui->status), "spr generate failed");
                }
            } else {
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
            }
            return 1;
        }
        if (k == SDLK_g) {
            ui->show_grid = !ui->show_grid;
            snprintf(ui->status, sizeof(ui->status), ui->show_grid ? "grid on" : "grid off");
            return 1;
        }
        if (k == SDLK_l) {
            ui->layer = ui->layer == UI_LAYER_BG ? UI_LAYER_SPR : UI_LAYER_BG;
            snprintf(ui->status, sizeof(ui->status), ui->layer == UI_LAYER_SPR ? "sprite layer" : "bg layer");
            return 1;
        }
        if (k == SDLK_DELETE || k == SDLK_BACKSPACE) {
            R01Screen *s = r01_project_active_screen(ui->project);
            if (ui->layer == UI_LAYER_SPR && s && ui->oam_sel >= 0) {
                r01_screen_oam_remove(s, ui->oam_sel);
                ui->oam_sel = -1;
                snprintf(ui->status, sizeof(ui->status), "oam deleted");
                return 1;
            }
        }
        if (k == SDLK_m && ui->layer == UI_LAYER_SPR) {
            R01Screen *s = r01_project_active_screen(ui->project);
            w = cur_world(ui);
            if (s && w && ui->oam_sel >= 0) {
                int idx = ui->oam_sel;
                int mid = r01_meta_create_from_oam(w, s, &idx, 1);
                if (mid >= 0) {
                    ui->meta_sel = mid;
                    snprintf(ui->status, sizeof(ui->status), "meta %d from oam", mid);
                } else {
                    snprintf(ui->status, sizeof(ui->status), "meta create failed");
                }
                return 1;
            }
        }
        if (k == SDLK_f && (mod & KMOD_CTRL)) {
            ui->fullscreen = !ui->fullscreen;
            return 2;
        }
        if (ui->edit_mode == UI_MODE_ATTR && ui->layer == UI_LAYER_BG) {
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
        if (ui->layer == UI_LAYER_SPR && ui->oam_sel >= 0) {
            R01Screen *s = r01_project_active_screen(ui->project);
            if (s && ui->oam_sel < s->oam_count) {
                R01Oam *o = &s->oam[ui->oam_sel];
                uint8_t a = o->attr;
                if (k == SDLK_b) {
                    o->attr = r01_oam_pack((r01_attr_bank(a) + 1) & 3, r01_attr_pal(a), r01_attr_flip_h(a),
                                           r01_attr_flip_v(a), r01_oam_priority(a), r01_oam_size_16(a));
                    return 1;
                }
                if (k == SDLK_p) {
                    o->attr = r01_oam_pack(r01_attr_bank(a), (r01_attr_pal(a) + 1) & 3, r01_attr_flip_h(a),
                                           r01_attr_flip_v(a), r01_oam_priority(a), r01_oam_size_16(a));
                    return 1;
                }
                if (k == SDLK_h) {
                    o->attr = r01_oam_pack(r01_attr_bank(a), r01_attr_pal(a), !r01_attr_flip_h(a), r01_attr_flip_v(a),
                                           r01_oam_priority(a), r01_oam_size_16(a));
                    return 1;
                }
                if (k == SDLK_v) {
                    o->attr = r01_oam_pack(r01_attr_bank(a), r01_attr_pal(a), r01_attr_flip_h(a), !r01_attr_flip_v(a),
                                           r01_oam_priority(a), r01_oam_size_16(a));
                    return 1;
                }
                if (k == SDLK_o) {
                    o->attr = r01_oam_pack(r01_attr_bank(a), r01_attr_pal(a), r01_attr_flip_h(a), r01_attr_flip_v(a),
                                           !r01_oam_priority(a), r01_oam_size_16(a));
                    return 1;
                }
                if (k == SDLK_n) {
                    o->attr = r01_oam_pack(r01_attr_bank(a), r01_attr_pal(a), r01_attr_flip_h(a), r01_attr_flip_v(a),
                                           r01_oam_priority(a), !r01_oam_size_16(a));
                    return 1;
                }
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

    if (ui->play.active && e->type == SDL_MOUSEMOTION) {
        return 0;
    }

    if (e->type == SDL_MOUSEMOTION && ui->brush_down) {
        if (ui->layer == UI_LAYER_SPR && ui->spr_tool == UI_SPR_TOOL_TILE) {
            paint_spr_tile_at(ui, logic_x, logic_y);
        } else {
            paint_at(ui, logic_x, logic_y);
        }
        return 1;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        int t;
        int cy;

        /* PLAY / STOP toolbar */
        if (hit(logic_x, logic_y, UI_LEFT_W + 52, 2, 40, 12) && ui->play.active) {
            r01_play_stop(&ui->play);
            ui->play_last_tick = 0;
            snprintf(ui->status, sizeof(ui->status), "play stopped");
            return 1;
        }

        if (in_left_viewport(logic_x, logic_y)) {
            cy = left_cy(ui, logic_y);
            w = cur_world(ui);

            /* Constraints panel */
            if (cy >= UI_CONSTRAINTS_Y && cy < UI_CONSTRAINTS_Y + UI_CONSTRAINTS_H) {
                R01Constraints *c = edit_constraints(ui);
                w = cur_world(ui);
                if (hit(logic_x, cy, 148, UI_CONSTRAINTS_Y + 2, 44, 12)) {
                    if (ui->play.active) {
                        r01_play_stop(&ui->play);
                        ui->play_last_tick = 0;
                        snprintf(ui->status, sizeof(ui->status), "play stopped");
                    } else {
                        r01_play_start(&ui->play, ui->project);
                        ui->play_last_tick = 0;
                        snprintf(ui->status, sizeof(ui->status),
                                 "play — WASD move; Z/X; Shift=coin Enter=start");
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 44, UI_CONSTRAINTS_Y + 16, 72, 12) && w) {
                    w->use_constraints = !w->use_constraints;
                    snprintf(ui->status, sizeof(ui->status),
                             w->use_constraints ? "world constraints" : "project constraints");
                    return 1;
                }
                if (hit(logic_x, cy, 48, UI_CONSTRAINTS_Y + 32, 84, 12)) {
                    c->scroll_mode = (c->scroll_mode + 1) % 4;
                    snprintf(ui->status, sizeof(ui->status), "scroll: %s (%s)",
                             scroll_mode_name(c->scroll_mode), scroll_mode_hint(c->scroll_mode));
                    return 1;
                }
                if (hit(logic_x, cy, 136, UI_CONSTRAINTS_Y + 32, 56, 12)) {
                    c->transition = c->transition == R01_XITION_CUT ? R01_XITION_FADE : R01_XITION_CUT;
                    snprintf(ui->status, sizeof(ui->status),
                             c->transition == R01_XITION_FADE ? "transition: fade-to-black" : "transition: cut");
                    return 1;
                }
                if (hit(logic_x, cy, 4, UI_CONSTRAINTS_Y + 74, 44, 12)) {
                    if (c->deadzone_x > 0) {
                        c->deadzone_x--;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 52, UI_CONSTRAINTS_Y + 74, 44, 12)) {
                    if (c->deadzone_x < 56) {
                        c->deadzone_x++;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 100, UI_CONSTRAINTS_Y + 74, 44, 12)) {
                    if (c->deadzone_y > 0) {
                        c->deadzone_y--;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 148, UI_CONSTRAINTS_Y + 74, 44, 12)) {
                    if (c->deadzone_y < 52) {
                        c->deadzone_y++;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 4, UI_CONSTRAINTS_Y + 104, 28, 12)) {
                    if (c->anim_rate > 1) {
                        c->anim_rate--;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 36, UI_CONSTRAINTS_Y + 104, 28, 12)) {
                    if (c->anim_rate < 120) {
                        c->anim_rate++;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 68, UI_CONSTRAINTS_Y + 104, 28, 12)) {
                    if (c->enemy_anim_rate > 1) {
                        c->enemy_anim_rate--;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 100, UI_CONSTRAINTS_Y + 104, 28, 12)) {
                    if (c->enemy_anim_rate < 120) {
                        c->enemy_anim_rate++;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 100, UI_CONSTRAINTS_Y + 120, 28, 12)) {
                    if (c->player_meta > -1) {
                        c->player_meta--;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 132, UI_CONSTRAINTS_Y + 120, 28, 12)) {
                    if (c->player_meta < R01_MAX_METASPRITES - 1) {
                        c->player_meta++;
                    }
                    return 1;
                }
                if (hit(logic_x, cy, 4, UI_CONSTRAINTS_Y + 136, 88, 12)) {
                    ui->project->has_cart_save = !ui->project->has_cart_save;
                    snprintf(ui->status, sizeof(ui->status),
                             ui->project->has_cart_save ? "cart I2C save on" : "cart I2C save off");
                    return 1;
                }
                return 1;
            }

            if (ui->play.active) {
                return 1;
            }

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

            if (hit(logic_x, cy, UI_WORLD_GRID_X, UI_WORLD_GRID_Y,
                    (w && w->grid_cols > 0 ? w->grid_cols : R01_GRID_SIZE) * UI_WORLD_CELL,
                    (w && w->grid_rows > 0 ? w->grid_rows : R01_GRID_SIZE) * UI_WORLD_CELL)) {
                int gc = w && w->grid_cols > 0 ? w->grid_cols : R01_GRID_SIZE;
                int gr = w && w->grid_rows > 0 ? w->grid_rows : R01_GRID_SIZE;
                int c = (logic_x - UI_WORLD_GRID_X) / UI_WORLD_CELL;
                int row = (cy - UI_WORLD_GRID_Y) / UI_WORLD_CELL;
                w = cur_world(ui);
                if (c >= 0 && c < gc && row >= 0 && row < gr && w) {
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
                    int x = 36 + b * 22;
                    if (hit(logic_x, cy, x, UI_BG_Y + 12, 20, 12)) {
                        if (r01_project_select_bg_bank(ui->project, b) == b) {
                            ui->bg_bank_tab = b;
                        }
                        return 1;
                    }
                }
            }

            /* sprite banks */
            for (t = 0; t < 4; t++) {
                int x = 36 + t * 22;
                if (hit(logic_x, cy, x, UI_SPR_Y + 12, 20, 12)) {
                    ui->spr_bank_tab = t;
                    ui->project->generate_bank = t;
                    ui->layer = UI_LAYER_SPR;
                    return 1;
                }
            }
            if (hit(logic_x, cy, 132, UI_SPR_Y + 12, 60, 12)) {
                ui->spr_size16 = !ui->spr_size16;
                return 1;
            }
            if (hit(logic_x, cy, 4, UI_SPR_Y + 48, 16 * 8, 8 * 8)) {
                int tx = (logic_x - 4) / 8;
                int ty = (cy - (UI_SPR_Y + 48)) / 8;
                int ti = ty * 16 + tx;
                if (ti >= 0 && ti < R01_TILES_PER_BANK) {
                    ui->spr_tile = ti;
                    w = cur_world(ui);
                    if (w) {
                        r01_spr_ensure_tile(w, ui->spr_bank_tab, ti);
                    }
                    ui->layer = UI_LAYER_SPR;
                    ui->spr_tool = UI_SPR_TOOL_TILE;
                }
                return 1;
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

        if (ui->play.active) {
            return 1;
        }

        /* layer / mode buttons */
        if (hit(logic_x, logic_y, UI_LEFT_W + 52, 2, 28, 12)) {
            ui->layer = UI_LAYER_BG;
            return 1;
        }
        if (hit(logic_x, logic_y, UI_LEFT_W + 82, 2, 40, 12)) {
            ui->layer = UI_LAYER_SPR;
            return 1;
        }
        if (hit(logic_x, logic_y, UI_LEFT_W + 126, 2, 44, 12)) {
            ui->edit_mode = UI_MODE_PIXEL;
            return 1;
        }
        if (hit(logic_x, logic_y, UI_LEFT_W + 174, 2, 36, 12)) {
            ui->edit_mode = UI_MODE_ATTR;
            return 1;
        }
        if (ui->layer == UI_LAYER_SPR) {
            if (hit(logic_x, logic_y, UI_LEFT_W + 200, 2, 40, 12)) {
                ui->spr_tool = UI_SPR_TOOL_PLACE;
                return 1;
            }
            if (hit(logic_x, logic_y, UI_LEFT_W + 244, 2, 40, 12)) {
                ui->spr_tool = UI_SPR_TOOL_TILE;
                return 1;
            }
        }

        if (ui->layer == UI_LAYER_BG && ui->edit_mode == UI_MODE_PIXEL) {
            int c;
            for (c = 0; c < 4; c++) {
                int bx = UI_LEFT_W + 240 + c * 18;
                if (hit(logic_x, logic_y, bx, 2, 16, 12)) {
                    ui->project->paint_color = c;
                    return 1;
                }
            }
        }
        {
            int b;
            for (b = 0; b < 4; b++) {
                int bx = UI_LEFT_W + 418 + b * 14;
                if (hit(logic_x, logic_y, bx, 2, 12, 12)) {
                    if (r01_project_select_bg_bank(ui->project, b) == b) {
                        if (ui->layer == UI_LAYER_SPR) {
                            ui->spr_bank_tab = b;
                        } else {
                            ui->bg_bank_tab = b;
                        }
                    }
                    return 1;
                }
            }
        }

        if (ui->layer == UI_LAYER_SPR && ui->spr_tool == UI_SPR_TOOL_TILE) {
            if (hit(logic_x, logic_y, UI_LEFT_W + 8 + 16, 28 + 16, 8 * 12, 8 * 12)) {
                ui->brush_down = 1;
                paint_spr_tile_at(ui, logic_x, logic_y);
                return 1;
            }
        }

        if (hit(logic_x, logic_y, UI_LEFT_W + 8, 28, R01_SCREEN_PX_W * ui->screen_zoom,
                R01_SCREEN_PX_H * ui->screen_zoom)) {
            if (ui->layer == UI_LAYER_SPR && ui->spr_tool == UI_SPR_TOOL_PLACE) {
                R01Screen *s = r01_project_active_screen(ui->project);
                int px, py, hit_i;
                if (!s) {
                    snprintf(ui->status, sizeof(ui->status), "select a grid screen for oam");
                    return 1;
                }
                screen_to_pixel(ui, logic_x, logic_y, &px, &py);
                hit_i = r01_screen_oam_hit(s, px, py);
                if (hit_i >= 0) {
                    ui->oam_sel = hit_i;
                } else {
                    uint8_t attr = r01_oam_pack(ui->spr_bank_tab, 0, 0, 0, 0, ui->spr_size16);
                    int idx;
                    if (ui->spr_size16) {
                        ui->spr_tile = (int)(ui->spr_tile & ~1);
                    }
                    w = cur_world(ui);
                    if (w) {
                        r01_spr_ensure_tile(w, ui->spr_bank_tab, ui->spr_tile + (ui->spr_size16 ? 1 : 0));
                    }
                    idx = r01_screen_oam_add(s, (uint8_t)px, (uint8_t)py, (uint8_t)ui->spr_tile, attr);
                    ui->oam_sel = idx;
                    ui->project->generate_bank = ui->spr_bank_tab;
                    snprintf(ui->status, sizeof(ui->status), idx >= 0 ? "oam placed" : "oam full");
                }
                return 1;
            }
            if (ui->layer == UI_LAYER_BG && ui->edit_mode == UI_MODE_ATTR) {
                int px, py;
                screen_to_pixel(ui, logic_x, logic_y, &px, &py);
                if (px >= 0 && py >= 0 && px < R01_SCREEN_PX_W && py < R01_SCREEN_PX_H) {
                    ui->attr_tx = px / 8;
                    ui->attr_ty = py / 8;
                }
            } else if (ui->layer == UI_LAYER_BG) {
                ui->brush_down = 1;
                paint_at(ui, logic_x, logic_y);
            }
            return 1;
        }
    }
    return 0;
}
