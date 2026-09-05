#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/metatiles.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_emu/play.h"
#include "retr01_emu/video.h"

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
    if (e->type == SDL_MOUSEWHEEL && ui->app_mode == UI_APP_SOUNDS &&
        ui->sound.plane == UI_SOUND_PLANE_BGM) {
        SoundEditorLayout lo;
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        int mx = lx;
        int my = ly;
        int ch, region, handle;
        sound_editor_layout(ui, &lo);
        /* Prefer live coords; fall back to last motion position. */
        if (mx == 0 && my == 0 && (ui->mouse_x || ui->mouse_y)) {
            mx = ui->mouse_x;
            my = ui->mouse_y;
        }
        ui->mouse_x = mx;
        ui->mouse_y = my;
        /* Pitch only when wheel is over the selected strip; otherwise always scroll. */
        if (ui->sound.sel_kind == UI_SOUND_SEL_REGION &&
            sound_region_hit(ui, mx, my, &ch, &region, &handle) && handle >= 1 &&
            ch == ui->sound.sel_ch && region == ui->sound.sel_region) {
            int track = ui->sound.track_idx;
            if (track < 0 || track >= ui->sound.track_count) {
                track = 0;
            }
            if (ch >= 0 && ch < UI_SOUND_BGM_CH && region >= 0 &&
                region < ui->sound.region_count[track][ch]) {
                ui_bgm_nudge_region(&ui->sound.region[track][ch][region], ch, e->wheel.y > 0 ? 1 : -1, shift);
                return 1;
            }
        }
        ui->sound.scroll_x -= e->wheel.y; /* down (y<0) → scroll right */
        ui_bgm_clamp_scroll(ui, lo.visible_ticks);
        return 1;
    }
    if (e->type == SDL_MOUSEWHEEL &&
        (ui->tile_edit.open || ui->sprite_edit.open || ui->metasprite_edit.open || ui->entity_edit.open)) {
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        int row;
        const R01World *w = r01_project_active_world_const(ui->project);
        row = w ? w->default_pal_row : 0;
        if (ui->tile_edit.open) {
            ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_BG, ui->tile_edit.pal, ui->tile_edit.color,
                                 e->wheel.y, shift);
            return 1;
        }
        if (ui->sprite_edit.open) {
            ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->sprite_edit.pal, ui->sprite_edit.color,
                                 e->wheel.y, shift);
            return 1;
        }
        if (ui->metasprite_edit.open) {
            ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->metasprite_edit.paint_pal,
                                 ui->metasprite_edit.paint_color, e->wheel.y, shift);
            return 1;
        }
        if (ui->entity_edit.open) {
            if (entity_modal_wheel(ui, lx, ly, e->wheel.y, shift)) {
                return 1;
            }
        }
    }
    if (e->type == SDL_TEXTINPUT) {
        if (ui->text.field_id > 0) {
            ui_text_input(ui, e->text.text);
            return 1;
        }
        return 0;
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
                if (ui->text.field_id > 0) {
                    ui_text_blur(ui);
                } else {
                    ui->metasprite_edit.open = 0;
                    ui_text_blur(ui);
                }
                return 1;
            }
            metasprite_modal_key(ui, e->key.keysym.sym);
            return 1;
        }
        if (ui->entity_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                if (ui->text.field_id > 0) {
                    ui_text_blur(ui);
                } else {
                    ui->entity_edit.open = 0;
                    ui_focus_clear(ui);
                    ui_text_blur(ui);
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
            !ui->play.active && !ui->menu.open) {
            if (ui->sel_instance >= 0) {
                R01World *w = r01_project_active_world(ui->project);
                if (w && r01_world_instance_remove(w, ui->sel_instance) == 0) {
                    ui->sel_instance = -1;
                    ui->inst_drag = 0;
                    ui_toast(ui, "instance removed", 0);
                }
                return 1;
            }
            if (ui_world_screen_remove(ui)) {
                return 1;
            }
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
            if (e->key.keysym.sym == SDLK_c) {
                if (ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM) {
                    ui_bgm_copy_sel(ui);
                    if (ui->sound.clip_valid) {
                        ui_toast(ui, "copied", 0);
                    }
                    return 1;
                }
                if (ui_world_screen_copy(ui)) {
                    return 1;
                }
            }
            if (e->key.keysym.sym == SDLK_v) {
                if (ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM) {
                    ui_bgm_paste_sel(ui);
                    return 1;
                }
                if (ui_world_screen_paste(ui)) {
                    return 1;
                }
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
            if ((e->key.keysym.mod & KMOD_SHIFT) && e->key.keysym.sym == SDLK_r) {
                return 4;
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
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            return 3; /* quit app when no modal/menu */
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
            if (ui->app_mode == UI_APP_SOUNDS) {
                ui_sound_play_toggle(ui);
            } else if (ui->app_mode == UI_APP_GRAPHICS) {
                ui_toggle_play(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_p && ui->app_mode == UI_APP_SOUNDS &&
            ui->sound.plane == UI_SOUND_PLANE_BGM) {
            ui_sound_play_pause(ui);
            return 1;
        }
        if ((e->key.keysym.sym == SDLK_DELETE || e->key.keysym.sym == SDLK_BACKSPACE) &&
            ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM &&
            ui->sound.sel_kind == UI_SOUND_SEL_REGION) {
            int track = ui->sound.track_idx;
            int ch = ui->sound.sel_ch;
            int region = ui->sound.sel_region;
            if (track < 0 || track >= ui->sound.track_count) {
                track = 0;
            }
            if (ch >= 0 && ch < UI_SOUND_BGM_CH && region >= 0 &&
                region < ui->sound.region_count[track][ch]) {
                ui_bgm_remove_region(ui, track, ch, region);
                ui->sound.sel_kind = UI_SOUND_SEL_NONE;
                ui->sound.sel_region = -1;
            }
            return 1;
        }
        /* Face buttons: Sim map (P1 G/H) is sampled each frame in ui_tick. */
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

        if (e->button.button == SDL_BUTTON_LEFT) {
            int app_tab;
            if (app_mode_tab_hit(ui, lx, ly, &app_tab)) {
                ui->arm_kind = UI_ARM_APP_TAB;
                ui->arm_a = app_tab;
                return 1;
            }
        }

        if (ui->app_mode == UI_APP_SOUNDS) {
            if (e->button.button == SDL_BUTTON_LEFT) {
                int idx, ch, tick, region, handle;
                ui->arm_kind = UI_ARM_NONE;
                if (sound_plane_tab_hit(ui, lx, ly, &idx)) {
                    ui->arm_kind = UI_ARM_SOUND_PLANE;
                    ui->arm_a = idx;
                    return 1;
                }
                if (ui->sound.plane == UI_SOUND_PLANE_BGM) {
                    if (sound_play_hit(ui, lx, ly)) {
                        ui->arm_kind = UI_ARM_SOUND_PLAY;
                        return 1;
                    }
                    if (sound_pause_hit(ui, lx, ly)) {
                        ui->arm_kind = UI_ARM_SOUND_PAUSE;
                        return 1;
                    }
                    if (sound_stop_hit(ui, lx, ly)) {
                        ui->arm_kind = UI_ARM_SOUND_STOP;
                        return 1;
                    }
                    if (sound_add_hit(ui, lx, ly)) {
                        ui->arm_kind = UI_ARM_SOUND_ADD;
                        return 1;
                    }
                    if (sound_track_hit(ui, lx, ly, &idx)) {
                        ui->arm_kind = UI_ARM_SOUND_TRACK;
                        ui->arm_a = idx;
                        return 1;
                    }
                    if (sound_channel_hit(ui, lx, ly, &ch)) {
                        ui->arm_kind = UI_ARM_SOUND_CH;
                        ui->arm_a = ch;
                        return 1;
                    }
                    handle = sound_region_hit(ui, lx, ly, &ch, &region, NULL);
                    if (handle == 2 || handle == 3) {
                        int track = ui->sound.track_idx;
                        const UiBgmRegion *rg;
                        if (track < 0 || track >= ui->sound.track_count) {
                            track = 0;
                        }
                        rg = &ui->sound.region[track][ch][region];
                        ui->sound.drag = (handle == 2) ? UI_SOUND_DRAG_RESIZE_L : UI_SOUND_DRAG_RESIZE_R;
                        ui->sound.drag_ch = ch;
                        ui->sound.drag_region = region;
                        ui->sound.drag_origin = rg->start;
                        ui->sound.drag_start0 = rg->start;
                        ui->sound.drag_len0 = rg->len;
                        ui->sound.drag_mx0 = lx;
                        ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                        ui->sound.sel_ch = ch;
                        ui->sound.sel_region = region;
                        return 1;
                    }
                    if (handle == 1) {
                        /* Toggle: re-click selected strip clears selection. */
                        if (ui->sound.sel_kind == UI_SOUND_SEL_REGION && ui->sound.sel_ch == ch &&
                            ui->sound.sel_region == region) {
                            ui->sound.sel_kind = UI_SOUND_SEL_NONE;
                        } else {
                            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                            ui->sound.sel_ch = ch;
                            ui->sound.sel_region = region;
                        }
                        return 1;
                    }
                    if (sound_timeline_hit(ui, lx, ly, &ch, &tick)) {
                        ui->sound.drag = UI_SOUND_DRAG_PAINT;
                        ui->sound.drag_ch = ch;
                        ui->sound.drag_origin = tick;
                        ui->sound.drag_region = -1;
                        ui->sound.drag_start0 = tick;
                        ui->sound.drag_len0 = 0;
                        ui->sound.drag_mx0 = lx;
                        /* Empty click clears strip selection; pivot kept for paste. */
                        ui->sound.sel_kind = UI_SOUND_SEL_EMPTY;
                        ui->sound.sel_ch = ch;
                        ui->sound.sel_tick = tick;
                        return 1;
                    }
                }
            }
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
            {
                int tile_id;
                if (banks_cell_hit(ui, lx, ly, &tile_id)) {
                    menu_open_bank_cell(ui, lx, ly, ui->banks_idx, tile_id, ui->banks_plane);
                    return 1;
                }
            }
            if (metasprites_list_hit(ui, lx, ly, &meta_idx)) {
                menu_open_metasprite(ui, lx, ly, meta_idx);
                return 1;
            }
            {
                int mt_idx;
                if (metatiles_list_hit(ui, lx, ly, &mt_idx)) {
                    menu_open_metatile(ui, lx, ly, mt_idx);
                    return 1;
                }
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
                if (screen_hit(ui, lx, ly, &tx, &ty) && ui_edit_map_screen(ui)) {
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

            ui->arm_kind = UI_ARM_NONE;
            if (play_button_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_PLAY;
                return 1;
            }
            if (lx < UI_SIDEBAR_W && accordion_header_hit(ui, lx, ly, &acc_sec)) {
                ui->arm_kind = UI_ARM_ACCORDION;
                ui->arm_a = acc_sec;
                return 1;
            }
            if (!ui->play.active && sprites_add_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_CATALOG_ADD;
                ui->arm_a = 0;
                return 1;
            }
            if (!ui->play.active && metasprites_add_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_CATALOG_ADD;
                ui->arm_a = 1;
                return 1;
            }
            if (!ui->play.active && metatiles_add_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_CATALOG_ADD;
                ui->arm_a = 3;
                return 1;
            }
            if (!ui->play.active && entities_add_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_CATALOG_ADD;
                ui->arm_a = 2;
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
                    ui->arm_kind = UI_ARM_PAL_ROW;
                    ui->arm_a = prow;
                } else {
                    ui->arm_kind = UI_ARM_PAL_STRIP;
                }
                return 1;
            }
            if (world_sub_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_WORLD_SUB;
                return 1;
            }
            if (banks_sub_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_BANK_SUB;
                return 1;
            }
            if (banks_tab_hit(ui, lx, ly, &wi)) {
                ui->arm_kind = UI_ARM_BANK_TAB;
                ui->arm_a = wi;
                return 1;
            }
            if (world_btn_hit(ui, lx, ly, &wi)) {
                ui->arm_kind = UI_ARM_WORLD_TAB;
                ui->arm_a = wi;
                return 1;
            }
            if (!ui->play.active && world_cell_hit(ui, lx, ly, &col, &row)) {
                ui->arm_kind = UI_ARM_WORLD_CELL;
                ui->arm_a = col;
                ui->arm_b = row;
                return 1;
            }
            if (!ui->play.active && screen_layer_hit(ui, lx, ly, &mode_row)) {
                ui->arm_kind = UI_ARM_LAYER;
                ui->arm_a = mode_row;
                return 1;
            }
            if (!ui->play.active && screen_mode_hit(ui, lx, ly, &mode_row)) {
                ui->arm_kind = UI_ARM_MODE;
                ui->arm_a = mode_row;
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
        if (ui->sound.drag != UI_SOUND_DRAG_NONE) {
            int drag = ui->sound.drag;
            ui->sound.drag = UI_SOUND_DRAG_NONE;
            if (drag == UI_SOUND_DRAG_PAINT && ui->sound.drag_len0 == 0) {
                /* Click empty: clear strip selection; keep empty paste pivot. */
                ui->sound.sel_kind = UI_SOUND_SEL_EMPTY;
                ui->sound.sel_ch = ui->sound.drag_ch;
                ui->sound.sel_tick = ui->sound.drag_origin;
            } else if (drag == UI_SOUND_DRAG_PAINT && ui->sound.drag_region >= 0) {
                ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                ui->sound.sel_ch = ui->sound.drag_ch;
                ui->sound.sel_region = ui->sound.drag_region;
            } else if (drag == UI_SOUND_DRAG_RESIZE_L || drag == UI_SOUND_DRAG_RESIZE_R) {
                ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                ui->sound.sel_ch = ui->sound.drag_ch;
                ui->sound.sel_region = ui->sound.drag_region;
            }
            return 1;
        }
        if (ui->arm_kind != UI_ARM_NONE) {
            int kind = ui->arm_kind;
            int a = ui->arm_a;
            int b = ui->arm_b;
            int wi, col, row, acc_sec, mode_row, prow;
            ui->arm_kind = UI_ARM_NONE;
            if (kind == UI_ARM_APP_TAB) {
                int app_tab;
                if (app_mode_tab_hit(ui, lx, ly, &app_tab) && app_tab == a) {
                    if (a == UI_APP_SOUNDS && ui->play.active) {
                        ui_play_stop(ui);
                    }
                    if (a != UI_APP_SOUNDS) {
                        ui_sound_play_stop(ui);
                    }
                    ui->app_mode = a;
                    return 1;
                }
            }
            if (kind == UI_ARM_SOUND_PLANE) {
                int idx;
                if (sound_plane_tab_hit(ui, lx, ly, &idx) && idx == a) {
                    ui->sound.plane = a;
                    if (a == UI_SOUND_PLANE_SFX) {
                        ui_sound_play_stop(ui);
                        ui_toast(ui, "SFX editor coming soon", 0);
                    }
                    return 1;
                }
            }
            if (kind == UI_ARM_SOUND_TRACK) {
                int idx;
                if (sound_track_hit(ui, lx, ly, &idx) && idx == a) {
                    ui->sound.track_idx = a;
                    return 1;
                }
            }
            if (kind == UI_ARM_SOUND_ADD && sound_add_hit(ui, lx, ly)) {
                if (ui->sound.track_count < UI_SOUND_TRACKS_MAX) {
                    int n = ui->sound.track_count;
                    snprintf(ui->sound.track_name[n], sizeof(ui->sound.track_name[n]), "Track %d", n + 1);
                    ui->sound.track_count++;
                    ui->sound.track_idx = n;
                    ui_toast(ui, "track added (UI stub)", 0);
                } else {
                    ui_toast(ui, "track limit", 1);
                }
                return 1;
            }
            if (kind == UI_ARM_SOUND_CH) {
                int ch;
                if (sound_channel_hit(ui, lx, ly, &ch) && ch == a) {
                    ui->sound.solo_ch = a;
                    /* Re-apply isolation if preview is running. */
                    if (ui->sound.playing || ui->sound.paused) {
                        int was_paused = ui->sound.paused;
                        float pos = ui->sound.play_pos;
                        ui_sound_play_stop(ui);
                        ui_sound_play_start(ui);
                        if (was_paused) {
                            ui_sound_play_pause(ui);
                            if (pos >= 0.f) {
                                ui->sound.play_pos = pos;
                            }
                        }
                    }
                    return 1;
                }
            }
            if (kind == UI_ARM_SOUND_PLAY && sound_play_hit(ui, lx, ly)) {
                ui_sound_play_start(ui);
                return 1;
            }
            if (kind == UI_ARM_SOUND_PAUSE && sound_pause_hit(ui, lx, ly)) {
                ui_sound_play_pause(ui);
                return 1;
            }
            if (kind == UI_ARM_SOUND_STOP && sound_stop_hit(ui, lx, ly)) {
                ui_sound_play_stop(ui);
                return 1;
            }
            if (kind == UI_ARM_PLAY && ui->app_mode == UI_APP_GRAPHICS && play_button_hit(ui, lx, ly)) {
                ui_toggle_play(ui);
                return 1;
            }
            if (kind == UI_ARM_ACCORDION && lx < UI_SIDEBAR_W && accordion_header_hit(ui, lx, ly, &acc_sec) &&
                acc_sec == a) {
                accordion_toggle(ui, acc_sec);
                return 1;
            }
            if (kind == UI_ARM_CATALOG_ADD && !ui->play.active) {
                if (a == 0 && sprites_add_hit(ui, lx, ly)) {
                    sprite_edit_open_new(ui);
                    return 1;
                }
                if (a == 1 && metasprites_add_hit(ui, lx, ly)) {
                    metasprite_edit_open_new(ui);
                    return 1;
                }
                if (a == 3 && metatiles_add_hit(ui, lx, ly)) {
                    R01World *w = r01_project_active_world(ui->project);
                    if (w && r01_world_metatile_add(w) >= 0) {
                        ui_toast(ui, "metatile created", 0);
                    } else {
                        ui_toast(ui, "metatile catalog full", 1);
                    }
                    return 1;
                }
                if (a == 2 && entities_add_hit(ui, lx, ly)) {
                    entity_edit_open_new(ui);
                    return 1;
                }
            }
            if (kind == UI_ARM_PAL_ROW && !ui->play.active && palette_row_btn_hit(ui, lx, ly, &prow) &&
                prow == a) {
                if (ui->pal_edit.open) {
                    ui->pal_edit.row = prow;
                } else {
                    pal_edit_set_row(ui, prow, 1);
                }
                return 1;
            }
            if (kind == UI_ARM_PAL_STRIP && !ui->play.active && palette_strip_hit(ui, lx, ly) &&
                !palette_row_btn_hit(ui, lx, ly, &prow) && !ui->pal_edit.open) {
                pal_edit_open(ui);
                return 1;
            }
            if (kind == UI_ARM_WORLD_SUB && world_sub_hit(ui, lx, ly)) {
                ui->worlds_plane =
                    (ui->worlds_plane == UI_WORLDS_PLANE_BG0) ? UI_WORLDS_PLANE_BG1 : UI_WORLDS_PLANE_BG0;
                if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
                    ui->screen_layer = UI_SCREEN_LAYER_BG;
                    ui->sel_instance = -1;
                    ui->inst_drag = 0;
                    screen_sel_clear(ui);
                }
                return 1;
            }
            if (kind == UI_ARM_BANK_SUB && banks_sub_hit(ui, lx, ly)) {
                ui->banks_plane =
                    (ui->banks_plane == UI_BANKS_PLANE_BG) ? UI_BANKS_PLANE_SPR : UI_BANKS_PLANE_BG;
                return 1;
            }
            if (kind == UI_ARM_BANK_TAB && banks_tab_hit(ui, lx, ly, &wi) && wi == a) {
                ui->banks_idx = wi;
                return 1;
            }
            if (kind == UI_ARM_WORLD_TAB && world_btn_hit(ui, lx, ly, &wi) && wi == a) {
                r01_project_set_active_world(ui->project, wi);
                ui->world_sel_col = -1;
                ui->world_sel_row = -1;
                return 1;
            }
            if (kind == UI_ARM_WORLD_CELL && !ui->play.active && world_cell_hit(ui, lx, ly, &col, &row) &&
                col == a && row == b) {
                Uint32 now = SDL_GetTicks();
                int dbl = (col == ui->last_click_col && row == ui->last_click_row &&
                           now - ui->last_click_ms < 350u);
                int up_ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
                handle_world_click(ui, col, row, up_ctrl, dbl);
                ui->last_click_ms = now;
                ui->last_click_col = col;
                ui->last_click_row = row;
                return 1;
            }
            if (kind == UI_ARM_LAYER && !ui->play.active && screen_layer_hit(ui, lx, ly, &mode_row) &&
                mode_row == a) {
                ui->screen_layer = mode_row;
                if (ui->screen_layer == UI_SCREEN_LAYER_BG) {
                    ui->sel_instance = -1;
                    ui->inst_drag = 0;
                } else {
                    screen_sel_clear(ui);
                }
                return 1;
            }
            if (kind == UI_ARM_MODE && !ui->play.active && screen_mode_hit(ui, lx, ly, &mode_row) &&
                mode_row == a) {
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
            return 1;
        }
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

    if (e->type == SDL_MOUSEMOTION && ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM &&
        ui->sound.drag != UI_SOUND_DRAG_NONE && (e->motion.state & SDL_BUTTON_LMASK)) {
        int ch, tick;
        int track = ui->sound.track_idx;
        if (track < 0 || track >= ui->sound.track_count) {
            track = 0;
        }
        if (!sound_timeline_hit(ui, lx, ly, &ch, &tick)) {
            SoundEditorLayout lo;
            sound_editor_layout(ui, &lo);
            if (point_in_rect(lx, ly, lo.timeline_x - 64, lo.timeline_y, lo.timeline_w + 128, lo.timeline_h)) {
                tick = ui->sound.scroll_x + (lx - lo.timeline_x) / lo.px_per_tick;
                if (tick < 0) {
                    tick = 0;
                }
                if (tick >= UI_SOUND_STEPS_MAX) {
                    tick = UI_SOUND_STEPS_MAX - 1;
                }
                ch = ui->sound.drag_ch;
            } else {
                return 1;
            }
        }
        if (ui->sound.drag == UI_SOUND_DRAG_PAINT) {
            int start = ui->sound.drag_origin;
            int end = tick;
            int len;
            int dx;
            UiBgmRegion rg;
            dx = lx - ui->sound.drag_mx0;
            if (dx < 0) {
                dx = -dx;
            }
            /* Ignore tiny jitter so a click selects empty pivot. */
            if (ui->sound.drag_len0 == 0 && tick == ui->sound.drag_origin && dx < UI_SOUND_PX_PER_TICK / 2) {
                return 1;
            }
            if (end < start) {
                int tmp = start;
                start = end;
                end = tmp;
            }
            len = end - start + 1;
            if (len < 1) {
                len = 1;
            }
            memset(&rg, 0, sizeof(rg));
            rg.start = start;
            rg.len = len;
            ui_bgm_default_tok(ui->sound.drag_ch, rg.tok, &rg.midi);
            if (ui->sound.drag_region >= 0) {
                ui_bgm_remove_region(ui, track, ui->sound.drag_ch, ui->sound.drag_region);
                ui->sound.drag_region = -1;
            }
            ui->sound.drag_region = ui_bgm_place_region(ui, track, ui->sound.drag_ch, &rg);
            ui->sound.drag_len0 = len;
            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
            ui->sound.sel_ch = ui->sound.drag_ch;
            ui->sound.sel_region = ui->sound.drag_region;
            return 1;
        }
        if (ui->sound.drag == UI_SOUND_DRAG_RESIZE_L || ui->sound.drag == UI_SOUND_DRAG_RESIZE_R) {
            int start = ui->sound.drag_start0;
            int end = ui->sound.drag_start0 + ui->sound.drag_len0;
            int new_start, new_len, idx;
            if (ui->sound.drag == UI_SOUND_DRAG_RESIZE_L) {
                new_start = tick;
                if (new_start > end - 1) {
                    new_start = end - 1;
                }
                if (new_start < 0) {
                    new_start = 0;
                }
                new_len = end - new_start;
            } else {
                new_start = start;
                new_len = tick - start + 1;
                if (new_len < 1) {
                    new_len = 1;
                }
            }
            idx = ui_bgm_resize_region(ui, track, ui->sound.drag_ch, ui->sound.drag_region, new_start, new_len);
            if (idx >= 0) {
                ui->sound.drag_region = idx;
                ui->sound.sel_region = idx;
            }
            return 1;
        }
        return 1;
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
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (entity_modal_wheel(ui, lx, ly, e->wheel.y, shift)) {
            return 1;
        }
    }
    if (e->type == SDL_MOUSEWHEEL && !ui->pal_edit.open && !ui->tile_edit.open && !ui->sprite_edit.open &&
        !ui->metasprite_edit.open && !ui->entity_edit.open && !ui->play.active) {
        AccordionLayout lo;
        accordion_layout(ui, &lo);
        if (lo.sprites_body_h > UI_BTN_H && lx < UI_SIDEBAR_W) {
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
        if (lo.metasprites_body_h > UI_BTN_H && lx < UI_SIDEBAR_W) {
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
        if (lo.metatiles_body_h > UI_BTN_H && lx < UI_SIDEBAR_W) {
            const R01World *w = r01_project_active_world_const(ui->project);
            int vis = (UI_METATILES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
            int max_scroll = 0;
            if (w && w->metatile_count > vis) {
                max_scroll = w->metatile_count - vis;
            }
            ui->metatiles_scroll -= e->wheel.y;
            if (ui->metatiles_scroll < 0) {
                ui->metatiles_scroll = 0;
            }
            if (ui->metatiles_scroll > max_scroll) {
                ui->metatiles_scroll = max_scroll;
            }
            return 1;
        }
        if (lo.entities_body_h > UI_BTN_H && lx < UI_SIDEBAR_W) {
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
