#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hit_chip(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    if (e->orient == R01S_ORIENT_H) {
        return lx >= x - R01S_CHIP_PIN_OUT && lx < x + e->body_w + R01S_CHIP_PIN_OUT &&
               ly >= y - R01S_CHIP_PIN_OUT && ly < y + e->body_h + R01S_CHIP_PIN_OUT;
    }
    return lx >= x - R01S_CHIP_PIN_OUT && lx < x + e->body_w + R01S_CHIP_PIN_OUT &&
           ly >= y - R01S_CHIP_PIN_OUT && ly < y + e->body_h + R01S_CHIP_PIN_OUT;
}

static int hit_island_frame(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    return lx >= x && lx < x + island->board_w && ly >= y && ly < y + island->board_h;
}

/* Returns corner id, or -1 if miss. */
static int hit_island_resize(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int hs = R01S_ISLAND_RESIZE_HANDLE;
    int right = x + island->board_w;
    int bottom = y + island->board_h;

    if (lx >= right - hs && lx < right && ly >= bottom - hs && ly < bottom) {
        return R01S_ISLAND_CORNER_BR;
    }
    if (lx >= x && lx < x + hs && ly >= bottom - hs && ly < bottom) {
        return R01S_ISLAND_CORNER_BL;
    }
    if (lx >= right - hs && lx < right && ly >= y && ly < y + hs) {
        return R01S_ISLAND_CORNER_TR;
    }
    if (lx >= x && lx < x + hs && ly >= y && ly < y + hs) {
        return R01S_ISLAND_CORNER_TL;
    }
    return -1;
}

/* Front-most island first (matches island_z_order). */
static int island_hit_stack(const R01sUi *ui, int *out_idx, int max_out) {
    int k = 0;
    int rank;

    if (!ui || !ui->group || !out_idx || max_out <= 0) {
        return 0;
    }
    if (ui->island_z_count <= 0) {
        int n = r01s_island_group_count(ui->group);
        int i;
        if (n > max_out) {
            n = max_out;
        }
        for (i = n - 1; i >= 0; i--) {
            out_idx[k++] = i;
        }
        return k;
    }
    for (rank = ui->island_z_count - 1; rank >= 0; rank--) {
        if (k >= max_out) {
            break;
        }
        out_idx[k++] = ui->island_z_order[rank];
    }
    return k;
}

/* Topmost chip of this island under (lx,ly), or -1. */
static int hit_chip_in_island(const R01sUi *ui, int island_index, int lx, int ly) {
    int i;
    for (i = ui->chip_count - 1; i >= 0; i--) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (ui_chip_hidden(ui, ui->chips[i])) {
            continue;
        }
        if (ui->chips[i] && hit_chip(ui, ui->chips[i], lx, ly)) {
            return i;
        }
    }
    return -1;
}

/*
 * Board pick matching draw occlusion.
 * Returns: 0 miss, 1 chip (*chip_out), 2 move island (*island_out), 3 resize (*island_out, *corner_out).
 */
int hit_board_top(const R01sUi *ui, int lx, int ly, int *chip_out, int *island_out, int *corner_out) {
    int stack[R01S_MAX_ISLANDS];
    int nstack;
    int s;

    if (chip_out) {
        *chip_out = -1;
    }
    if (island_out) {
        *island_out = -1;
    }
    if (corner_out) {
        *corner_out = -1;
    }
    if (!ui || !ui_logic_in_view(lx, ly)) {
        return 0;
    }

    if (ui->group && !ui_layout_flat(ui)) {
        nstack = island_hit_stack(ui, stack, R01S_MAX_ISLANDS);
        for (s = 0; s < nstack; s++) {
            int ii = stack[s];
            const R01sIsland *island = r01s_island_group_at(ui->group, ii);
            int corner;
            int chip_i;
            if (!island || !hit_island_frame(ui, island, lx, ly)) {
                continue;
            }
            /* This island fully occludes anything behind it. */
            corner = hit_island_resize(ui, island, lx, ly);
            if (corner >= 0) {
                if (island_out) {
                    *island_out = ii;
                }
                if (corner_out) {
                    *corner_out = corner;
                }
                return 3;
            }
            chip_i = hit_chip_in_island(ui, ii, lx, ly);
            if (chip_i >= 0) {
                if (chip_out) {
                    *chip_out = chip_i;
                }
                if (island_out) {
                    *island_out = ii;
                }
                return 1;
            }
            if (island_out) {
                *island_out = ii;
            }
            return 2;
        }
        return 0;
    }

    /* Compact (or no islands): chips only, front-most in chip_z_order wins. */
    {
        int rank;
        int n_chips = ui->chip_z_count;
        if (n_chips <= 0) {
            n_chips = ui->chip_count;
        }
        for (rank = n_chips - 1; rank >= 0; rank--) {
            int ci = (rank < ui->chip_z_count) ? (int)ui->chip_z_order[rank] : rank;
            if (ci < 0 || ci >= ui->chip_count) {
                continue;
            }
            if (ui_chip_hidden(ui, ui->chips[ci])) {
                continue;
            }
            if (ui->chips[ci] && hit_chip(ui, ui->chips[ci], lx, ly)) {
                if (chip_out) {
                    *chip_out = ci;
                }
                return 1;
            }
        }
    }
    return 0;
}

int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y) {
    int i;
    int board_mx = 0;
    int board_my = 0;
    if (!ui || !e) {
        return 0;
    }
    ui->mouse_lx = logic_x;
    ui->mouse_ly = logic_y;
    if (ui_logic_in_view(logic_x, logic_y)) {
        ui_logic_to_board(ui, logic_x, logic_y, &board_mx, &board_my);
    }
    if (e->type == SDL_MOUSEWHEEL) {
        if (sidebar_hit(logic_x, logic_y)) {
            ui->sidebar_scroll -= e->wheel.y * R01S_UI_STATUS_ROW_H;
            sidebar_clamp_scroll(ui);
            return 1;
        }
        if (ui_logic_in_view(logic_x, logic_y)) {
            ui->pan_x -= e->wheel.x * 32;
            ui->pan_y -= e->wheel.y * 32;
            r01s_ui_clamp_pan(ui);
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        /* Chip context menu (orient); otherwise board pan. */
        if (ui_logic_in_view(logic_x, logic_y)) {
            int chip_i = -1;
            int kind = hit_board_top(ui, logic_x, logic_y, &chip_i, NULL, NULL);
            if (kind == 1 && chip_i >= 0 && chip_i < ui->chip_count && ui->chips[chip_i] &&
                ui->chips[chip_i]->visual == R01S_ENTITY_VIS_IC) {
                ui->ctx_chip = chip_i;
                ui->ctx_x = logic_x;
                ui->ctx_y = logic_y;
                if (ui_layout_flat(ui)) {
                    if (!ui->chip_sel[chip_i]) {
                        ui_sel_set_one(ui, chip_i);
                    } else {
                        ui->selected = chip_i;
                    }
                } else {
                    ui->selected = chip_i;
                }
                return 1;
            }
            ui->ctx_chip = -1;
            ui->drag_pan = 1;
            ui->drag_last_x = logic_x;
            ui->drag_last_y = logic_y;
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        e->button.button == SDL_BUTTON_MIDDLE && ui_logic_in_view(logic_x, logic_y)) {
        ui->ctx_chip = -1;
        ui->drag_pan = 1;
        ui->drag_last_x = logic_x;
        ui->drag_last_y = logic_y;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP &&
        (e->button.button == SDL_BUTTON_MIDDLE || e->button.button == SDL_BUTTON_RIGHT)) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        if (ui_logic_in_view(logic_x, logic_y) || ui_logic_in_view(ui->drag_last_x, ui->drag_last_y)) {
            ui->pan_x -= (logic_x - ui->drag_last_x);
            ui->pan_y -= (logic_y - ui->drag_last_y);
            r01s_ui_clamp_pan(ui);
        }
        ui->drag_last_x = logic_x;
        ui->drag_last_y = logic_y;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_stick >= 0) {
        gp_stick_from_point(ui, &ui->gamepad[ui->drag_stick], ui->drag_stick, logic_x, logic_y);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->resize_island >= 0) {
        resize_island_drag(ui, ui->resize_island, board_mx, board_my);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_island >= 0) {
        move_island_drag(ui, ui->drag_island, board_mx, board_my);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->box_sel) {
        ui->box_bx1 = board_mx;
        ui->box_by1 = board_my;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        if (ui_layout_flat(ui) && ui_sel_count(ui) > 1) {
            move_selection_drag(ui, board_mx, board_my);
        } else {
            move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
        }
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        const Uint8 *mods = SDL_GetKeyboardState(NULL);
        int step = 48;
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_s) {
            ui_save_layout_now(ui);
            return 1;
        }
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_r) {
            if (r01s_ui_rotate_selected(ui)) {
                return 1;
            }
        }
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_g) {
            if (r01s_board_from_group(ui->group)) {
                ui_toggle_lcd_scale(ui);
                return 1;
            }
        }
        if (mods[SDL_SCANCODE_LSHIFT] || mods[SDL_SCANCODE_RSHIFT]) {
            if (e->key.keysym.sym == SDLK_LEFT) {
                ui->pan_x -= step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHT) {
                ui->pan_x += step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_UP) {
                ui->pan_y -= step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_DOWN) {
                ui->pan_y += step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
        }
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        int was_layout_drag =
            (ui->drag_chip >= 0 || ui->drag_island >= 0 || ui->resize_island >= 0);
        if (ui->drag_stick >= 0) {
            ui->gamepad[ui->drag_stick].stick_x = 0;
            ui->gamepad[ui->drag_stick].stick_y = 0;
            ui->drag_stick = -1;
            return 1;
        }
        if (ui->drag_btn >= 0) {
            ui->mouse_btn[ui->drag_btn / 4][ui->drag_btn % 4] = 0;
            ui->drag_btn = -1;
            return 1;
        }
        if (ui->box_sel) {
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            int w = ui->box_bx1 - ui->box_bx0;
            int h = ui->box_by1 - ui->box_by0;
            if (w < 0) {
                w = -w;
            }
            if (h < 0) {
                h = -h;
            }
            ui->box_sel = 0;
            if (w >= 4 || h >= 4) {
                ui_sel_from_box(ui, shift);
                snprintf(ui->status, sizeof(ui->status), "selected %d", ui_sel_count(ui));
            } else if (!shift) {
                ui_sel_clear(ui);
            }
            return 1;
        }
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;
        if (was_layout_drag) {
            ui->layout_dirty = 1;
        }
        return ui->selected >= 0 || ui_sel_count(ui) > 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int gp_player = 0;
        int gp_btn = -1;
        int hit;

        /* Context menu: toggle package orientation. */
        if (ui->ctx_chip >= 0 && ui->ctx_chip < ui->chip_count) {
            const R01sEntity *ce = ui->chips[ui->ctx_chip];
            const char *item =
                (ce && ce->orient == R01S_ORIENT_V) ? "ORIENT HORIZONTAL" : "ORIENT VERTICAL";
            int mw = font_text_width(item) + 16;
            int mh = 22;
            int mx = ui->ctx_x;
            int my = ui->ctx_y;
            if (mx + mw > R01S_LOGIC_W - 4) {
                mx = R01S_LOGIC_W - 4 - mw;
            }
            if (my + mh > R01S_LOGIC_H - 4) {
                my = R01S_LOGIC_H - 4 - mh;
            }
            if (logic_x >= mx && logic_x < mx + mw && logic_y >= my && logic_y < my + mh) {
                ui->selected = ui->ctx_chip;
                r01s_ui_rotate_selected(ui);
                ui->ctx_chip = -1;
                return 1;
            }
            ui->ctx_chip = -1; /* click elsewhere dismisses */
        }

        /* Save layout (top HUD). */
        {
            SDL_Rect sbtn;
            save_btn_rect(ui, &sbtn);
            if (logic_x >= sbtn.x && logic_x < sbtn.x + sbtn.w && logic_y >= sbtn.y &&
                logic_y < sbtn.y + sbtn.h) {
                ui_save_layout_now(ui);
                return 1;
            }
        }

        /* Compact / Islands layout toggle (top HUD). */
        {
            SDL_Rect cbtn;
            compact_btn_rect(ui, &cbtn);
            if (logic_x >= cbtn.x && logic_x < cbtn.x + cbtn.w && logic_y >= cbtn.y &&
                logic_y < cbtn.y + cbtn.h) {
                ui_toggle_compact(ui);
                return 1;
            }
        }

        /* STATUS: copy WARN/FAIL rows or system header to clipboard. */
        if (sidebar_hit(logic_x, logic_y) && ui_health_copy_at(ui, logic_x, logic_y)) {
            return 1;
        }

        /* PROBE: gray pin stubs toggle. */
        {
            SDL_Rect rc;
            sidebar_probe_quiet_btn_rect(ui, sidebar_sy(ui, sidebar_probe_content_y(ui)), &rc);
            if (sidebar_hit(logic_x, logic_y) && radio_hit(&rc, logic_x, logic_y)) {
                ui->pins_quiet = !ui->pins_quiet;
                snprintf(ui->status, sizeof(ui->status), ui->pins_quiet ? "Pin stubs: gray (quiet)"
                                                                           : "Pin stubs: live levels");
                return 1;
            }
        }

        /* CART: show/hide flash + EEPROM. */
        if (sidebar_hit(logic_x, logic_y)) {
            SDL_Rect rc;
            sidebar_cart_btn_rect(ui, 0, &rc);
            if (radio_hit(&rc, logic_x, logic_y)) {
                ui->show_cart_flash = !ui->show_cart_flash;
                if (!ui->show_cart_flash && ui->selected >= 0 && ui->selected < ui->chip_count &&
                    ui_chip_is_cart_flash(ui->chips[ui->selected])) {
                    ui->selected = -1;
                }
                snprintf(ui->status, sizeof(ui->status),
                         ui->show_cart_flash ? "Cart FLASH (U40) shown" : "Cart FLASH (U40) hidden");
                return 1;
            }
            sidebar_cart_btn_rect(ui, 1, &rc);
            if (radio_hit(&rc, logic_x, logic_y)) {
                ui->show_cart_eeprom = !ui->show_cart_eeprom;
                if (!ui->show_cart_eeprom && ui->selected >= 0 && ui->selected < ui->chip_count &&
                    ui_chip_is_cart_eeprom(ui->chips[ui->selected])) {
                    ui->selected = -1;
                }
                snprintf(ui->status, sizeof(ui->status),
                         ui->show_cart_eeprom ? "Cart 24C64 (U50) shown" : "Cart 24C64 (U50) hidden");
                return 1;
            }
        }

        /* SCALE: 1X / 2X LCD preview. */
        if (sidebar_hit(logic_x, logic_y) && r01s_board_from_group(ui->group)) {
            SDL_Rect rc;
            sidebar_scale_btn_rect(ui, 0, &rc);
            if (radio_hit(&rc, logic_x, logic_y)) {
                ui_set_lcd_scale(ui, 0);
                return 1;
            }
            sidebar_scale_btn_rect(ui, 1, &rc);
            if (radio_hit(&rc, logic_x, logic_y)) {
                ui_set_lcd_scale(ui, 1);
                return 1;
            }
        }

        hit = gp_hit_any(ui, logic_x, logic_y, &gp_player, &gp_btn);
        if (hit == 2) {
            ui->drag_btn = gp_player * 4 + gp_btn;
            ui->mouse_btn[gp_player][gp_btn] = 1;
            return 1;
        }
        if (hit == 1) {
            ui->drag_stick = gp_player;
            gp_stick_from_point(ui, &ui->gamepad[gp_player], gp_player, logic_x, logic_y);
            return 1;
        }
        ui->selected = -1;
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;

        if (!ui_logic_in_view(logic_x, logic_y)) {
            return 1;
        }

        if (ui_layout_conn(ui) && (SDL_GetModState() & KMOD_CTRL)) {
            int bx, by;
            ui_logic_to_board(ui, logic_x, logic_y, &bx, &by);
            if (ui_conn_ctrl_click(ui, bx, by)) {
                return 1;
            }
        }

        {
            int chip_i = -1;
            int island_i = -1;
            int corner = -1;
            int kind = hit_board_top(ui, logic_x, logic_y, &chip_i, &island_i, &corner);
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

            if (kind == 3 && island_i >= 0) {
                const R01sIsland *island = r01s_island_group_at(ui->group, island_i);
                ui_sel_clear(ui);
                r01s_ui_island_z_raise(ui, island_i);
                ui->resize_island = island_i;
                ui->resize_corner = corner;
                snprintf(ui->status, sizeof(ui->status), "resize %s",
                         island && island->title ? island->title : "ISLAND");
                return 1;
            }
            if (kind == 1 && chip_i >= 0 && chip_i < ui->chip_count && ui->chips[chip_i]) {
                if (ui_layout_flat(ui)) {
                    r01s_ui_chip_z_raise(ui, chip_i);
                    ui->layout_dirty = 1;
                } else if (island_i >= 0) {
                    r01s_ui_island_z_raise(ui, island_i);
                }
                if (ui_layout_flat(ui) && shift) {
                    ui_sel_toggle(ui, chip_i);
                    snprintf(ui->status, sizeof(ui->status), "selected %d", ui_sel_count(ui));
                    return 1;
                }
                if (ui_layout_flat(ui) && ui->chip_sel[chip_i] && ui_sel_count(ui) > 1) {
                    /* Drag whole selection; keep multi-select. */
                    ui->selected = chip_i;
                    ui->drag_chip = chip_i;
                    ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                    ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                    ui_begin_sel_drag(ui, board_mx, board_my);
                    snprintf(ui->status, sizeof(ui->status), "drag %d chips", ui_sel_count(ui));
                    return 1;
                }
                if (ui_layout_flat(ui)) {
                    ui_sel_set_one(ui, chip_i);
                } else {
                    ui_sel_clear(ui);
                    ui->selected = chip_i;
                }
                ui->drag_chip = chip_i;
                ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                ui_begin_sel_drag(ui, board_mx, board_my);
                snprintf(ui->status, sizeof(ui->status), "drag %s (%s)  pins=%d",
                         ui->chips[chip_i]->refdes ? ui->chips[chip_i]->refdes : "?",
                         ui->chips[chip_i]->part ? ui->chips[chip_i]->part : "?",
                         ui->chips[chip_i]->pin_count);
                return 1;
            }
            if (kind == 2 && island_i >= 0) {
                const R01sIsland *island = r01s_island_group_at(ui->group, island_i);
                ui_sel_clear(ui);
                r01s_ui_island_z_raise(ui, island_i);
                ui->drag_island = island_i;
                ui->drag_grab_bx = board_mx - island->board_x;
                ui->drag_grab_by = board_my - island->board_y;
                snprintf(ui->status, sizeof(ui->status), "move %s",
                         island && island->title ? island->title : "ISLAND");
                return 1;
            }
            /* Compact empty board: start marquee select. */
            if (ui_layout_flat(ui)) {
                if (!shift) {
                    ui_sel_clear(ui);
                }
                ui->box_sel = 1;
                ui->box_bx0 = ui->box_bx1 = board_mx;
                ui->box_by0 = ui->box_by1 = board_my;
                return 1;
            }
            ui_sel_clear(ui);
        }
        return 1;
    }
    return 0;
}
