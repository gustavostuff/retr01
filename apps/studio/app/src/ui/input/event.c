#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/undo/undo.h"
#include "ui/undo/undo_cmds.h"
#include "ui/sound/bgm_edit.h"
#include "ui/modals/project_io.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/entity_import.h"
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

static void bank_sel_begin_marquee(UiState *ui, int plane, int bank, int tile_id, int ctrl) {
    if (!ui) {
        return;
    }
    if (ui->bank_sel_plane != plane || ui->bank_sel_bank != bank) {
        memset(ui->bank_sel_mask, 0, sizeof(ui->bank_sel_mask));
        ui->bank_sel_tile = -1;
    }
    ui->bank_sel_plane = plane;
    ui->bank_sel_bank = bank;
    ui->sel_instance = -1;
    ui->inst_drag = 0;
    screen_sel_clear(ui);
    memcpy(ui->bank_sel_mask_before, ui->bank_sel_mask, sizeof(ui->bank_sel_mask_before));
    ui->bank_sel_add = ctrl && bank_sel_valid(ui);
    ui->bank_sel_drag = 1;
    ui->bank_sel_drag_moved = 0;
    ui->bank_sel_anchor = tile_id;
    ui->bank_sel_drag_tile = tile_id;
}

static void bank_sel_finish_marquee(UiState *ui, int lx, int ly) {
    int tid;
    if (!ui || !ui->bank_sel_drag) {
        return;
    }
    tid = bank_sel_cell_clamped(ui, lx, ly);
    ui->bank_sel_drag_tile = tid;
    if (ui->bank_sel_drag_moved) {
        bank_sel_select_rect(ui, ui->bank_sel_anchor, tid, ui->bank_sel_add);
    } else {
        bank_sel_select_rect(ui, ui->bank_sel_anchor, ui->bank_sel_anchor, ui->bank_sel_add);
    }
    ui->bank_sel_drag = 0;
    ui->bank_sel_drag_moved = 0;
}

/* Catalog drops belong on the BG1 playfield Sprite layer (README Place on screen). */
static void catalog_drop_arm_spr_preview(UiState *ui) {
    R01World *w;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (ui->worlds_plane == UI_WORLDS_PLANE_BG0) {
        R01Screen *bg0 = r01_project_active_bg0_screen(ui->project);
        ui->worlds_plane = UI_WORLDS_PLANE_BG1;
        if (w && bg0) {
            int idx = r01_world_find_screen(w, bg0->col, bg0->row);
            if (idx >= 0) {
                ui->project->active_screen = idx;
            }
        }
    }
    ui->hide_spr_layer = 0;
    ui->screen_layer = UI_SCREEN_LAYER_SPR;
    screen_sel_clear(ui);
}

int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly) {
    if (!ui) {
        return 0;
    }
    if (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP) {
        ui->mouse_x = lx;
        ui->mouse_y = ly;
    }
    if (e->type == SDL_KEYDOWN && (e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_f) {
        return 2; /* toggle fullscreen */
    }
    if (ui_project_io_is_open(ui)) {
        return ui_project_io_event(ui, e, lx, ly);
    }
    if (ui->menu.open && (e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEWHEEL ||
                          e->type == SDL_MOUSEBUTTONUP)) {
        if (e->type == SDL_MOUSEMOTION) {
            ui_update_cursor(ui);
            menu_update_hover(ui, lx, ly);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION) {
        ui_update_cursor(ui);
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
        int ch = 0;
        int region = 0;
        sound_editor_layout(ui, &lo);
        /* Prefer live coords; fall back to last motion position. */
        if (mx == 0 && my == 0 && (ui->mouse_x || ui->mouse_y)) {
            mx = ui->mouse_x;
            my = ui->mouse_y;
        }
        ui->mouse_x = mx;
        ui->mouse_y = my;
        /* Hovering a strip: select it (or nudge the whole selection) and change pitch.
         * Empty space: deselect and pan. */
        if (sound_region_hit(ui, mx, my, &ch, &region, NULL)) {
            int track = ui->sound.track_idx;
            if (track < 0 || track >= ui->sound.track_count) {
                track = 0;
            }
            if (ch >= 0 && ch < UI_SOUND_BGM_CH && region >= 0 &&
                region < ui->sound.region_count[track][ch]) {
                if (ui_bgm_is_sel(ui, ch, region)) {
                    ui_bgm_nudge_sel(ui, e->wheel.y > 0 ? 1 : -1, shift);
                } else {
                    ui_bgm_sel_only(ui, ch, region);
                    ui_bgm_nudge_region(&ui->sound.region[track][ch][region], ch,
                                        e->wheel.y > 0 ? 1 : -1, shift);
                }
            }
            return 1;
        }
        ui_bgm_sel_clear(ui);
        ui->sound.sel_kind = UI_SOUND_SEL_NONE;
        ui->sound.scroll_x -= e->wheel.y; /* down (y<0) -> scroll right */
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
            int plane = ui->tile_edit.other_spr ? UI_PAL_PLANE_SPR : UI_PAL_PLANE_BG;
            ui_palette_grid_nudge(ui->project, row, plane, ui->tile_edit.pal, ui->tile_edit.color, e->wheel.y,
                                 shift);
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
            ui_text_input(&ui->text, e->text.text);
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
                    ui_text_blur(&ui->text);
                } else {
                    ui->metasprite_edit.open = 0;
                    ui_text_blur(&ui->text);
                }
                return 1;
            }
            metasprite_modal_key(ui, e->key.keysym.sym);
            return 1;
        }
        if (ui->entity_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                if (ui->text.field_id > 0) {
                    ui_text_blur(&ui->text);
                } else {
                    ui_undo_spr_paint_end(ui);
                    ui->entity_edit.open = 0;
                    ui_focus_clear(ui);
                    ui_text_blur(&ui->text);
                }
                return 1;
            }
            if ((e->key.keysym.mod & KMOD_CTRL) && ui->text.field_id < 1 &&
                (e->key.keysym.sym == SDLK_z || e->key.keysym.sym == SDLK_y)) {
                if (e->key.keysym.sym == SDLK_z) {
                    if (e->key.keysym.mod & KMOD_SHIFT) {
                        (void)ui_undo_redo(ui);
                    } else {
                        (void)ui_undo_undo(ui);
                    }
                } else {
                    (void)ui_undo_redo(ui);
                }
                return 1;
            }
            if ((e->key.keysym.mod & KMOD_CTRL) && ui->text.field_id < 1) {
                if (e->key.keysym.sym == SDLK_c) {
                    entity_edit_copy_parts(ui);
                    return 1;
                }
                if (e->key.keysym.sym == SDLK_v) {
                    (void)entity_edit_paste_clipboard(ui);
                    return 1;
                }
                if (e->key.keysym.sym == SDLK_a) {
                    entity_edit_select_all_parts(ui);
                    return 1;
                }
            }
            entity_modal_key(ui, e->key.keysym.sym);
            return 1;
        }
        if (ui->tile_edit.open) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                tile_modal_stroke_end(ui);
                ui->tile_edit.open = 0;
                return 1;
            }
            if ((e->key.keysym.mod & KMOD_CTRL) && (e->key.keysym.sym == SDLK_z || e->key.keysym.sym == SDLK_y)) {
                if (e->key.keysym.sym == SDLK_z) {
                    if (e->key.keysym.mod & KMOD_SHIFT) {
                        (void)tile_edit_redo(ui);
                    } else {
                        (void)tile_edit_undo(ui);
                    }
                } else {
                    (void)tile_edit_redo(ui);
                }
                return 1;
            }
            if ((e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_v) {
                uint8_t before[R01_TILE_BYTES];
                memcpy(before, ui->tile_edit.chr, R01_TILE_BYTES);
                if (ui_paste_clipboard_png_tile(ui, ui->tile_edit.chr, ui->tile_edit.pal, 0) == 0) {
                    tile_modal_stroke_end(ui);
                    memcpy(ui->tile_edit.stroke_before, before, R01_TILE_BYTES);
                    ui->tile_edit.stroke_open = 1;
                    ui->tile_edit.stroke_dirty = 1;
                    tile_modal_stroke_end(ui);
                }
                return 1;
            }
            return 1;
        }
        if ((e->key.keysym.sym == SDLK_DELETE || e->key.keysym.sym == SDLK_BACKSPACE) &&
            !ui->play.active && !ui->menu.open) {
            /* Audio BGM must run before world-screen Delete (that path always consumes). */
            if (ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM &&
                ui_bgm_sel_count(ui) > 0) {
                ui_bgm_remove_sel(ui);
                if (ui->sound.play_sel) {
                    ui_sound_play_stop(ui);
                }
                return 1;
            }
            if (ui->app_mode == UI_APP_GRAPHICS) {
                int region = ui_region_get(ui);
                if (region == UI_REGION_BANKS || region == UI_REGION_GLOBAL_BANKS) {
                    if (bank_sel_valid(ui)) {
                        bank_sel_remove_selected(ui);
                    }
                    return 1;
                }
                if (region == UI_REGION_PREVIEW) {
                    if (ui->sel_instance >= 0 && ui_work_allows_spr(ui)) {
                        R01World *w = r01_project_active_world(ui->project);
                        if (w && ui->sel_instance < w->instance_count) {
                            R01EntityInstance removed = w->instances[ui->sel_instance];
                            int idx = ui->sel_instance;
                            if (r01_world_instance_remove(w, idx) == 0) {
                                ui_undo_push_instance_remove(ui, idx, &removed);
                                ui->sel_instance = -1;
                                ui->inst_drag = 0;
                                ui_toast(ui, "instance removed", 0);
                            }
                        }
                    }
                    return 1;
                }
                if (region == UI_REGION_WORLDS) {
                    if (ui_world_screen_remove(ui)) {
                        return 1;
                    }
                    return 1;
                }
                return 1;
            }
        }
        if (!ui->play.active && !ui->menu.open && ui_work_allows_spr(ui) &&
            ui->sel_instance >= 0 && ui_region_get(ui) == UI_REGION_PREVIEW &&
            (e->key.keysym.sym == SDLK_h || e->key.keysym.sym == SDLK_v)) {
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
            if (ui->text.field_id < 1 && (e->key.keysym.sym == SDLK_z || e->key.keysym.sym == SDLK_y)) {
                if (e->key.keysym.sym == SDLK_z) {
                    if (e->key.keysym.mod & KMOD_SHIFT) {
                        (void)ui_undo_redo(ui);
                    } else {
                        (void)ui_undo_undo(ui);
                    }
                } else {
                    (void)ui_undo_redo(ui);
                }
                return 1;
            }
            if (e->key.keysym.sym == SDLK_s) {
                ui_project_io_request_save(ui, (e->key.keysym.mod & KMOD_SHIFT) != 0);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_a && ui->text.field_id < 1 && !ui->play.active) {
                if (ui->app_mode == UI_APP_GRAPHICS) {
                    int region = ui_region_get(ui);
                    if (region == UI_REGION_BANKS || region == UI_REGION_GLOBAL_BANKS) {
                        bank_sel_select_all(ui);
                        return 1;
                    }
                }
                if (ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM) {
                    ui_bgm_sel_all(ui);
                    return 1;
                }
            }
            if (e->key.keysym.sym == SDLK_c) {
                if (ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM) {
                    ui_bgm_copy_sel(ui);
                    if (ui->sound.clip_valid) {
                        ui_toast(ui, "copied", 0);
                    }
                    return 1;
                }
                if (ui->app_mode == UI_APP_GRAPHICS) {
                    int region;
                    if (ui->play.active) {
                        return 1;
                    }
                    region = ui_region_get(ui);
                    if (region == UI_REGION_WORLDS) {
                        (void)ui_world_screen_copy(ui);
                        return 1;
                    }
                    if (region == UI_REGION_PREVIEW) {
                        if (!ui_tile_selection_copy(ui)) {
                            ui_toast(ui, "select tiles to copy", 1);
                        }
                        return 1;
                    }
                    return 1;
                }
            }
            if (e->key.keysym.sym == SDLK_v) {
                if (ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM) {
                    ui_bgm_paste_sel(ui);
                    return 1;
                }
                if (ui->app_mode == UI_APP_GRAPHICS) {
                    int region;
                    if (ui->play.active) {
                        return 1;
                    }
                    region = ui_region_get(ui);
                    if (region == UI_REGION_WORLDS) {
                        (void)ui_world_screen_paste(ui);
                        return 1;
                    }
                    if (region == UI_REGION_PREVIEW) {
                        if (!ui_tile_selection_paste(ui)) {
                            ui_toast(ui, "no tiles on clipboard", 1);
                        }
                        return 1;
                    }
                    return 1;
                }
            }
            if (e->key.keysym.sym == SDLK_e) {
                ui_export(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_o) {
                ui_project_io_open_browse(ui);
                return 1;
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
        if (!ui->play.active && !ui->menu.open && screen_sel_valid(ui) && ui_work_allows_bg(ui) &&
            ui->sel_instance < 0 && ui_region_get(ui) == UI_REGION_PREVIEW) {
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
        if (UI_SOUND_SECTION_PLAY && ui->text.field_id < 1 && !ui->play.active && !ui->menu.open &&
            ui->app_mode == UI_APP_SOUNDS && ui->sound.plane == UI_SOUND_PLANE_BGM &&
            !(e->key.keysym.mod & (KMOD_CTRL | KMOD_ALT)) &&
            (e->key.keysym.sym == SDLK_LEFT || e->key.keysym.sym == SDLK_RIGHT)) {
            ui_sound_play_section(ui, e->key.keysym.sym == SDLK_RIGHT ? 1 : -1);
            return 1;
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
            if (ui->sound.sel_kind == UI_SOUND_SEL_REGION || ui_bgm_sel_count(ui) > 0) {
                if (ui->sound.playing && ui->sound.play_sel) {
                    ui_sound_play_pause(ui);
                } else {
                    ui_sound_play_start_sel(ui);
                }
            } else {
                ui_sound_play_pause(ui);
            }
            return 1;
        }
        /* Face buttons: Sim map (P1 G/H) is sampled each frame in ui_tick. */
    }
    if (e->type == SDL_KEYUP) {
        ui->keys[e->key.keysym.scancode] = 0;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN) {
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
            tile_modal_handle(ui, lx, ly, 1, e->button.button);
            return 1;
        }

        ui_region_focus_at(ui, lx, ly);

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
                int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
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
                    if (sound_zoom_out_hit(ui, lx, ly)) {
                        ui->arm_kind = UI_ARM_SOUND_ZOOM_OUT;
                        return 1;
                    }
                    if (sound_zoom_in_hit(ui, lx, ly)) {
                        ui->arm_kind = UI_ARM_SOUND_ZOOM_IN;
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
                    if (shift && sound_timeline_hit(ui, lx, ly, &ch, &tick)) {
                        ui->sound.drag = UI_SOUND_DRAG_MARQUEE;
                        ui->sound.drag_ch = ch;
                        ui->sound.drag_ch1 = ch;
                        ui->sound.drag_start0 = tick;
                        ui->sound.drag_origin = tick;
                        ui->sound.drag_moved = 0;
                        ui->sound.drag_mx0 = lx;
                        ui->sound.drag_group = 0;
                        return 1;
                    }
                    handle = sound_region_hit(ui, lx, ly, &ch, &region, NULL);
                    if (ctrl && handle) {
                        ui_bgm_sel_toggle(ui, ch, region);
                        return 1;
                    }
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
                        ui->sound.drag_group = 0;
                        if (!ui_bgm_is_sel(ui, ch, region)) {
                            ui_bgm_sel_only(ui, ch, region);
                        } else {
                            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                            ui->sound.sel_ch = ch;
                            ui->sound.sel_region = region;
                        }
                        return 1;
                    }
                    if (handle == 1) {
                        int track = ui->sound.track_idx;
                        const UiBgmRegion *rg;
                        int grab;
                        if (track < 0 || track >= ui->sound.track_count) {
                            track = 0;
                        }
                        rg = &ui->sound.region[track][ch][region];
                        if (!ui_bgm_is_sel(ui, ch, region)) {
                            ui_bgm_sel_only(ui, ch, region);
                        } else {
                            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                            ui->sound.sel_ch = ch;
                            ui->sound.sel_region = region;
                        }
                        ui->sound.drag = UI_SOUND_DRAG_MOVE;
                        ui->sound.drag_ch = ch;
                        ui->sound.drag_region = region;
                        ui->sound.drag_start0 = rg->start;
                        ui->sound.drag_len0 = rg->len;
                        ui->sound.drag_mx0 = lx;
                        ui->sound.drag_group = ui_bgm_sel_count(ui) > 1;
                        if (ui->sound.drag_group) {
                            ui_bgm_move_sel_grab(ui);
                        }
                        grab = 0;
                        if (sound_timeline_hit(ui, lx, ly, NULL, &grab)) {
                            ui->sound.drag_origin = grab - rg->start;
                        } else {
                            ui->sound.drag_origin = 0;
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
                        ui->sound.drag_group = 0;
                        ui_bgm_sel_clear(ui);
                        ui->sound.sel_kind = UI_SOUND_SEL_EMPTY;
                        ui->sound.sel_ch = ch;
                        ui->sound.sel_tick = tick;
                        return 1;
                    }
                }
            }
            return 1;
        }

        if (ui->app_mode == UI_APP_CODE) {
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
                    int in_sel = bank_sel_valid(ui) && ui->bank_sel_plane == ui->banks_plane &&
                                 ui->bank_sel_bank == ui->banks_idx && bank_sel_has(ui, tile_id);
                    if (!in_sel) {
                        bank_sel_set(ui, ui->banks_plane, ui->banks_idx, tile_id);
                    }
                    menu_open_bank_cell(ui, lx, ly, ui->banks_idx, tile_id, ui->banks_plane);
                    return 1;
                }
                if (global_banks_cell_hit(ui, lx, ly, &tile_id)) {
                    int in_sel = bank_sel_valid(ui) && ui->bank_sel_plane == ui->global_banks_plane &&
                                 ui->bank_sel_bank == ui->global_banks_idx && bank_sel_has(ui, tile_id);
                    if (!in_sel) {
                        bank_sel_set(ui, ui->global_banks_plane, ui->global_banks_idx, tile_id);
                    }
                    menu_open_bank_cell(ui, lx, ly, ui->global_banks_idx, tile_id, ui->global_banks_plane);
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
                if (ui_work_allows_spr(ui) && instance_hit_on_screen(ui, lx, ly, &inst)) {
                    ui->sel_instance = inst;
                    ui->inst_drag = 0;
                    screen_sel_clear(ui);
                    bank_sel_clear(ui);
                    menu_open_instance(ui, lx, ly, inst);
                    return 1;
                }
                if (ui_work_allows_bg(ui) && screen_hit(ui, lx, ly, &tx, &ty) && ui_edit_map_screen(ui)) {
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
                if (ui_work_allows_spr(ui)) {
                    return 1;
                }
            }
        }

        if (e->button.button == SDL_BUTTON_LEFT) {
            int wi, col, row, tx, ty, acc_sec, mode_row;
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            int alt = (SDL_GetModState() & KMOD_ALT) != 0;
            int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
            int flood = ui->keys[SDL_SCANCODE_F] != 0;

            ui->arm_kind = UI_ARM_NONE;
            if (play_button_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_PLAY;
                return 1;
            }
            if (preview_inspect_copy_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_PREVIEW_COPY;
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
            if (!ui->play.active && entities_import_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_ASEPRITE_IMPORT;
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
            if (global_banks_sub_hit(ui, lx, ly)) {
                ui->arm_kind = UI_ARM_GLOBAL_BANK_SUB;
                return 1;
            }
            if (banks_tab_hit(ui, lx, ly, &wi)) {
                ui->arm_kind = UI_ARM_BANK_TAB;
                ui->arm_a = wi;
                return 1;
            }
            if (global_banks_tab_hit(ui, lx, ly, &wi)) {
                ui->arm_kind = UI_ARM_GLOBAL_BANK_TAB;
                ui->arm_a = wi;
                return 1;
            }
            {
                int tile_id;
                if (!ui->play.active && banks_cell_hit(ui, lx, ly, &tile_id)) {
                    if (shift) {
                        bank_sel_begin_marquee(ui, ui->banks_plane, ui->banks_idx, tile_id, ctrl);
                        return 1;
                    }
                    if (ctrl) {
                        bank_sel_toggle(ui, ui->banks_plane, ui->banks_idx, tile_id);
                        return 1;
                    }
                    bank_sel_set(ui, ui->banks_plane, ui->banks_idx, tile_id);
                    return 1;
                }
                if (!ui->play.active && global_banks_cell_hit(ui, lx, ly, &tile_id)) {
                    if (shift) {
                        bank_sel_begin_marquee(ui, ui->global_banks_plane, ui->global_banks_idx, tile_id, ctrl);
                        return 1;
                    }
                    if (ctrl) {
                        bank_sel_toggle(ui, ui->global_banks_plane, ui->global_banks_idx, tile_id);
                        return 1;
                    }
                    bank_sel_set(ui, ui->global_banks_plane, ui->global_banks_idx, tile_id);
                    return 1;
                }
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
            {
                int hide_bg;
                if (!ui->play.active && screen_hide_hit(ui, lx, ly, &hide_bg)) {
                    ui->arm_kind = UI_ARM_HIDE_LAYER;
                    ui->arm_a = hide_bg;
                    return 1;
                }
            }
            if (!ui->play.active && screen_hit(ui, lx, ly, &tx, &ty)) {
                int inst;
                if (ui_work_allows_spr(ui) && instance_hit_on_screen(ui, lx, ly, &inst)) {
                    int px, py;
                    R01World *w = r01_project_active_world(ui->project);
                    R01Screen *s = r01_project_active_screen(ui->project);
                    if (!screen_pixel_hit(ui, lx, ly, &px, &py)) {
                        return 1;
                    }
                    ui->sel_instance = inst;
                    screen_sel_clear(ui);
                    bank_sel_clear(ui);
                    if (w && s && inst >= 0 && inst < w->instance_count) {
                        ui->inst_drag = 1;
                        ui->inst_drag_off_x = w->instances[inst].world_x - (s->col * R01_SCREEN_PX_W + px);
                        ui->inst_drag_off_y = w->instances[inst].world_y - (s->row * R01_SCREEN_PX_H + py);
                    }
                    return 1;
                }
                if (!ui_work_allows_bg(ui)) {
                    ui->sel_instance = -1;
                    ui->inst_drag = 0;
                    return 1;
                }
                ui->sel_instance = -1;
                ui->inst_drag = 0;
                if (alt) {
                    ui_paint_stamp_from_cell(ui, tx, ty);
                    screen_sel_set(ui, tx, ty, tx, ty);
                    ui_toast(ui, "stamp picked", 0);
                    return 1;
                }
                if (flood) {
                    ui_flood_fill(ui, tx, ty);
                    return 1;
                }
                if (ctrl) {
                    ui->last_paint_tx = -1;
                    ui->last_paint_ty = -1;
                    (void)ui_undo_paint_begin(ui);
                    ui_paint_tile(ui, tx, ty);
                    return 1;
                }
                if (shift) {
                    ui->sel_drag = 1;
                    ui->sel_drag_moved = 0;
                    ui->sel_anchor_x = tx;
                    ui->sel_anchor_y = ty;
                    if (!screen_sel_valid(ui)) {
                        screen_sel_set(ui, tx, ty, tx, ty);
                    }
                    return 1;
                }
                screen_sel_set(ui, tx, ty, tx, ty);
                ui_paint_stamp_from_cell(ui, tx, ty);
                return 1;
            }
        }
    }

    if (e->type == SDL_MOUSEBUTTONUP &&
        (e->button.button == SDL_BUTTON_RIGHT || e->button.button == SDL_BUTTON_MIDDLE) && ui->entity_edit.open) {
        entity_modal_handle(ui, lx, ly, 0, e->button.button);
        return 1;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (ui->tile_edit.open) {
            tile_modal_stroke_end(ui);
            return 1;
        }
        if (ui->bank_sel_drag) {
            bank_sel_finish_marquee(ui, lx, ly);
        }
        if (ui->sel_drag && ui_work_allows_bg(ui) && !ui->play.active) {
            int shift_up = (SDL_GetModState() & KMOD_SHIFT) != 0;
            if (!ui->sel_drag_moved && shift_up) {
                screen_sel_expand(ui, ui->sel_anchor_x, ui->sel_anchor_y);
            }
            if (screen_sel_valid(ui)) {
                ui_paint_stamp_from_selection(ui);
            }
        }
        ui_undo_paint_end(ui);
        ui->last_paint_tx = -1;
        ui->last_paint_ty = -1;
        ui->sel_drag = 0;
        ui->sel_drag_moved = 0;
        ui->inst_drag = 0;
        if (ui->sound.drag != UI_SOUND_DRAG_NONE) {
            int drag = ui->sound.drag;
            ui->sound.drag = UI_SOUND_DRAG_NONE;
            if (drag == UI_SOUND_DRAG_MARQUEE) {
                int ctrl_up = (SDL_GetModState() & KMOD_CTRL) != 0;
                if (ui->sound.drag_moved) {
                    ui_bgm_sel_rect(ui, ui->sound.drag_ch, ui->sound.drag_start0, ui->sound.drag_ch1,
                                    ui->sound.drag_origin, ctrl_up);
                } else {
                    int ch = 0, region = 0;
                    if (sound_region_hit(ui, lx, ly, &ch, &region, NULL)) {
                        if (!ui_bgm_is_sel(ui, ch, region)) {
                            ui_bgm_sel_toggle(ui, ch, region);
                        } else {
                            ui->sound.sel_ch = ch;
                            ui->sound.sel_region = region;
                            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                        }
                    }
                }
                ui->sound.drag_moved = 0;
                ui->sound.drag_group = 0;
                return 1;
            }
            if (drag == UI_SOUND_DRAG_PAINT && ui->sound.drag_len0 == 0) {
                /* Click empty: clear strip selection; keep empty paste pivot. */
                ui_bgm_sel_clear(ui);
                ui->sound.sel_kind = UI_SOUND_SEL_EMPTY;
                ui->sound.sel_ch = ui->sound.drag_ch;
                ui->sound.sel_tick = ui->sound.drag_origin;
            } else if (drag == UI_SOUND_DRAG_PAINT && ui->sound.drag_region >= 0) {
                ui_bgm_sel_only(ui, ui->sound.drag_ch, ui->sound.drag_region);
            } else if (drag == UI_SOUND_DRAG_RESIZE_L || drag == UI_SOUND_DRAG_RESIZE_R ||
                       drag == UI_SOUND_DRAG_MOVE) {
                ui->sound.sel_kind = UI_SOUND_SEL_REGION;
                ui->sound.sel_ch = ui->sound.drag_ch;
                ui->sound.sel_region = ui->sound.drag_region;
                ui_bgm_sel_sync(ui);
            }
            ui->sound.drag_moved = 0;
            ui->sound.drag_group = 0;
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
                    if ((a == UI_APP_SOUNDS || a == UI_APP_CODE) && ui->play.active) {
                        ui_play_stop(ui);
                    }
                    if (a != UI_APP_SOUNDS) {
                        ui_sound_play_stop(ui);
                    }
                    ui->app_mode = a;
                    if (a == UI_APP_SOUNDS) {
                        ui_region_set(ui, UI_REGION_SOUNDS);
                    } else if (a == UI_APP_GRAPHICS && ui_region_get(ui) == UI_REGION_SOUNDS) {
                        ui_region_set(ui, UI_REGION_WORLDS);
                    } else if (a == UI_APP_CODE) {
                        ui_region_set(ui, UI_REGION_NONE);
                    }
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
                    ui_bgm_sel_sync(ui);
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
            if (kind == UI_ARM_SOUND_ZOOM_OUT && sound_zoom_out_hit(ui, lx, ly)) {
                ui_bgm_zoom(ui, -1);
                return 1;
            }
            if (kind == UI_ARM_SOUND_ZOOM_IN && sound_zoom_in_hit(ui, lx, ly)) {
                ui_bgm_zoom(ui, 1);
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
            if (kind == UI_ARM_PREVIEW_COPY && preview_inspect_copy_hit(ui, lx, ly)) {
                preview_inspect_copy(ui);
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
                    int idx;
                    if (w && (idx = r01_world_metatile_add(w)) >= 0) {
                        ui_undo_push_metatile_add(ui, idx);
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
            if (kind == UI_ARM_ASEPRITE_IMPORT && !ui->play.active && entities_import_hit(ui, lx, ly)) {
                R01AsepriteImportResult res;
                char err[160];
                char toast[96];
                int before;
                const R01World *w = r01_project_active_world_const(ui->project);
                before = w ? w->entity_count : 0;
                err[0] = '\0';
                if (r01_project_import_aseprite_entities(ui->project, ui->project_path, &res, err, sizeof(err)) !=
                    0) {
                    ui_toast(ui, err[0] ? err : "aseprite import failed", 1);
                    return 1;
                }
                {
                    R01World *ww = r01_project_active_world(ui->project);
                    int i;
                    if (ww) {
                        for (i = before; i < ww->entity_count; i++) {
                            ui_undo_push_entity_add(ui, i);
                        }
                    }
                    if (ui->entity_edit.open && !ui->entity_edit.is_new && ww && ui->entity_edit.type_idx >= 0 &&
                        ui->entity_edit.type_idx < ww->entity_count) {
                        ui->entity_edit.draft = ww->entities[ui->entity_edit.type_idx];
                    }
                }
                if (res.unchanged || res.generated < 1) {
                    ui_toast(ui, "no new entities in aseprite_entities/", 0);
                    return 1;
                }
                if (res.generated == 1) {
                    snprintf(toast, sizeof(toast), "1 entity generated from aseprite_entities/ folder");
                } else {
                    snprintf(toast, sizeof(toast), "%d entities generated from aseprite_entities/ folder",
                             res.generated);
                }
                ui_toast(ui, toast, 0);
                return 1;
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
                } else if (ui->screen_layer == UI_SCREEN_LAYER_BG) {
                    /* BG0 forces BG work; restore Both so entity drops work on BG1. */
                    ui->screen_layer = UI_SCREEN_LAYER_BOTH;
                }
                return 1;
            }
            if (kind == UI_ARM_BANK_SUB && banks_sub_hit(ui, lx, ly)) {
                ui->banks_plane =
                    (ui->banks_plane == UI_BANKS_PLANE_BG) ? UI_BANKS_PLANE_SPR : UI_BANKS_PLANE_BG;
                bank_sel_clear(ui);
                return 1;
            }
            if (kind == UI_ARM_GLOBAL_BANK_SUB && global_banks_sub_hit(ui, lx, ly)) {
                ui->global_banks_plane = (ui->global_banks_plane == UI_BANKS_PLANE_GLOBAL_BG)
                                            ? UI_BANKS_PLANE_GLOBAL_SPR
                                            : UI_BANKS_PLANE_GLOBAL_BG;
                bank_sel_clear(ui);
                return 1;
            }
            if (kind == UI_ARM_BANK_TAB && banks_tab_hit(ui, lx, ly, &wi) && wi == a) {
                ui->banks_idx = wi;
                bank_sel_clear(ui);
                return 1;
            }
            if (kind == UI_ARM_GLOBAL_BANK_TAB && global_banks_tab_hit(ui, lx, ly, &wi) && wi == a) {
                ui->global_banks_idx = wi;
                bank_sel_clear(ui);
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
                } else if (ui->screen_layer == UI_SCREEN_LAYER_SPR) {
                    screen_sel_clear(ui);
                }
                return 1;
            }
            if (kind == UI_ARM_HIDE_LAYER && !ui->play.active) {
                int hide_bg;
                if (screen_hide_hit(ui, lx, ly, &hide_bg) && hide_bg == a) {
                    if (hide_bg) {
                        ui->hide_bg_layer = !ui->hide_bg_layer;
                        if (ui->hide_bg_layer) {
                            screen_sel_clear(ui);
                            ui->sel_drag = 0;
                            ui->sel_drag_moved = 0;
                        }
                    } else {
                        ui->hide_spr_layer = !ui->hide_spr_layer;
                        if (ui->hide_spr_layer) {
                            ui->sel_instance = -1;
                            ui->inst_drag = 0;
                        }
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
            if (!ui->play.active && screen_pixel_hit(ui, lx, ly, &px, &py)) {
                R01World *w;
                R01Screen *s;
                int wx, wy;
                int idx = -1;
                catalog_drop_arm_spr_preview(ui);
                w = r01_project_active_world(ui->project);
                s = r01_project_active_screen(ui->project);
                if (!w || !s) {
                    ui_toast(ui, "no screen", 1);
                    ui->catalog_drag.active = 0;
                    return 1;
                }
                wx = s->col * R01_SCREEN_PX_W + px;
                wy = s->row * R01_SCREEN_PX_H + py;
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
                    ui_undo_push_instance_add(ui, idx);
                    ui->sel_instance = idx;
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
        if (ui->sound.drag == UI_SOUND_DRAG_MARQUEE) {
            SoundEditorLayout lo;
            int lane;
            sound_editor_layout(ui, &lo);
            lane = lo.lane_h + lo.lane_gap;
            if (lane < 1) {
                lane = 1;
            }
            ch = (ly - lo.timeline_y) / lane;
            if (ch < 0) {
                ch = 0;
            }
            if (ch >= UI_SOUND_BGM_CH) {
                ch = UI_SOUND_BGM_CH - 1;
            }
            if (tick < 0) {
                tick = 0;
            }
            if (tick >= UI_SOUND_STEPS_MAX) {
                tick = UI_SOUND_STEPS_MAX - 1;
            }
            ui->sound.drag_ch1 = ch;
            ui->sound.drag_origin = tick;
            if (ch != ui->sound.drag_ch || tick != ui->sound.drag_start0) {
                ui->sound.drag_moved = 1;
            }
            return 1;
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
            if (ui->sound.drag_region >= 0) {
                ui_bgm_sel_only(ui, ui->sound.drag_ch, ui->sound.drag_region);
            }
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
        if (ui->sound.drag == UI_SOUND_DRAG_MOVE) {
            int new_start = tick - ui->sound.drag_origin;
            int len = ui->sound.drag_len0;
            int idx;
            int dx = lx - ui->sound.drag_mx0;
            if (dx < 0) {
                dx = -dx;
            }
            /* Tiny motion: treat as click-select (keep original start). */
            if (dx < UI_SOUND_PX_PER_TICK / 2 && tick == ui->sound.drag_start0 + ui->sound.drag_origin) {
                return 1;
            }
            if (ui->sound.drag_group) {
                ui_bgm_move_sel_apply(ui, new_start - ui->sound.drag_start0);
                ui->sound.drag_ch = ui->sound.sel_ch;
                ui->sound.drag_region = ui->sound.sel_region;
                return 1;
            }
            if (len < 1) {
                len = 1;
            }
            if (new_start < 0) {
                new_start = 0;
            }
            if (new_start + len > UI_SOUND_STEPS_MAX) {
                new_start = UI_SOUND_STEPS_MAX - len;
            }
            if (new_start < 0) {
                new_start = 0;
            }
            /* Stay on the channel the strip started on. */
            idx = ui_bgm_resize_region(ui, track, ui->sound.drag_ch, ui->sound.drag_region, new_start, len);
            if (idx >= 0) {
                ui->sound.drag_region = idx;
                ui->sound.sel_ch = ui->sound.drag_ch;
                ui->sound.sel_region = idx;
                ui->sound.sel_kind = UI_SOUND_SEL_REGION;
            }
            return 1;
        }
        return 1;
    }

    if (e->type == SDL_MOUSEMOTION && !ui->play.active && !ui->tile_edit.open && !ui->sprite_edit.open &&
        !ui->metasprite_edit.open && !ui->entity_edit.open && !ui->menu.open && !ui->catalog_drag.active) {
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        int tx, ty;
        if (ui->bank_sel_drag && shift && (e->motion.state & SDL_BUTTON_LMASK)) {
            int tid = bank_sel_cell_clamped(ui, lx, ly);
            if (tid != ui->bank_sel_anchor) {
                ui->bank_sel_drag_moved = 1;
            }
            ui->bank_sel_drag_tile = tid;
            if (ui->bank_sel_drag_moved) {
                bank_sel_select_rect(ui, ui->bank_sel_anchor, tid, ui->bank_sel_add);
            }
            return 1;
        }
        if (ui_work_allows_spr(ui) && ui->inst_drag && ui->sel_instance >= 0 &&
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
        if (ui_work_allows_bg(ui) && ui->sel_drag &&
            shift && (e->motion.state & SDL_BUTTON_LMASK) && screen_hit(ui, lx, ly, &tx, &ty)) {
            if (tx != ui->sel_anchor_x || ty != ui->sel_anchor_y) {
                ui->sel_drag_moved = 1;
            }
            if (ui->sel_drag_moved) {
                screen_sel_set(ui, ui->sel_anchor_x, ui->sel_anchor_y, tx, ty);
            }
            return 1;
        }
        if (ui_work_allows_bg(ui) && (SDL_GetModState() & KMOD_CTRL) &&
            (e->motion.state & SDL_BUTTON_LMASK) &&
            !(SDL_GetModState() & KMOD_ALT) && ui->keys[SDL_SCANCODE_F] == 0) {
            if (screen_hit(ui, lx, ly, &tx, &ty)) {
                ui_paint_tile(ui, tx, ty);
                return 1;
            }
        }
    }

    if (e->type == SDL_MOUSEMOTION && ui->tile_edit.open && (e->motion.state & SDL_BUTTON_LMASK)) {
        tile_modal_handle(ui, lx, ly, 1, SDL_BUTTON_LEFT);
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
        if (e->motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK | SDL_BUTTON_MMASK)) {
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
