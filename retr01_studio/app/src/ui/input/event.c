#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
