#include "ui.h"
#include "ui_internal.h"
#include "font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int uri_hex(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void uri_decode_inplace(char *s) {
    char *r = s;
    char *w = s;
    while (*r) {
        if (r[0] == '%' && r[1] && r[2]) {
            int hi = uri_hex((unsigned char)r[1]);
            int lo = uri_hex((unsigned char)r[2]);
            if (hi >= 0 && lo >= 0) {
                *w++ = (char)(hi * 16 + lo);
                r += 3;
                continue;
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static void normalize_drop_path(char *path) {
    char *p;
    size_t n;
    if (!path || !path[0]) {
        return;
    }
    n = strlen(path);
    while (n > 0 && (path[n - 1] == '\r' || path[n - 1] == '\n' || path[n - 1] == ' ')) {
        path[--n] = '\0';
    }
    if (strncmp(path, "file://", 7) == 0) {
        p = path + 7;
        if (*p == '/') {
            memmove(path, p, strlen(p) + 1u);
        } else {
            char *slash = strchr(p, '/');
            if (slash) {
                memmove(path, slash, strlen(slash) + 1u);
            } else {
                memmove(path, p, strlen(p) + 1u);
            }
        }
    } else if (strncmp(path, "file:/", 6) == 0) {
        memmove(path, path + 5, strlen(path + 5) + 1u);
    }
    uri_decode_inplace(path);
}

static void resolve_drop_path(const char *in, char *out, size_t out_cap) {
    char tmp[R01_PATH_MAX];
    char resolved[R01_PATH_MAX];
    if (!out || out_cap < 1) {
        return;
    }
    snprintf(tmp, sizeof(tmp), "%s", in ? in : "");
    normalize_drop_path(tmp);
    if (realpath(tmp, resolved)) {
        snprintf(out, out_cap, "%s", resolved);
        return;
    }
    snprintf(out, out_cap, "%s", tmp);
}

static int path_ends_with_ci(const char *path, const char *suffix) {
    size_t pl, sl;
    const char *p;
    if (!path || !suffix) {
        return 0;
    }
    pl = strlen(path);
    sl = strlen(suffix);
    if (pl < sl) {
        return 0;
    }
    p = path + pl - sl;
    while (*suffix) {
        char a = *p++;
        char b = *suffix++;
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

void ui_reset_after_project_load(UiState *ui) {
    if (!ui) {
        return;
    }
    if (ui->play.active) {
        r01_play_stop(&ui->play);
    }
    ui->tile_edit.open = 0;
    ui->pal_edit.open = 0;
    ui->menu.open = 0;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    screen_sel_clear(ui);
    ui->paint_stamp_valid = 0;
    ui->last_paint_tx = -1;
    ui->last_paint_ty = -1;
}

void ui_save(UiState *ui) {
    char err[128];
    if (!ui->project_path[0]) {
        ui_toast(ui, "drop a .r01proj to save", 1);
        return;
    }
    if (r01_project_save_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s saved", ui->project_path);
        ui_toast(ui, msg, 0);
    }
}

void ui_export(UiState *ui) {
    char err[128];
    if (r01_export_bundle(ui->project, R01_DEFAULT_CART_STEM, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    ui_toast(ui, R01_DEFAULT_CART_STEM ".retr01 exported", 0);
}

int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly) {
    char err[128];
    char local[R01_PATH_MAX];
    (void)lx;
    (void)ly;
    if (!ui || !path) {
        return 0;
    }
    resolve_drop_path(path, local, sizeof(local));
    if (path_ends_with_ci(local, ".r01proj") || path_ends_with_ci(local, ".json")) {
        if (r01_project_load_json(ui->project, local, err, sizeof(err)) != 0) {
            ui_toast(ui, err, 1);
            return 1;
        }
        snprintf(ui->project_path, sizeof(ui->project_path), "%s", local);
        ui_reset_after_project_load(ui);
        ui_toast(ui, "project loaded", 0);
        return 1;
    }
    if (path_ends_with_ci(local, ".png")) {
        if (r01_project_import_png(ui->project, local, err, sizeof(err)) != 0) {
            ui_toast(ui, err, 1);
            return 1;
        }
        r01_project_select_start_screen(ui->project);
        ui_toast(ui, "png imported", 0);
        return 1;
    }
    ui_toast(ui, "drop a .r01proj or .png", 1);
    return 1;
}

int ui_screen_nav(UiState *ui, int dcol, int drow) {
    R01World *w;
    R01Screen *cur;
    int ncol, nrow, idx;
    if (!ui || ui->play.active || ui->pal_edit.open || ui->tile_edit.open || ui->menu.open) {
        return 0;
    }
    w = r01_project_active_world(ui->project);
    cur = r01_project_active_screen(ui->project);
    if (!w || !cur) {
        return 0;
    }
    ncol = cur->col + dcol;
    nrow = cur->row + drow;
    idx = r01_world_find_screen(w, ncol, nrow);
    if (idx < 0) {
        return 0;
    }
    ui->project->active_screen = idx;
    screen_sel_clear(ui);
    return 1;
}

void handle_world_click(UiState *ui, int col, int row, int ctrl, int dbl) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    if (!w) {
        return;
    }
    if (ctrl) {
        r01_world_remove_screen(w, col, row);
        if (ui->project->active_screen == r01_world_screen_index(w, col, row)) {
            r01_project_select_start_screen(ui->project);
        }
        return;
    }
    if (dbl) {
        idx = r01_world_create_screen(w, col, row);
        if (idx >= 0) {
            ui->project->active_screen = idx;
        }
        return;
    }
    idx = r01_world_find_screen(w, col, row);
    if (idx >= 0) {
        ui->project->active_screen = idx;
    }
}

int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly) {
    if (!ui) {
        return 0;
    }
    if (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        ui->mouse_x = lx;
        ui->mouse_y = ly;
    }
    if (e->type == SDL_MOUSEMOTION) {
        ui_update_cursor(ui);
        if (ui->menu.open) {
            menu_update_hover(ui, lx, ly);
        }
    }
    if (e->type == SDL_MOUSEWHEEL && ui->pal_edit.open) {
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        pal_edit_nudge_master(ui, e->wheel.y, shift);
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        ui->keys[e->key.keysym.scancode] = 1;
        if (ui->pal_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                pal_edit_cancel(ui);
                return 1;
            }
            return 1;
        }
        if (ui->tile_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                ui->tile_edit.open = 0;
                return 1;
            }
            return 1;
        }
        if ((e->key.keysym.mod & KMOD_SHIFT) && !(e->key.keysym.mod & KMOD_CTRL)) {
            if (e->key.keysym.sym == SDLK_LEFT && ui_screen_nav(ui, -1, 0)) {
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHT && ui_screen_nav(ui, 1, 0)) {
                return 1;
            }
            if (e->key.keysym.sym == SDLK_UP && ui_screen_nav(ui, 0, -1)) {
                return 1;
            }
            if (e->key.keysym.sym == SDLK_DOWN && ui_screen_nav(ui, 0, 1)) {
                return 1;
            }
        }
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
                if (!ui->project_path[0]) {
                    ui_toast(ui, "drop a .r01proj to reload", 1);
                    return 1;
                }
                if (r01_project_load_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
                    ui_toast(ui, err, 1);
                } else {
                    ui_reset_after_project_load(ui);
                    ui_toast(ui, "loaded", 0);
                }
                return 1;
            }
            if (e->key.keysym.sym == SDLK_f) {
                return 2;
            }
        }
        if (e->key.keysym.sym == SDLK_ESCAPE && ui->menu.open) {
            if (ui->menu.submenu != UI_MENU_SUB_NONE) {
                ui->menu.submenu = UI_MENU_SUB_NONE;
            } else {
                menu_close(ui);
            }
            return 1;
        }
        if (!ui->play.active && !ui->menu.open && ui->screen_mode == UI_SCREEN_MODE_SEL && screen_sel_valid(ui)) {
            R01Screen *s = r01_project_active_screen(ui->project);
            int min_x, min_y, max_x, max_y, ty, tx;
            if (s) {
                if (e->key.keysym.sym == SDLK_h || e->key.keysym.sym == SDLK_v) {
                    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
                    for (ty = min_y; ty <= max_y; ty++) {
                        for (tx = min_x; tx <= max_x; tx++) {
                            int cell = ty * R01_SCREEN_TILES_X + tx;
                            uint8_t old = s->attrs[cell];
                            if (e->key.keysym.sym == SDLK_h) {
                                s->attrs[cell] = r01_attr_merge(old, r01_attr_bank(old), r01_attr_pal(old),
                                                                !r01_attr_flip_h(old), r01_attr_flip_v(old));
                            } else {
                                s->attrs[cell] = r01_attr_merge(old, r01_attr_bank(old), r01_attr_pal(old),
                                                                r01_attr_flip_h(old), !r01_attr_flip_v(old));
                            }
                        }
                    }
                    screen_refresh_sel(ui);
                    return 1;
                }
                if (e->key.keysym.sym >= SDLK_1 && e->key.keysym.sym <= SDLK_4) {
                    int pal = (int)(e->key.keysym.sym - SDLK_1);
                    screen_set_sel_pal(ui, pal);
                    return 1;
                }
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

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;

        if (ui->pal_edit.open) {
            int prow;
            if (palette_row_btn_hit(ui, lx, ly, &prow)) {
                ui->pal_edit.row = prow;
                return 1;
            }
            pal_modal_handle(ui, lx, ly, 1);
            return 1;
        }

        if (ui->tile_edit.open) {
            tile_modal_handle(ui, lx, ly, 1);
            return 1;
        }

        if (ui->menu.open) {
            int item, is_sub;
            if (menu_hit(ui, lx, ly, &item, &is_sub)) {
                if (!is_sub && item >= 0 && item < ui->menu.item_count && ui->menu.item_disabled[item]) {
                    return 1;
                }
                handle_menu_pick(ui, item, is_sub);
            } else {
                menu_close(ui);
            }
            return 1;
        }

        if (e->button.button == SDL_BUTTON_RIGHT && !ui->play.active) {
            int col, row;
            if (world_cell_hit(ui, lx, ly, &col, &row)) {
                R01World *w = r01_project_active_world(ui->project);
                int idx = w ? r01_world_screen_index(w, col, row) : -1;
                if (idx >= 0 && w->screens[idx].present) {
                    ui->project->active_screen = idx;
                    menu_open_world_cell(ui, lx, ly, idx);
                    return 1;
                }
            }
            {
                int tx, ty;
                if (screen_hit(ui, lx, ly, &tx, &ty) && r01_project_active_screen(ui->project)) {
                    int min_x, min_y, max_x, max_y;
                    int in_sel = 0;
                    if (screen_sel_valid(ui)) {
                        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
                        in_sel = tx >= min_x && tx <= max_x && ty >= min_y && ty <= max_y;
                    }
                    if (!in_sel) {
                        screen_sel_set(ui, tx, ty, tx, ty);
                    }
                    menu_open_tile(ui, lx, ly, tx, ty);
                    return 1;
                }
            }
        }

        if (e->button.button == SDL_BUTTON_LEFT) {
            int wi, col, row, tx, ty, acc_sec, mode_row;
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            int alt = (SDL_GetModState() & KMOD_ALT) != 0;
            int flood = ui->keys[SDL_SCANCODE_F] != 0;

            if (play_button_hit(ui, lx, ly)) {
                ui_toggle_play(ui);
                return 1;
            }
            if (lx < UI_SIDEBAR_W && accordion_header_hit(ui, lx, ly, &acc_sec)) {
                accordion_toggle(ui, acc_sec);
                return 1;
            }
            if (!ui->play.active && palette_strip_hit(ui, lx, ly)) {
                int prow;
                if (palette_row_btn_hit(ui, lx, ly, &prow)) {
                    if (ui->pal_edit.open) {
                        ui->pal_edit.row = prow;
                    } else {
                        pal_edit_set_row(ui, prow, 1);
                    }
                } else if (!ui->pal_edit.open) {
                    pal_edit_open(ui);
                }
                return 1;
            }
            if (world_btn_hit(ui, lx, ly, &wi)) {
                r01_project_set_active_world(ui->project, wi);
                return 1;
            }
            if (!ui->play.active && world_cell_hit(ui, lx, ly, &col, &row)) {
                Uint32 now = SDL_GetTicks();
                int dbl = (col == ui->last_click_col && row == ui->last_click_row &&
                           now - ui->last_click_ms < 350u);
                handle_world_click(ui, col, row, ctrl, dbl);
                ui->last_click_ms = now;
                ui->last_click_col = col;
                ui->last_click_row = row;
                return 1;
            }
            if (!ui->play.active && screen_mode_hit(ui, lx, ly, &mode_row)) {
                ui->screen_mode = mode_row;
                if (mode_row == UI_SCREEN_MODE_PAINT && !ui->paint_stamp_valid) {
                    uint8_t tile, attr;
                    if (ui_paint_stamp_from_sel(ui, &tile, &attr)) {
                        ui_paint_stamp_set(ui, tile, attr);
                    }
                }
                return 1;
            }
            if (!ui->play.active && screen_hit(ui, lx, ly, &tx, &ty)) {
                if (ui->screen_mode == UI_SCREEN_MODE_PAINT) {
                    if (alt) {
                        ui_paint_stamp_from_cell(ui, tx, ty);
                        ui_toast(ui, "stamp picked", 0);
                        return 1;
                    }
                    if (flood) {
                        ui_flood_fill(ui, tx, ty);
                        return 1;
                    }
                    ui->last_paint_tx = -1;
                    ui->last_paint_ty = -1;
                    ui_paint_tile(ui, tx, ty);
                    return 1;
                }
                if (shift) {
                    ui->sel_drag = 1;
                    ui->sel_anchor_x = tx;
                    ui->sel_anchor_y = ty;
                    screen_sel_set(ui, tx, ty, tx, ty);
                    return 1;
                }
                screen_sel_set(ui, tx, ty, tx, ty);
                ui_paint_stamp_from_cell(ui, tx, ty);
                return 1;
            }
        }
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ui->last_paint_tx = -1;
        ui->last_paint_ty = -1;
        ui->sel_drag = 0;
    }

    if (e->type == SDL_MOUSEMOTION && !ui->play.active && !ui->tile_edit.open && !ui->menu.open) {
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        int tx, ty;
        if (ui->screen_mode == UI_SCREEN_MODE_SEL && ui->sel_drag && shift &&
            (e->motion.state & SDL_BUTTON_LMASK) && screen_hit(ui, lx, ly, &tx, &ty)) {
            screen_sel_set(ui, ui->sel_anchor_x, ui->sel_anchor_y, tx, ty);
            return 1;
        }
        if (ui->screen_mode == UI_SCREEN_MODE_PAINT && (e->motion.state & SDL_BUTTON_LMASK) &&
            !(SDL_GetModState() & KMOD_ALT) && ui->keys[SDL_SCANCODE_F] == 0) {
            if (screen_hit(ui, lx, ly, &tx, &ty)) {
                ui_paint_tile(ui, tx, ty);
                return 1;
            }
        }
    }

    if (e->type == SDL_MOUSEMOTION && ui->tile_edit.open && (e->motion.state & SDL_BUTTON_LMASK)) {
        tile_modal_handle(ui, lx, ly, 1);
        return 1;
    }
    return 0;
}
