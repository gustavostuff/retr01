#include "ui.h"
#include "retr01_sim/ui_button.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/frame_log.h"
#include "breadboard.h"
#include "passive.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ui_canvas_zoom_by(R01sUi *ui, int delta, int lx, int ly) {
    int z0 = ui_zoom(ui);
    int z1 = z0 + delta;
    int bx;
    int by;
    if (!ui) {
        return;
    }
    if (z1 < 1) {
        z1 = 1;
    }
    if (z1 > R01S_ZOOM_MAX) {
        z1 = R01S_ZOOM_MAX;
    }
    if (z1 == z0) {
        return;
    }
    ui_logic_to_board(ui, lx, ly, &bx, &by);
    ui->zoom = z1;
    ui->pan_x = bx - (ui_div_floor(lx, z1) - R01S_UI_VIEW_X);
    ui->pan_y = by - (ui_div_floor(ly, z1) - R01S_UI_VIEW_Y);
    r01s_ui_clamp_pan(ui);
    ui->layout_dirty = 1;
    snprintf(ui->status, sizeof(ui->status), "zoom %dx", z1);
}

static void ui_pan_grab_begin(R01sUi *ui, int lx, int ly) {
    ui_logic_to_board(ui, lx, ly, &ui->drag_grab_bx, &ui->drag_grab_by);
    ui->drag_last_x = lx;
    ui->drag_last_y = ly;
}

static void ui_pan_grab_to(R01sUi *ui, int lx, int ly) {
    int z = ui_zoom(ui);
    ui->pan_x = ui->drag_grab_bx - (ui_div_floor(lx, z) - R01S_UI_VIEW_X);
    ui->pan_y = ui->drag_grab_by - (ui_div_floor(ly, z) - R01S_UI_VIEW_Y);
    ui->drag_last_x = lx;
    ui->drag_last_y = ly;
    r01s_ui_clamp_pan(ui);
    ui->layout_dirty = 1;
}

static int hit_filled(int lx, int ly, int x, int y, int w, int h) {
    return w > 0 && h > 0 && lx >= x && lx < x + w && ly >= y && ly < y + h;
}

static int hit_chip(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    return ui_part_image_hit(ui, e, lx, ly);
}

/* Selected passives draw a 1px gold rect outside the PNG. It scales with zoom. */
static int hit_selected_outline(const R01sUi *ui, const R01sEntity *e, int ci, int lx, int ly) {
    int x;
    int y;
    if (!e || e->visual != R01S_ENTITY_VIS_PASSIVE) {
        return 0;
    }
    if (ci != ui->selected && (ci < 0 || !ui->chip_sel[ci])) {
        return 0;
    }
    x = ui_board_sx(ui, e->board_x);
    y = ui_board_sy(ui, e->board_y);
    return hit_filled(lx, ly, x - 1, y - 1, e->body_w + 2, e->body_h + 2);
}

static int hit_island_frame(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    return lx >= x && lx < x + island->board_w && ly >= y && ly < y + island->board_h;
}

/* Closest corner within hs px of (lx, ly), or -1. No drawn grip. */
static int hit_rect_corner(int lx, int ly, int x, int y, int w, int h, int hs) {
    int pts[4][3];
    int i;
    int best = -1;
    int best_d = hs * hs + 1;
    pts[0][0] = x;
    pts[0][1] = y;
    pts[0][2] = R01S_ISLAND_CORNER_TL;
    pts[1][0] = x + w;
    pts[1][1] = y;
    pts[1][2] = R01S_ISLAND_CORNER_TR;
    pts[2][0] = x;
    pts[2][1] = y + h;
    pts[2][2] = R01S_ISLAND_CORNER_BL;
    pts[3][0] = x + w;
    pts[3][1] = y + h;
    pts[3][2] = R01S_ISLAND_CORNER_BR;
    for (i = 0; i < 4; i++) {
        int dx = lx - pts[i][0];
        int dy = ly - pts[i][1];
        int d;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dx > hs || dy > hs) {
            continue;
        }
        d = dx * dx + dy * dy;
        if (d < best_d) {
            best_d = d;
            best = pts[i][2];
        }
    }
    return best;
}

/* Returns corner id, or -1 if miss. */
static int hit_island_resize(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    return hit_rect_corner(lx, ly, x, y, island->board_w, island->board_h, R01S_ISLAND_RESIZE_HANDLE);
}

static int hit_floor_corner(const R01sUi *ui, int logic_x, int logic_y, int *zone_out) {
    int z;
    int lx;
    int ly;
    int i;
    const int hs = 8;
    if (zone_out) {
        *zone_out = -1;
    }
    if (!ui || !ui->floor_on) {
        return -1;
    }
    z = ui_zoom(ui);
    lx = ui_div_floor(logic_x, z);
    ly = ui_div_floor(logic_y, z);
    for (i = R01S_ZONE_COUNT - 1; i >= 0; i--) {
        int x;
        int y;
        int corner;
        if (ui->floor_w[i] <= 0 || ui->floor_h[i] <= 0) {
            continue;
        }
        x = ui_board_sx(ui, ui->floor_x[i]);
        y = ui_board_sy(ui, ui->floor_y[i]);
        corner = hit_rect_corner(lx, ly, x, y, ui->floor_w[i], ui->floor_h[i], hs);
        if (corner >= 0) {
            if (zone_out) {
                *zone_out = i;
            }
            return corner;
        }
    }
    return -1;
}

/* 0 arrow, 1 NW-SE, 2 NE-SW, 3 move. */
static void ui_set_sys_cursor(int which) {
    static SDL_Cursor *nwse;
    static SDL_Cursor *nesw;
    static SDL_Cursor *move;
    static int ready;
    static int last = -2;
    SDL_Cursor *c;
    if (which == last) {
        return;
    }
    last = which;
    if (!ready) {
        nwse = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
        nesw = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
        move = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
        ready = 1;
    }
    if (which == 1) {
        c = nwse;
    } else if (which == 2) {
        c = nesw;
    } else if (which == 3) {
        c = move;
    } else {
        c = SDL_GetDefaultCursor();
    }
    if (c) {
        SDL_SetCursor(c);
    }
}

static int cursor_for_corner(int corner) {
    if (corner == R01S_ISLAND_CORNER_TL || corner == R01S_ISLAND_CORNER_BR) {
        return 1;
    }
    if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_BL) {
        return 2;
    }
    return 0;
}

/* Topmost zone whose interior contains the point and no part. -1 otherwise. */
static int hit_floor_interior(const R01sUi *ui, int logic_x, int logic_y) {
    int z;
    int lx;
    int ly;
    int i;
    if (!ui || !ui->floor_on || !ui_logic_in_view(logic_x, logic_y)) {
        return -1;
    }
    if (hit_board_top(ui, logic_x, logic_y, NULL, NULL, NULL) == 1) {
        return -1;
    }
    z = ui_zoom(ui);
    lx = ui_div_floor(logic_x, z);
    ly = ui_div_floor(logic_y, z);
    for (i = R01S_ZONE_COUNT - 1; i >= 0; i--) {
        int x;
        int y;
        if (ui->floor_w[i] <= 0 || ui->floor_h[i] <= 0) {
            continue;
        }
        x = ui_board_sx(ui, ui->floor_x[i]);
        y = ui_board_sy(ui, ui->floor_y[i]);
        if (hit_filled(lx, ly, x, y, ui->floor_w[i], ui->floor_h[i])) {
            return i;
        }
    }
    return -1;
}

static void ui_sync_corner_cursor(const R01sUi *ui, int logic_x, int logic_y) {
    int zone = -1;
    int corner = -1;
    if (ui && ui->floor_resize >= 0) {
        ui_set_sys_cursor(cursor_for_corner(ui->floor_resize_corner));
        return;
    }
    if (ui && ui->floor_drag >= 0) {
        ui_set_sys_cursor(3);
        return;
    }
    corner = hit_floor_corner(ui, logic_x, logic_y, &zone);
    if (corner >= 0) {
        ui_set_sys_cursor(cursor_for_corner(corner));
        return;
    }
    if (!(SDL_GetModState() & KMOD_SHIFT) && hit_floor_interior(ui, logic_x, logic_y) >= 0) {
        ui_set_sys_cursor(3);
        return;
    }
    (void)zone;
    ui_set_sys_cursor(0);
}

static void floor_resize_to(R01sUi *ui, int board_mx, int board_my) {
    int corner;
    int left;
    int top;
    int right;
    int bottom;
    const int min_s = 16;
    int z;
    if (!ui || ui->floor_resize < 0 || ui->floor_resize >= R01S_ZONE_COUNT) {
        return;
    }
    z = ui->floor_resize;
    corner = ui->floor_resize_corner;
    left = ui->floor_anchor_x;
    top = ui->floor_anchor_y;
    right = ui->floor_anchor_x;
    bottom = ui->floor_anchor_y;
    if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_TR) {
        right = board_mx;
    } else {
        left = board_mx;
    }
    if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_BL) {
        bottom = board_my;
    } else {
        top = board_my;
    }
    if (right < left + min_s) {
        if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
            left = right - min_s;
        } else {
            right = left + min_s;
        }
    }
    if (bottom < top + min_s) {
        if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
            top = bottom - min_s;
        } else {
            bottom = top + min_s;
        }
    }
    ui->floor_x[z] = left;
    ui->floor_y[z] = top;
    ui->floor_w[z] = right - left;
    ui->floor_h[z] = bottom - top;
    ui->layout_dirty = 1;
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

static int entity_is_screen_sink(const R01sEntity *e) {
    return e && e->visual == R01S_ENTITY_VIS_DISPLAY && e->part && strcmp(e->part, "SCREEN_SINK") == 0;
}

/* Front-most drawn chip whose image contains the point, or -1. */
static int hit_chip_front(const R01sUi *ui, int lx, int ly) {
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
        if (ui->chips[ci] && (hit_chip(ui, ui->chips[ci], lx, ly) ||
                              hit_selected_outline(ui, ui->chips[ci], ci, lx, ly))) {
            return ci;
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
    lx = ui_div_floor(lx, ui_zoom(ui));
    ly = ui_div_floor(ly, ui_zoom(ui));

    if (ui->group && !ui->layout_compact) {
        int chip_i = hit_chip_front(ui, lx, ly);
        /* Parts are drawn above island fills, so the image wins over the frame. */
        if (chip_i >= 0) {
            if (chip_out) {
                *chip_out = chip_i;
            }
            if (island_out) {
                *island_out = (int)ui->chip_island[chip_i];
            }
            return 1;
        }
        nstack = island_hit_stack(ui, stack, R01S_MAX_ISLANDS);
        for (s = 0; s < nstack; s++) {
            int ii = stack[s];
            const R01sIsland *island = r01s_island_group_at(ui->group, ii);
            int corner;
            if (!island || !hit_island_frame(ui, island, lx, ly)) {
                continue;
            }
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
            if (ui->chips[ci] && (hit_chip(ui, ui->chips[ci], lx, ly) ||
                                  hit_selected_outline(ui, ui->chips[ci], ci, lx, ly))) {
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
    int board_mx = 0;
    int board_my = 0;
    static struct {
        int chip_i;
        Uint32 t_ms;
        int bx;
        int by;
    } scr1_dclick;
    if (!ui || !e) {
        return 0;
    }
    ui->mouse_lx = logic_x;
    ui->mouse_ly = logic_y;
    if (r01s_ui_modal_handle_event(ui, e, logic_x, logic_y)) {
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION) {
        if (logic_x != ui->tip_stable_mx || logic_y != ui->tip_stable_my) {
            ui_tip_reset(ui, logic_x, logic_y);
        }
    }
    /* An in-progress drag keeps following the cursor past the canvas edge and
     * through the letterbox. Leaving board_mx at 0 there jumps the part. */
    if (ui_logic_in_view(logic_x, logic_y) || ui->drag_chip >= 0 || ui->drag_island >= 0 ||
        ui->resize_island >= 0 || ui->box_sel || ui->floor_drag >= 0 || ui->floor_resize >= 0) {
        ui_logic_to_board(ui, logic_x, logic_y, &board_mx, &board_my);
    }
    if (e->type == SDL_MOUSEMOTION && !ui->drag_chip && !ui->box_sel && !ui->drag_pan) {
        /* Breadboard hole hover: light-blue rail highlight. */
        int i;
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *ent = ui->chips[i];
            R01sBreadboard *bb;
            R01sPbHole hole;
            if (!ent || ent->visual != R01S_ENTITY_VIS_BREADBOARD) {
                continue;
            }
            bb = (R01sBreadboard *)(void *)ent;
            if (ui_logic_in_view(logic_x, logic_y) &&
                r01s_breadboard_hit_hole(bb, board_mx, board_my, &hole)) {
                r01s_breadboard_set_hover(bb, 1, hole);
            } else {
                r01s_breadboard_clear_hover(bb);
            }
        }
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_legend_strip) {
        ui->legend_strip_x = logic_x - R01S_UI_VIEW_X - ui->drag_legend_ox;
        ui->legend_strip_y = logic_y - R01S_UI_VIEW_Y - ui->drag_legend_oy;
        ui->legend_strip_moved = 1;
        ui_legend_strip_clamp(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_islands_strip) {
        ui->islands_strip_x = logic_x - R01S_UI_VIEW_X - ui->drag_islands_ox;
        ui->islands_strip_y = logic_y - R01S_UI_VIEW_Y - ui->drag_islands_oy;
        ui->islands_strip_moved = 1;
        ui_islands_strip_clamp(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_wave_monitor) {
        ui->wave_monitor_x = logic_x - R01S_UI_VIEW_X - ui->drag_wave_ox;
        ui->wave_monitor_y = logic_y - R01S_UI_VIEW_Y - ui->drag_wave_oy;
        ui->wave_monitor_moved = 1;
        ui_wave_monitor_clamp(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT) &&
        ui_legend_strip_contains(ui, logic_x, logic_y)) {
        ui->drag_legend_strip = 1;
        ui->drag_legend_ox = logic_x - (R01S_UI_VIEW_X + ui->legend_strip_x);
        ui->drag_legend_oy = logic_y - (R01S_UI_VIEW_Y + ui->legend_strip_y);
        ui->legend_strip_moved = 0;
        ui->ctx_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT) &&
        ui_islands_strip_contains(ui, logic_x, logic_y)) {
        ui->drag_islands_strip = 1;
        ui->drag_islands_ox = logic_x - (R01S_UI_VIEW_X + ui->islands_strip_x);
        ui->drag_islands_oy = logic_y - (R01S_UI_VIEW_Y + ui->islands_strip_y);
        ui->islands_strip_moved = 0;
        ui->ctx_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT) &&
        ui_wave_monitor_contains(ui, logic_x, logic_y)) {
        ui->drag_wave_monitor = 1;
        ui->drag_wave_ox = logic_x - (R01S_UI_VIEW_X + ui->wave_monitor_x);
        ui->drag_wave_oy = logic_y - (R01S_UI_VIEW_Y + ui->wave_monitor_y);
        ui->wave_monitor_moved = 0;
        ui->ctx_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEWHEEL) {
        int dy = e->wheel.y;
        int dx = e->wheel.x;
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            dy = -dy;
            dx = -dx;
        }
        if (ui_logic_in_view(logic_x, logic_y)) {
            int step;
            if (SDL_GetModState() & KMOD_CTRL) {
                if (dy > 0) {
                    ui_canvas_zoom_by(ui, 1, logic_x, logic_y);
                } else if (dy < 0) {
                    ui_canvas_zoom_by(ui, -1, logic_x, logic_y);
                }
                return 1;
            }
            step = 32 / ui_zoom(ui);
            if (step < 1) {
                step = 1;
            }
            ui->pan_x -= dx * step;
            ui->pan_y -= dy * step;
            r01s_ui_clamp_pan(ui);
            ui->layout_dirty = 1;
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        /* Chip context menu (orient); otherwise board pan. */
        if (ui_logic_in_view(logic_x, logic_y)) {
            int chip_i = -1;
            int kind = hit_board_top(ui, logic_x, logic_y, &chip_i, NULL, NULL);
            if (kind == 1 && chip_i >= 0 && chip_i < ui->chip_count && ui->chips[chip_i] &&
                (ui->chips[chip_i]->visual == R01S_ENTITY_VIS_IC ||
                 ui->chips[chip_i]->visual == R01S_ENTITY_VIS_PASSIVE ||
                 ui->chips[chip_i]->visual == R01S_ENTITY_VIS_BREADBOARD)) {
                ui->ctx_chip = chip_i;
                ui->ctx_x = logic_x;
                ui->ctx_y = logic_y;
                if (ui->layout_compact) {
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
            ui_pan_grab_begin(ui, logic_x, logic_y);
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        e->button.button == SDL_BUTTON_MIDDLE && ui_logic_in_view(logic_x, logic_y)) {
        ui->ctx_chip = -1;
        ui->drag_pan = 1;
        ui_pan_grab_begin(ui, logic_x, logic_y);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP &&
        (e->button.button == SDL_BUTTON_LEFT || e->button.button == SDL_BUTTON_RIGHT)) {
        if (ui->drag_legend_strip) {
            int moved = ui->legend_strip_moved;
            ui->drag_legend_strip = 0;
            if (moved) {
                ui->layout_dirty = 1;
            }
            return 1;
        }
        if (ui->drag_islands_strip) {
            int moved = ui->islands_strip_moved;
            ui->drag_islands_strip = 0;
            if (moved) {
                ui->layout_dirty = 1;
            } else if (e->button.button == SDL_BUTTON_LEFT) {
                ui_health_copy_at(ui, logic_x, logic_y);
            }
            return 1;
        }
        if (ui->drag_wave_monitor) {
            int moved = ui->wave_monitor_moved;
            ui->drag_wave_monitor = 0;
            if (moved) {
                ui->layout_dirty = 1;
            }
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONUP &&
        (e->button.button == SDL_BUTTON_MIDDLE || e->button.button == SDL_BUTTON_RIGHT)) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->floor_resize >= 0) {
        floor_resize_to(ui, board_mx, board_my);
        ui_set_sys_cursor(cursor_for_corner(ui->floor_resize_corner));
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->floor_drag >= 0) {
        floor_zone_drag_to(ui, ui->floor_drag, board_mx, board_my);
        ui_set_sys_cursor(3);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        if (ui_logic_in_view(logic_x, logic_y) || ui_logic_in_view(ui->drag_last_x, ui->drag_last_y)) {
            ui_pan_grab_to(ui, logic_x, logic_y);
        }
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
        if (ui->layout_compact && ui_sel_count(ui) > 1) {
            move_selection_drag(ui, board_mx, board_my);
        } else {
            move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
        }
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        const Uint8 *mods = SDL_GetKeyboardState(NULL);
        int step = 48 / ui_zoom(ui);
        if (step < 1) {
            step = 1;
        }
        if (r01s_frame_log_enabled()) {
            if (e->key.keysym.sym == SDLK_LEFTBRACKET || e->key.keysym.sym == SDLK_PAGEUP) {
                r01s_frame_log_page_delta(-1);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHTBRACKET || e->key.keysym.sym == SDLK_PAGEDOWN) {
                r01s_frame_log_page_delta(1);
                return 1;
            }
        }
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_s) {
            ui_save_layout_now(ui);
            return 1;
        }
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_r) {
            if (r01s_ui_rotate_selected(ui)) {
                return 1;
            }
        }
        if ((e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_PERIOD) {
            if (ui_sort_compact_by_type(ui)) {
                return 1;
            }
            snprintf(ui->status, sizeof(ui->status), "Ctrl+. sort only in compact view");
            return 1;
        }
        if ((e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_z) {
            if (ui_undo_compact_pose(ui)) {
                return 1;
            }
            snprintf(ui->status, sizeof(ui->status), "nothing to undo");
            return 1;
        }
        if (mods[SDL_SCANCODE_LSHIFT] || mods[SDL_SCANCODE_RSHIFT]) {
            if (e->key.keysym.sym == SDLK_LEFT) {
                ui->pan_x -= step;
                r01s_ui_clamp_pan(ui);
                ui->layout_dirty = 1;
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHT) {
                ui->pan_x += step;
                r01s_ui_clamp_pan(ui);
                ui->layout_dirty = 1;
                return 1;
            }
            if (e->key.keysym.sym == SDLK_UP) {
                ui->pan_y -= step;
                r01s_ui_clamp_pan(ui);
                ui->layout_dirty = 1;
                return 1;
            }
            if (e->key.keysym.sym == SDLK_DOWN) {
                ui->pan_y += step;
                r01s_ui_clamp_pan(ui);
                ui->layout_dirty = 1;
                return 1;
            }
        }
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        int was_layout_drag;
        if (ui->floor_resize >= 0 || ui->floor_drag >= 0) {
            ui->floor_resize = -1;
            ui->floor_drag = -1;
            ui_sync_corner_cursor(ui, logic_x, logic_y);
            return 1;
        }
        was_layout_drag = (ui->drag_chip >= 0 || ui->drag_island >= 0 || ui->resize_island >= 0);
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
        if (ui->drag_chip >= 0 && ui->drag_chip < ui->chip_count &&
            entity_is_screen_sink(ui->chips[ui->drag_chip])) {
            R01sEntity *ent = ui->chips[ui->drag_chip];
            int moved =
                ent->board_x != ui->drag_chip_start_bx || ent->board_y != ui->drag_chip_start_by;
            if (!moved) {
                Uint32 now = SDL_GetTicks();
                int dx = board_mx - scr1_dclick.bx;
                int dy = board_my - scr1_dclick.by;
                if (dx < 0) {
                    dx = -dx;
                }
                if (dy < 0) {
                    dy = -dy;
                }
                if (scr1_dclick.chip_i == ui->drag_chip && scr1_dclick.t_ms != 0 &&
                    now - scr1_dclick.t_ms <= 400u && dx <= 6 && dy <= 6) {
                    ui_toggle_lcd_scale(ui);
                    scr1_dclick.t_ms = 0;
                } else {
                    scr1_dclick.chip_i = ui->drag_chip;
                    scr1_dclick.t_ms = now;
                    scr1_dclick.bx = board_mx;
                    scr1_dclick.by = board_my;
                }
            } else {
                scr1_dclick.t_ms = 0;
            }
        }
        if (was_layout_drag) {
            if (!ui->floor_on) {
                if (ui_sel_count(ui) > 0) {
                    ui_sel_snap_to_breadboard(ui);
                } else if (ui->drag_chip >= 0) {
                    ui_chip_snap_to_breadboard(ui, ui->drag_chip);
                }
            }
            ui->layout_dirty = 1;
        }
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;
        return ui->selected >= 0 || ui_sel_count(ui) > 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        /* Context menu: rotate package 90 deg CW. */
        if (ui->ctx_chip >= 0 && ui->ctx_chip < ui->chip_count) {
            const char *item = "ROTATE 90 CW";
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

        if (ui->floor_on) {
            int zone = -1;
            int corner = hit_floor_corner(ui, logic_x, logic_y, &zone);
            if (corner >= 0 && zone >= 0) {
                ui->floor_resize = zone;
                ui->floor_resize_corner = corner;
                if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_TR) {
                    ui->floor_anchor_x = ui->floor_x[zone];
                } else {
                    ui->floor_anchor_x = ui->floor_x[zone] + ui->floor_w[zone];
                }
                if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_BL) {
                    ui->floor_anchor_y = ui->floor_y[zone];
                } else {
                    ui->floor_anchor_y = ui->floor_y[zone] + ui->floor_h[zone];
                }
                ui_set_sys_cursor(cursor_for_corner(corner));
                return 1;
            }
            if (!(SDL_GetModState() & KMOD_SHIFT)) {
                int body = hit_floor_interior(ui, logic_x, logic_y);
                if (body >= 0) {
                    ui->floor_drag = body;
                    ui->floor_drag_grab_x = board_mx - ui->floor_x[body];
                    ui->floor_drag_grab_y = board_my - ui->floor_y[body];
                    floor_zone_drag_begin(ui, body);
                    ui_set_sys_cursor(3);
                    snprintf(ui->status, sizeof(ui->status), "move %s", r01s_air_zone_name(body));
                    return 1;
                }
            }
        }

        /* Compact / Islands toggle removed from UI (always compact). */

        ui->selected = -1;
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;

        if (!ui_logic_in_view(logic_x, logic_y)) {
            return 1;
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
                if (ui->chips[chip_i]->visual == R01S_ENTITY_VIS_BUTTON) {
                    r01s_ui_button_press((R01sUiButton *)ui->chips[chip_i]);
                    snprintf(ui->status, sizeof(ui->status), "button %s",
                             ui->chips[chip_i]->refdes ? ui->chips[chip_i]->refdes : "?");
                    return 1;
                }
                r01s_ui_chip_z_raise(ui, chip_i);
                ui->layout_dirty = 1;
                if (!ui->layout_compact && island_i >= 0) {
                    r01s_ui_island_z_raise(ui, island_i);
                }
                if (ui->layout_compact && shift) {
                    ui_sel_toggle(ui, chip_i);
                    snprintf(ui->status, sizeof(ui->status), "selected %d", ui_sel_count(ui));
                    return 1;
                }
                if (ui->layout_compact && ui->chip_sel[chip_i] && ui_sel_count(ui) > 1) {
                    /* Drag whole selection; keep multi-select. */
                    ui->selected = chip_i;
                    ui->drag_chip = chip_i;
                    ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                    ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                    ui->drag_chip_start_bx = ui->chips[chip_i]->board_x;
                    ui->drag_chip_start_by = ui->chips[chip_i]->board_y;
                    ui_begin_sel_drag(ui, board_mx, board_my);
                    snprintf(ui->status, sizeof(ui->status), "drag %d chips", ui_sel_count(ui));
                    return 1;
                }
                if (ui->layout_compact) {
                    ui_sel_set_one(ui, chip_i);
                } else {
                    ui_sel_clear(ui);
                    ui->selected = chip_i;
                }
                ui->drag_chip = chip_i;
                ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                ui->drag_chip_start_bx = ui->chips[chip_i]->board_x;
                ui->drag_chip_start_by = ui->chips[chip_i]->board_y;
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
            if (ui->layout_compact) {
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
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip < 0 && ui->drag_island < 0 && ui->resize_island < 0 &&
        ui->floor_drag < 0 && ui->floor_resize < 0 && !ui->drag_pan && !ui->box_sel) {
        ui_sync_corner_cursor(ui, logic_x, logic_y);
    }
    return 0;
}
