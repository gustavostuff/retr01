#include "ui.h"
#include "font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ui_toast(UiState *ui, const char *msg, int is_error) {
    if (!ui || !msg) {
        return;
    }
    snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
    ui->toast_error = is_error;
    ui->toast_until = SDL_GetTicks() + UI_TOAST_MS;
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

static int world_cell_size(const R01World *w) {
    int max_w;
    int max_h;
    int cs;
    if (!w || w->grid_cols < 1 || w->grid_rows < 1) {
        return UI_WORLD_CELL;
    }
    max_w = (UI_LEFT_W - UI_WORLD_GRID_X - 8) / w->grid_cols;
    max_h = (UI_LOGIC_H - UI_WORLD_GRID_Y - 24) / w->grid_rows;
    cs = max_w < max_h ? max_w : max_h;
    if (cs > UI_WORLD_CELL) {
        cs = UI_WORLD_CELL;
    }
    if (cs < 8) {
        cs = 8;
    }
    return cs;
}

static int world_cell_at(const R01World *w, int lx, int ly, int *out_idx) {
    int col, row, cs, idx;
    int gx = UI_WORLD_GRID_X;
    int gy = UI_WORLD_GRID_Y;
    if (!w || lx < gx || ly < gy) {
        return 0;
    }
    cs = world_cell_size(w);
    col = (lx - gx) / cs;
    row = (ly - gy) / cs;
    if (col < 0 || row < 0 || col >= w->grid_cols || row >= w->grid_rows) {
        return 0;
    }
    idx = r01_world_screen_index(w, col, row);
    if (idx < 0 || !w->screens[idx].present) {
        return 0;
    }
    if (out_idx) {
        *out_idx = idx;
    }
    return 1;
}

static int play_button_hit(int lx, int ly) {
    return lx >= UI_PLAY_BTN_X && lx < UI_PLAY_BTN_X + UI_PLAY_BTN_W && ly >= UI_PLAY_BTN_Y &&
           ly < UI_PLAY_BTN_Y + UI_PLAY_BTN_H;
}

static void draw_play_button(const UiState *ui, SDL_Renderer *r) {
    int x = UI_PLAY_BTN_X;
    int y = UI_PLAY_BTN_Y;
    if (ui->play.active) {
        fill_rect(r, x, y, UI_PLAY_BTN_W, UI_PLAY_BTN_H, 120, 45, 45);
        font_draw(r, x + 16, y + 3, "STOP", 240, 240, 240);
    } else {
        fill_rect(r, x, y, UI_PLAY_BTN_W, UI_PLAY_BTN_H, 35, 110, 55);
        font_draw(r, x + 16, y + 3, "PLAY", 240, 240, 240);
    }
    draw_rect(r, x, y, UI_PLAY_BTN_W, UI_PLAY_BTN_H, ui->play.active ? 180 : 80, ui->play.active ? 80 : 160,
              ui->play.active ? 80 : 90);
}

static void ui_toggle_play(UiState *ui) {
    if (ui->play.active) {
        r01_play_stop(&ui->play);
        snprintf(ui->status, sizeof(ui->status), "edit mode — drop PNG or click PLAY");
    } else if (!r01_play_start(&ui->play, ui->project)) {
        ui_toast(ui, "no screens — drop PNG first", 1);
    } else {
        ui->play_last_tick = SDL_GetTicks();
        snprintf(ui->status, sizeof(ui->status), "play — WASD/arrows move  X/Y warp");
    }
}

static void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int scale = 2;
    int ox = UI_SCREEN_X + (UI_SCREEN_VIEW_W - R01_SCREEN_PX_W * scale) / 2;
    int oy = UI_SCREEN_Y + (UI_SCREEN_VIEW_H - R01_SCREEN_PX_H * scale) / 2;
    int y, x;
    if (!s) {
        font_draw(r, UI_SCREEN_X + 8, UI_SCREEN_Y + 8, "NO SCREEN — DROP PNG", 140, 140, 150);
        return;
    }
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t cr, cg, cb;
            SDL_Rect px;
            r01_screen_pixel_rgb(ui->project, r01_project_world0(ui->project), s, x, y, &cr, &cg, &cb);
            px.x = ox + x * scale;
            px.y = oy + y * scale;
            px.w = scale;
            px.h = scale;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    draw_rect(r, ox - 1, oy - 1, R01_SCREEN_PX_W * scale + 2, R01_SCREEN_PX_H * scale + 2, 80, 90, 100);
}

static void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int ox = UI_SCREEN_X + (UI_SCREEN_VIEW_W - R01_SCREEN_PX_W * 2) / 2;
    int oy = UI_SCREEN_Y + (UI_SCREEN_VIEW_H - R01_SCREEN_PX_H * 2) / 2;
    int scale = 2;
    int vy, vx;
    for (vy = 0; vy < R01_SCREEN_PX_H; vy++) {
        for (vx = 0; vx < R01_SCREEN_PX_W; vx++) {
            uint8_t cr = 0, cg = 0, cb = 0;
            SDL_Rect px;
            r01_play_sample_bg(ui->project, &ui->play, vx, vy, &cr, &cg, &cb);
            px.x = ox + vx * scale;
            px.y = oy + vy * scale;
            px.w = scale;
            px.h = scale;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    {
        int pcx, pcy;
        uint8_t pr, pg, pb;
        r01_project_player_rgb(ui->project, &pr, &pg, &pb);
        for (pcy = 0; pcy < R01_PLAY_PLAYER_SIZE; pcy++) {
            for (pcx = 0; pcx < R01_PLAY_PLAYER_SIZE; pcx++) {
                int wx = ui->play.player_x + pcx;
                int wy = ui->play.player_y + pcy;
                int vx = wx - ui->play.cam_x;
                int vy = wy - ui->play.cam_y;
                SDL_Rect px;
                if (vx < 0 || vy < 0 || vx >= R01_SCREEN_PX_W || vy >= R01_SCREEN_PX_H) {
                    continue;
                }
                px.x = ox + vx * scale;
                px.y = oy + vy * scale;
                px.w = scale;
                px.h = scale;
                SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
                SDL_RenderFillRect(r, &px);
            }
        }
    }
    draw_rect(r, ox - 1, oy - 1, R01_SCREEN_PX_W * scale + 2, R01_SCREEN_PX_H * scale + 2, 60, 180, 80);
}

int ui_init(UiState *ui) {
    if (!ui) {
        return -1;
    }
    memset(ui, 0, sizeof(*ui));
    ui->project = (R01Project *)calloc(1, sizeof(R01Project));
    if (!ui->project) {
        return -1;
    }
    snprintf(ui->project_path, sizeof(ui->project_path), "%s", R01_DEFAULT_PROJECT);
    {
        char err[128];
        if (r01_project_load_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
            r01_project_init(ui->project, "test");
        } else {
            r01_chr_pack_world_bank0(r01_project_world0(ui->project));
        }
    }
    snprintf(ui->status, sizeof(ui->status), "DROP PNG — CTRL+S SAVE — CTRL+E EXPORT");
    return 0;
}

void ui_shutdown(UiState *ui) {
    if (!ui) {
        return;
    }
    free(ui->project);
    ui->project = NULL;
}

void ui_tick(UiState *ui) {
    int dx = 0, dy = 0;
    if (!ui || !ui->play.active) {
        return;
    }
    Uint32 now = SDL_GetTicks();
    if (now - ui->play_last_tick < 16u) {
        return;
    }
    ui->play_last_tick = now;
    if (ui->keys[SDL_SCANCODE_W] || ui->keys[SDL_SCANCODE_UP]) {
        dy = -1;
    }
    if (ui->keys[SDL_SCANCODE_S] || ui->keys[SDL_SCANCODE_DOWN]) {
        dy = 1;
    }
    if (ui->keys[SDL_SCANCODE_A] || ui->keys[SDL_SCANCODE_LEFT]) {
        dx = -1;
    }
    if (ui->keys[SDL_SCANCODE_D] || ui->keys[SDL_SCANCODE_RIGHT]) {
        dx = 1;
    }
    r01_play_tick(&ui->play, ui->project, dx, dy);
}

void ui_draw(UiState *ui, SDL_Renderer *r) {
    R01World *w = r01_project_world0(ui->project);
    int col, row, idx, cs;
    char grid_label[32];
    if (!ui || !ui->project || !w) {
        return;
    }
    cs = world_cell_size(w);
    SDL_SetRenderDrawColor(r, 18, 20, 24, 255);
    SDL_RenderClear(r);
    fill_rect(r, 0, 0, UI_LOGIC_W, 20, 12, 14, 18);
    font_draw(r, 8, 6, "RETR01 STUDIO — SMOOTH + EAGLE VIEW", 200, 210, 220);
    draw_play_button(ui, r);

    fill_rect(r, 0, 20, UI_LEFT_W, UI_LOGIC_H - 20, 24, 26, 30);
    font_draw(r, 8, 28, "WORLD 0", 160, 170, 180);
    snprintf(grid_label, sizeof(grid_label), "%dX%d GRID", w->grid_cols, w->grid_rows);
    font_draw(r, 8, 38, grid_label, 100, 110, 120);

    for (row = 0; row < w->grid_rows; row++) {
        for (col = 0; col < w->grid_cols; col++) {
            int x = UI_WORLD_GRID_X + col * cs;
            int y = UI_WORLD_GRID_Y + row * cs;
            idx = r01_world_screen_index(w, col, row);
            if (idx < 0 || !w->screens[idx].present) {
                continue;
            }
            if (idx == ui->project->active_screen) {
                fill_rect(r, x, y, cs - 2, cs - 2, 40, 70, 50);
            } else {
                fill_rect(r, x, y, cs - 2, cs - 2, 32, 36, 42);
            }
            draw_rect(r, x, y, cs - 2, cs - 2, 70, 80, 90);
        }
    }

    fill_rect(r, UI_LEFT_W, 20, UI_LOGIC_W - UI_LEFT_W, UI_LOGIC_H - 20, 28, 30, 34);
    if (ui->play.active) {
        font_draw(r, UI_SCREEN_X, 28, "PLAY", 80, 200, 100);
        draw_play_view(ui, r);
    } else {
        R01Screen *s = r01_project_active_screen(ui->project);
        char label[32];
        if (s) {
            snprintf(label, sizeof(label), "SCREEN %d,%d", s->col, s->row);
        } else {
            snprintf(label, sizeof(label), "SCREEN");
        }
        font_draw(r, UI_SCREEN_X, 28, label, 160, 170, 180);
        draw_screen_editor(ui, r, s);
    }

    font_draw(r, 8, UI_LOGIC_H - 14, ui->status, 120, 130, 140);
    if (ui->toast_until > SDL_GetTicks() && ui->toast[0]) {
        int ty = UI_LOGIC_H - 28;
        fill_rect(r, 8, ty - 2, UI_LOGIC_W - 16, 14, ui->toast_error ? 60 : 30, ui->toast_error ? 24 : 36,
                  ui->toast_error ? 24 : 42);
        font_draw(r, 12, ty, ui->toast, 240, 240, 240);
    }
    (void)w;
}

static void ui_save(UiState *ui) {
    char err[128];
    if (r01_project_save_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    ui_toast(ui, R01_DEFAULT_PROJECT " saved", 0);
}

static void ui_export(UiState *ui) {
    char err[128];
    if (r01_export_bundle(ui->project, R01_DEFAULT_CART_STEM, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    ui_toast(ui, R01_DEFAULT_CART_STEM ".retr01 exported", 0);
}

int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly) {
    char err[128];
    R01World *w;
    (void)lx;
    (void)ly;
    if (!ui || !path) {
        return 0;
    }
    if (r01_project_import_png(ui->project, path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return 1;
    }
    w = r01_project_world0(ui->project);
    if (!w) {
        return 0;
    }
    {
        int idx = r01_world_screen_index(w, R01_START_COL, R01_START_ROW);
        if (idx >= 0 && w->screens[idx].present) {
            ui->project->active_screen = idx;
        } else {
            int i;
            ui->project->active_screen = 0;
            for (i = 0; i < w->screen_count; i++) {
                if (w->screens[i].present) {
                    ui->project->active_screen = i;
                    break;
                }
            }
        }
    }
    ui_toast(ui, "png imported", 0);
    snprintf(ui->status, sizeof(ui->status), "PNG OK — %dx%d grid, %d tiles", w->grid_cols, w->grid_rows,
             w->bg_banks[0].tile_count);
    return 1;
}

int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly) {
    if (!ui) {
        return 0;
    }
    if (e->type == SDL_KEYDOWN) {
        ui->keys[e->key.keysym.scancode] = 1;
        if (e->key.keysym.mod & KMOD_CTRL) {
            if (e->key.keysym.sym == SDLK_s) {
                ui_save(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_e) {
                ui_export(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_o) {
                char err[128];
                if (r01_project_load_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
                    ui_toast(ui, err, 1);
                } else {
                    r01_chr_pack_world_bank0(r01_project_world0(ui->project));
                    ui_toast(ui, "loaded", 0);
                }
                return 1;
            }
            if (e->key.keysym.sym == SDLK_f) {
                return 2;
            }
        }
        if (e->key.keysym.sym == SDLK_SPACE) {
            ui_toggle_play(ui);
            return 1;
        }
        if (ui->play.active) {
            if (e->key.keysym.sym == SDLK_x) {
                r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_X);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_y) {
                r01_play_button(&ui->play, ui->project, R01_PLAY_BTN_Y);
                return 1;
            }
        }
    }
    if (e->type == SDL_KEYUP) {
        ui->keys[e->key.keysym.scancode] = 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (play_button_hit(lx, ly)) {
            ui_toggle_play(ui);
            return 1;
        }
        if (!ui->play.active) {
            int idx;
            R01World *w = r01_project_world0(ui->project);
            if (w && world_cell_at(w, lx, ly, &idx)) {
                ui->project->active_screen = idx;
                return 1;
            }
        }
    }
    return 0;
}
