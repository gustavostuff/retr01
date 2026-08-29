#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/metasprites.h"
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
        if (ui->sprite_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                ui->sprite_edit.open = 0;
                return 1;
            }
            if ((e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_v) {
                (void)ui_paste_clipboard_png_tile(ui, ui->sprite_edit.chr, ui->sprite_edit.pal, 1);
                return 1;
            }
            return 1;
        }
        if (ui->metasprite_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                ui->metasprite_edit.open = 0;
                return 1;
            }
            metasprite_modal_key(ui, e->key.keysym.sym);
            return 1;
        }
        if (ui->entity_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                if (ui->entity_edit.name_focus) {
                    ui->entity_edit.name_focus = 0;
                } else {
                    ui->entity_edit.open = 0;
                }
                return 1;
            }
            entity_modal_key(ui, e->key.keysym.sym);
            return 1;
        }
        if (ui->tile_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                ui->tile_edit.open = 0;
                return 1;
            }
            if ((e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_v) {
                (void)ui_paste_clipboard_png_tile(ui, ui->tile_edit.chr, ui->tile_edit.pal, 0);
                return 1;
            }
            return 1;
        }
        if ((e->key.keysym.sym == SDLK_DELETE || e->key.keysym.sym == SDLK_BACKSPACE) &&
            !ui->play.active && !ui->menu.open && ui->sel_instance >= 0) {
            R01World *w = r01_project_active_world(ui->project);
            if (w && r01_world_instance_remove(w, ui->sel_instance) == 0) {
                ui->sel_instance = -1;
                ui->inst_drag = 0;
                ui_toast(ui, "instance removed", 0);
            }
            return 1;
        }
        if (!ui->play.active && !ui->menu.open && ui->screen_layer == UI_SCREEN_LAYER_SPR &&
            ui->sel_instance >= 0 && (e->key.keysym.sym == SDLK_h || e->key.keysym.sym == SDLK_v)) {
            R01World *w = r01_project_active_world(ui->project);
            if (w && ui->sel_instance < w->instance_count) {
                if (e->key.keysym.sym == SDLK_h) {
                    w->instances[ui->sel_instance].flip_h = !w->instances[ui->sel_instance].flip_h;
                } else {
                    w->instances[ui->sel_instance].flip_v = !w->instances[ui->sel_instance].flip_v;
                }
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

        if (ui->sprite_edit.open) {
            sprite_modal_handle(ui, lx, ly, 1);
            return 1;
        }

        if (ui->metasprite_edit.open) {
            metasprite_modal_handle(ui, lx, ly, 1, e->button.button);
            return 1;
        }

        if (ui->entity_edit.open) {
            entity_modal_handle(ui, lx, ly, 1, e->button.button);
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
            int spr_idx;
            int meta_idx;
            int ent_idx;
            if (ui->metasprite_edit.open) {
                metasprite_modal_handle(ui, lx, ly, 1, SDL_BUTTON_RIGHT);
                return 1;
            }
            if (ui->entity_edit.open) {
                entity_modal_handle(ui, lx, ly, 1, SDL_BUTTON_RIGHT);
                return 1;
            }
            if (sprites_list_hit(ui, lx, ly, &spr_idx)) {
                menu_open_sprite(ui, lx, ly, spr_idx);
                return 1;
            }
            if (metasprites_list_hit(ui, lx, ly, &meta_idx)) {
                menu_open_metasprite(ui, lx, ly, meta_idx);
                return 1;
            }
            if (entities_list_hit(ui, lx, ly, &ent_idx)) {
                menu_open_entity(ui, lx, ly, ent_idx);
                return 1;
            }
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
                int inst;
                if (ui->screen_layer == UI_SCREEN_LAYER_SPR) {
                    if (instance_hit_on_screen(ui, lx, ly, &inst)) {
                        ui->sel_instance = inst;
                        ui->inst_drag = 0;
                        screen_sel_clear(ui);
                        menu_open_instance(ui, lx, ly, inst);
                        return 1;
                    }
                    return 1;
                }
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
                    ui->sel_instance = -1;
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
            if (!ui->play.active && sprites_add_hit(ui, lx, ly)) {
                sprite_edit_open_new(ui);
                return 1;
            }
            if (!ui->play.active && metasprites_add_hit(ui, lx, ly)) {
                metasprite_edit_open_new(ui);
                return 1;
            }
            if (!ui->play.active && entities_add_hit(ui, lx, ly)) {
                entity_edit_open_new(ui);
                return 1;
            }
            {
                int catalog_idx;
                if (!ui->play.active && sprites_list_hit(ui, lx, ly, &catalog_idx)) {
                    ui->catalog_drag.active = UI_CATALOG_DRAG_SPRITE;
                    ui->catalog_drag.index = catalog_idx;
                    ui->catalog_drag.off_x = 4;
                    ui->catalog_drag.off_y = 4;
                    return 1;
                }
                if (!ui->play.active && metasprites_list_hit(ui, lx, ly, &catalog_idx)) {
                    ui->catalog_drag.active = UI_CATALOG_DRAG_METASPRITE;
                    ui->catalog_drag.index = catalog_idx;
                    ui->catalog_drag.off_x = 4;
                    ui->catalog_drag.off_y = 4;
                    return 1;
                }
                if (!ui->play.active && entities_list_hit(ui, lx, ly, &catalog_idx)) {
                    ui->catalog_drag.active = UI_CATALOG_DRAG_ENTITY;
                    ui->catalog_drag.index = catalog_idx;
                    ui->catalog_drag.off_x = 4;
                    ui->catalog_drag.off_y = 4;
                    return 1;
                }
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
            if (!ui->play.active && screen_layer_hit(ui, lx, ly, &mode_row)) {
                ui->screen_layer = mode_row;
                if (ui->screen_layer == UI_SCREEN_LAYER_BG) {
                    ui->sel_instance = -1;
                    ui->inst_drag = 0;
                } else {
                    screen_sel_clear(ui);
                }
                return 1;
            }
            if (!ui->play.active && screen_mode_hit(ui, lx, ly, &mode_row)) {
                if (ui->screen_layer != UI_SCREEN_LAYER_BG) {
                    return 1;
                }
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
                int inst;
                if (ui->screen_layer == UI_SCREEN_LAYER_SPR) {
                    int px, py;
                    if (instance_hit_on_screen(ui, lx, ly, &inst) && screen_pixel_hit(ui, lx, ly, &px, &py)) {
                        R01World *w = r01_project_active_world(ui->project);
                        R01Screen *s = r01_project_active_screen(ui->project);
                        ui->sel_instance = inst;
                        screen_sel_clear(ui);
                        if (w && s && inst >= 0 && inst < w->instance_count) {
                            ui->inst_drag = 1;
                            ui->inst_drag_off_x = w->instances[inst].world_x - (s->col * R01_SCREEN_PX_W + px);
                            ui->inst_drag_off_y = w->instances[inst].world_y - (s->row * R01_SCREEN_PX_H + py);
                        }
                    } else {
                        ui->sel_instance = -1;
                        ui->inst_drag = 0;
                    }
                    return 1;
                }
                ui->sel_instance = -1;
                ui->inst_drag = 0;
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
        ui->inst_drag = 0;
        if (ui->metasprite_edit.open) {
            metasprite_modal_handle(ui, lx, ly, 0, e->button.button);
            return 1;
        }
        if (ui->entity_edit.open) {
            entity_modal_handle(ui, lx, ly, 0, e->button.button);
            return 1;
        }
        if (ui->catalog_drag.active) {
            int px, py;
            R01World *w = r01_project_active_world(ui->project);
            R01Screen *s = r01_project_active_screen(ui->project);
            if (w && s && !ui->play.active && screen_pixel_hit(ui, lx, ly, &px, &py)) {
                int wx = s->col * R01_SCREEN_PX_W + px;
                int wy = s->row * R01_SCREEN_PX_H + py;
                int idx = -1;
                if (ui->catalog_drag.active == UI_CATALOG_DRAG_SPRITE) {
                    idx = r01_world_place_sprite(w, ui->catalog_drag.index, wx, wy);
                    if (idx >= 0) {
                        ui_toast(ui, "sprite placed", 0);
                    } else {
                        ui_toast(ui, "cannot place sprite", 1);
                    }
                } else if (ui->catalog_drag.active == UI_CATALOG_DRAG_METASPRITE) {
                    idx = r01_world_place_metasprite(w, ui->catalog_drag.index, wx, wy);
                    if (idx >= 0) {
                        ui_toast(ui, "metasprite placed", 0);
                    } else {
                        ui_toast(ui, "cannot place metasprite", 1);
                    }
                } else if (ui->catalog_drag.active == UI_CATALOG_DRAG_ENTITY) {
                    idx = r01_world_place_entity(w, ui->catalog_drag.index, wx, wy);
                    if (idx >= 0) {
                        ui_toast(ui, "entity placed", 0);
                    } else {
                        ui_toast(ui, "cannot place entity", 1);
                    }
                }
                if (idx >= 0) {
                    ui->sel_instance = idx;
                    ui->screen_layer = UI_SCREEN_LAYER_SPR;
                    screen_sel_clear(ui);
                }
            }
            ui->catalog_drag.active = 0;
            return 1;
        }
    }

    if (e->type == SDL_MOUSEMOTION && !ui->play.active && !ui->tile_edit.open && !ui->sprite_edit.open &&
        !ui->metasprite_edit.open && !ui->entity_edit.open && !ui->menu.open && !ui->catalog_drag.active) {
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        int tx, ty;
        if (ui->screen_layer == UI_SCREEN_LAYER_SPR && ui->inst_drag && ui->sel_instance >= 0 &&
            (e->motion.state & SDL_BUTTON_LMASK)) {
            int px, py;
            R01World *w = r01_project_active_world(ui->project);
            R01Screen *s = r01_project_active_screen(ui->project);
            if (w && s && ui->sel_instance < w->instance_count && screen_pixel_hit(ui, lx, ly, &px, &py)) {
                w->instances[ui->sel_instance].world_x = s->col * R01_SCREEN_PX_W + px + ui->inst_drag_off_x;
                w->instances[ui->sel_instance].world_y = s->row * R01_SCREEN_PX_H + py + ui->inst_drag_off_y;
            }
            return 1;
        }
        if (ui->screen_layer == UI_SCREEN_LAYER_BG && ui->screen_mode == UI_SCREEN_MODE_SEL && ui->sel_drag &&
            shift && (e->motion.state & SDL_BUTTON_LMASK) && screen_hit(ui, lx, ly, &tx, &ty)) {
            screen_sel_set(ui, ui->sel_anchor_x, ui->sel_anchor_y, tx, ty);
            return 1;
        }
        if (ui->screen_layer == UI_SCREEN_LAYER_BG && ui->screen_mode == UI_SCREEN_MODE_PAINT &&
            (e->motion.state & SDL_BUTTON_LMASK) &&
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
    if (e->type == SDL_MOUSEMOTION && ui->sprite_edit.open && (e->motion.state & SDL_BUTTON_LMASK)) {
        sprite_modal_handle(ui, lx, ly, 1);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->metasprite_edit.open) {
        if (e->motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)) {
            metasprite_modal_drag(ui, lx, ly, e->motion.state);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->entity_edit.open) {
        if (e->motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)) {
            entity_modal_drag(ui, lx, ly, e->motion.state);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEWHEEL && ui->entity_edit.open) {
        EntityModalLayout lo;
        const R01World *w = r01_project_active_world_const(ui->project);
        entity_modal_layout(&lo);
        if (point_in_rect(lx, ly, lo.left_list_x, lo.left_list_y, lo.right_grid_x - lo.left_list_x - UI_UNIT,
                          lo.left_list_h)) {
            int vis = lo.left_list_h / UI_SPRITE_ROW_H;
            int max_scroll = 0;
            if (w && w->metasprite_count > vis) {
                max_scroll = w->metasprite_count - vis;
            }
            ui->entity_edit.meta_scroll -= e->wheel.y;
            if (ui->entity_edit.meta_scroll < 0) {
                ui->entity_edit.meta_scroll = 0;
            }
            if (ui->entity_edit.meta_scroll > max_scroll) {
                ui->entity_edit.meta_scroll = max_scroll;
            }
            return 1;
        }
    }
    if (e->type == SDL_MOUSEWHEEL && !ui->pal_edit.open && !ui->tile_edit.open && !ui->sprite_edit.open &&
        !ui->metasprite_edit.open && !ui->entity_edit.open && !ui->play.active) {
        AccordionLayout lo;
        accordion_layout(ui, &lo);
        if (lo.sprites_open && lx < UI_SIDEBAR_W) {
            const R01World *w = r01_project_active_world_const(ui->project);
            int vis = (UI_SPRITES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
            int max_scroll = 0;
            if (w && w->sprite_count > vis) {
                max_scroll = w->sprite_count - vis;
            }
            ui->sprites_scroll -= e->wheel.y;
            if (ui->sprites_scroll < 0) {
                ui->sprites_scroll = 0;
            }
            if (ui->sprites_scroll > max_scroll) {
                ui->sprites_scroll = max_scroll;
            }
            return 1;
        }
        if (lo.metasprites_open && lx < UI_SIDEBAR_W) {
            const R01World *w = r01_project_active_world_const(ui->project);
            int vis = (UI_METASPRITES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
            int max_scroll = 0;
            if (w && w->metasprite_count > vis) {
                max_scroll = w->metasprite_count - vis;
            }
            ui->metasprites_scroll -= e->wheel.y;
            if (ui->metasprites_scroll < 0) {
                ui->metasprites_scroll = 0;
            }
            if (ui->metasprites_scroll > max_scroll) {
                ui->metasprites_scroll = max_scroll;
            }
            return 1;
        }
        if (lo.entities_open && lx < UI_SIDEBAR_W) {
            const R01World *w = r01_project_active_world_const(ui->project);
            int vis = (UI_ENTITIES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
            int max_scroll = 0;
            if (w && w->entity_count > vis) {
                max_scroll = w->entity_count - vis;
            }
            ui->entities_scroll -= e->wheel.y;
            if (ui->entities_scroll < 0) {
                ui->entities_scroll = 0;
            }
            if (ui->entities_scroll > max_scroll) {
                ui->entities_scroll = max_scroll;
            }
            return 1;
        }
    }
    return 0;
}
