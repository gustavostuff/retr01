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

/* Internal placement helpers (defined later in this file). */
static void ui_row_place_islands(R01sUi *ui);
static void ui_pack_island_chips(R01sUi *ui, int island_index);
static void ui_tighten_island_to_chips(R01sUi *ui, int island_index);
static int island_chips_overlap(const R01sUi *ui, int island_index);
static int island_saved_chip_layout_sane(const R01sUi *ui, int island_index);
static void island_expand_for_saved_chips(R01sUi *ui, int island_index);
static void island_content_min_size(const R01sUi *ui, int island_index, int *min_w, int *min_h);

static void clamp_chip_to_board(R01sEntity *e) {
    int min_x = R01S_CHIP_PIN_OUT;
    int min_y = R01S_CHIP_PIN_OUT;
    int max_x;
    int max_y;
    int bx, by;

    if (!e) {
        return;
    }
    max_x = R01S_BOARD_W - R01S_CHIP_PIN_OUT - e->body_w;
    max_y = R01S_BOARD_H - R01S_CHIP_PIN_OUT - e->body_h;
    if (max_x < min_x) {
        max_x = min_x;
    }
    if (max_y < min_y) {
        max_y = min_y;
    }
    bx = e->board_x;
    by = e->board_y;
    if (bx < min_x) {
        bx = min_x;
    }
    if (by < min_y) {
        by = min_y;
    }
    if (bx > max_x) {
        bx = max_x;
    }
    if (by > max_y) {
        by = max_y;
    }
    bx = r01s_grid_snap(bx);
    by = r01s_grid_snap(by);
    if (bx < min_x) {
        bx = r01s_grid_snap_up(min_x);
    }
    if (by < min_y) {
        by = r01s_grid_snap_up(min_y);
    }
    if (bx > max_x) {
        bx = r01s_grid_snap(max_x);
    }
    if (by > max_y) {
        by = r01s_grid_snap(max_y);
    }
    r01s_entity_place(e, bx, by);
}

static void clamp_chip_in_island(R01sUi *ui, R01sEntity *e, int island_index) {
    R01sIsland *island;
    int min_x, min_y, max_x, max_y;
    int bx, by;

    if (!ui || !e) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    min_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
    min_y = island->board_y + R01S_ISLAND_PAD_TOP;
    max_x = island->board_x + island->board_w - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT - e->body_w;
    max_y = island->board_y + island->board_h - R01S_ISLAND_PAD_BOTTOM - e->body_h;
    /* Grow the frame instead of collapsing every chip onto the same center. */
    if (max_x < min_x) {
        int need = r01s_grid_snap_up(e->body_w + 2 * R01S_CHIP_PIN_OUT + 2 * R01S_ISLAND_PAD_X);
        if (island->board_w < need) {
            island->board_w = need;
        }
        min_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
        max_x = island->board_x + island->board_w - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT - e->body_w;
        if (max_x < min_x) {
            min_x = max_x = island->board_x + (island->board_w - e->body_w) / 2;
        }
    }
    if (max_y < min_y) {
        int need = r01s_grid_snap_up(e->body_h + R01S_ISLAND_PAD_TOP + R01S_ISLAND_PAD_BOTTOM);
        if (island->board_h < need) {
            island->board_h = need;
        }
        min_y = island->board_y + R01S_ISLAND_PAD_TOP;
        max_y = island->board_y + island->board_h - R01S_ISLAND_PAD_BOTTOM - e->body_h;
        if (max_y < min_y) {
            min_y = max_y = island->board_y + (island->board_h - e->body_h) / 2;
        }
    }
    bx = e->board_x;
    by = e->board_y;
    if (bx < min_x) {
        bx = min_x;
    }
    if (by < min_y) {
        by = min_y;
    }
    if (bx > max_x) {
        bx = max_x;
    }
    if (by > max_y) {
        by = max_y;
    }
    bx = r01s_grid_snap(bx);
    by = r01s_grid_snap(by);
    if (bx < min_x) {
        bx = r01s_grid_snap_up(min_x);
    }
    if (by < min_y) {
        by = r01s_grid_snap_up(min_y);
    }
    if (bx > max_x) {
        bx = r01s_grid_snap(max_x);
    }
    if (by > max_y) {
        by = r01s_grid_snap(max_y);
    }
    r01s_entity_place(e, bx, by);
}

void clamp_chip(R01sUi *ui, R01sEntity *e, int island_index) {
    if (!ui) {
        return;
    }
    if (ui->layout_compact) {
        clamp_chip_to_board(e);
    } else {
        clamp_chip_in_island(ui, e, island_index);
    }
}

void move_chip_drag(R01sUi *ui, int chip_i, int board_mx, int board_my) {
    R01sEntity *e = ui->chips[chip_i];
    r01s_entity_place(e, board_mx - ui->drag_grab_bx, board_my - ui->drag_grab_by);
    clamp_chip(ui, e, ui->chip_island[chip_i]);
}

void ui_sel_clear(R01sUi *ui) {
    if (!ui) {
        return;
    }
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    ui->selected = -1;
}

int ui_sel_count(const R01sUi *ui) {
    int i;
    int n = 0;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_sel[i]) {
            n++;
        }
    }
    return n;
}

void ui_sel_set_one(R01sUi *ui, int chip_i) {
    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    ui->chip_sel[chip_i] = 1;
    ui->selected = chip_i;
}

void ui_sel_toggle(R01sUi *ui, int chip_i) {
    int i;
    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    ui->chip_sel[chip_i] = ui->chip_sel[chip_i] ? 0 : 1;
    if (ui->chip_sel[chip_i]) {
        ui->selected = chip_i;
        return;
    }
    if (ui->selected == chip_i) {
        ui->selected = -1;
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chip_sel[i]) {
                ui->selected = i;
                break;
            }
        }
    }
}

static int chip_board_intersects_box(const R01sEntity *e, int x0, int y0, int x1, int y1) {
    int l, t, r, b;
    int el, et, er, eb;
    if (!e || e->visual == R01S_ENTITY_VIS_NONE) {
        return 0;
    }
    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    el = e->board_x;
    et = e->board_y;
    er = e->board_x + e->body_w;
    eb = e->board_y + e->body_h;
    l = x0;
    t = y0;
    r = x1;
    b = y1;
    return el < r && er > l && et < b && eb > t;
}

void ui_sel_from_box(R01sUi *ui, int additive) {
    int i;
    int first = -1;
    if (!ui) {
        return;
    }
    if (!additive) {
        memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
        ui->selected = -1;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!chip_board_intersects_box(e, ui->box_bx0, ui->box_by0, ui->box_bx1, ui->box_by1)) {
            continue;
        }
        ui->chip_sel[i] = 1;
        if (first < 0) {
            first = i;
        }
    }
    if (first >= 0) {
        ui->selected = first;
    } else if (!additive) {
        ui->selected = -1;
    }
}

void ui_begin_sel_drag(R01sUi *ui, int board_mx, int board_my) {
    int i;
    if (!ui) {
        return;
    }
    ui->sel_drag_ox = board_mx;
    ui->sel_drag_oy = board_my;
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        ui->sel_start_x[i] = e ? e->board_x : 0;
        ui->sel_start_y[i] = e ? e->board_y : 0;
    }
}

void move_selection_drag(R01sUi *ui, int board_mx, int board_my) {
    int i;
    int dx;
    int dy;
    int dx_lo = -0x3fffffff;
    int dx_hi = 0x3fffffff;
    int dy_lo = -0x3fffffff;
    int dy_hi = 0x3fffffff;
    int any = 0;

    if (!ui) {
        return;
    }
    dx = r01s_grid_snap(board_mx - ui->sel_drag_ox);
    dy = r01s_grid_snap(board_my - ui->sel_drag_oy);

    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        int min_x, min_y, max_x, max_y;
        if (!ui->chip_sel[i]) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        any = 1;
        min_x = R01S_CHIP_PIN_OUT;
        min_y = R01S_CHIP_PIN_OUT;
        max_x = R01S_BOARD_W - R01S_CHIP_PIN_OUT - e->body_w;
        max_y = R01S_BOARD_H - R01S_CHIP_PIN_OUT - e->body_h;
        if (max_x < min_x) {
            max_x = min_x;
        }
        if (max_y < min_y) {
            max_y = min_y;
        }
        if (min_x - ui->sel_start_x[i] > dx_lo) {
            dx_lo = min_x - ui->sel_start_x[i];
        }
        if (max_x - ui->sel_start_x[i] < dx_hi) {
            dx_hi = max_x - ui->sel_start_x[i];
        }
        if (min_y - ui->sel_start_y[i] > dy_lo) {
            dy_lo = min_y - ui->sel_start_y[i];
        }
        if (max_y - ui->sel_start_y[i] < dy_hi) {
            dy_hi = max_y - ui->sel_start_y[i];
        }
    }
    if (!any) {
        return;
    }
    if (dx < dx_lo) {
        dx = dx_lo;
    }
    if (dx > dx_hi) {
        dx = dx_hi;
    }
    if (dy < dy_lo) {
        dy = dy_lo;
    }
    if (dy > dy_hi) {
        dy = dy_hi;
    }
    dx = r01s_grid_snap(dx);
    dy = r01s_grid_snap(dy);
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        if (!ui->chip_sel[i]) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        r01s_entity_place(e, ui->sel_start_x[i] + dx, ui->sel_start_y[i] + dy);
    }
}

typedef struct {
    int idx;
    int pw;
    int ph;
} R01sPackItem;

static void chip_pack_footprint(const R01sEntity *e, int *pw, int *ph) {
    if (!e || !pw || !ph) {
        return;
    }
    /* Pin stubs stick out of the long sides of a DIP (H: L/R, V: T/B). */
    if (e->visual == R01S_ENTITY_VIS_IC && e->orient == R01S_ORIENT_V) {
        *pw = e->body_w;
        *ph = e->body_h + 2 * R01S_CHIP_PIN_OUT;
    } else {
        *pw = e->body_w + 2 * R01S_CHIP_PIN_OUT;
        *ph = e->body_h;
    }
    if (*pw < 1) {
        *pw = 1;
    }
    if (*ph < 1) {
        *ph = 1;
    }
}

static int pack_item_taller(const void *a, const void *b) {
    const R01sPackItem *pa = a;
    const R01sPackItem *pb = b;
    if (pb->ph != pa->ph) {
        return pb->ph - pa->ph;
    }
    return pb->pw - pa->pw;
}

static int ui_isqrt(int n) {
    int x;
    int y;
    if (n <= 0) {
        return 0;
    }
    x = n;
    y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/* Shelf-pack items into rows capped at max_row_w; write board positions into out_x/out_y. */
static void pack_shelves(const R01sPackItem *items, int n, int max_row_w, int gap, int origin_x, int origin_y,
                         int *out_x, int *out_y, int *bb_w, int *bb_h) {
    int i;
    int x = origin_x;
    int y = origin_y;
    int row_h = 0;
    int max_x = origin_x;
    int max_y = origin_y;

    for (i = 0; i < n; i++) {
        int pw = items[i].pw;
        int ph = items[i].ph;
        if (i > 0 && x > origin_x && x + pw > origin_x + max_row_w) {
            x = origin_x;
            y += row_h + gap;
            row_h = 0;
        }
        out_x[i] = x;
        out_y[i] = y;
        if (x + pw > max_x) {
            max_x = x + pw;
        }
        if (y + ph > max_y) {
            max_y = y + ph;
        }
        if (ph > row_h) {
            row_h = ph;
        }
        x += pw + gap;
    }
    *bb_w = max_x - origin_x;
    *bb_h = max_y - origin_y;
}

static void ui_save_island_layout(R01sUi *ui) {
    r01s_ui_snapshot_island_layout(ui);
}

static void ui_chip_rel_from_abs(const R01sUi *ui, int chip_i, int abs_x, int abs_y, int *rx, int *ry) {
    const R01sIsland *island;
    if (!ui || !ui->group || chip_i < 0 || chip_i >= ui->chip_count || !rx || !ry) {
        if (rx) {
            *rx = abs_x;
        }
        if (ry) {
            *ry = abs_y;
        }
        return;
    }
    island = r01s_island_group_at(ui->group, ui->chip_island[chip_i]);
    if (!island) {
        *rx = abs_x;
        *ry = abs_y;
        return;
    }
    *rx = abs_x - island->board_x;
    *ry = abs_y - island->board_y;
}

static void ui_chip_place_rel(R01sUi *ui, int chip_i, int rx, int ry) {
    R01sEntity *e;
    const R01sIsland *island;
    if (!ui || !ui->group || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    island = r01s_island_group_at(ui->group, ui->chip_island[chip_i]);
    if (!e || !island) {
        return;
    }
    r01s_entity_place(e, island->board_x + rx, island->board_y + ry);
    clamp_chip_in_island(ui, e, ui->chip_island[chip_i]);
}

/* Place at exact island-relative coords — used for faithful load (no clamp/grow). */
static void ui_chip_place_rel_exact(R01sUi *ui, int chip_i, int rx, int ry) {
    R01sEntity *e;
    const R01sIsland *island;
    if (!ui || !ui->group || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    island = r01s_island_group_at(ui->group, ui->chip_island[chip_i]);
    if (!e || !island) {
        return;
    }
    r01s_entity_place(e, island->board_x + rx, island->board_y + ry);
}

void r01s_ui_snapshot_island_layout(R01sUi *ui) {
    int i;

    if (!ui || !ui->group) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!e) {
            ui->save_chip_x[i] = 0;
            ui->save_chip_y[i] = 0;
            ui->save_chip_orient[i] = (uint8_t)R01S_ORIENT_H;
            continue;
        }
        ui_chip_rel_from_abs(ui, i, e->board_x, e->board_y, &ui->save_chip_x[i], &ui->save_chip_y[i]);
        ui->save_chip_orient[i] = (uint8_t)e->orient;
    }
    r01s_ui_snapshot_island_frames(ui);
    ui->layout_saved = 1;
}

void r01s_ui_snapshot_island_frames(R01sUi *ui) {
    int i;
    int n_islands;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        const R01sIsland *island = r01s_island_group_at(ui->group, i);
        if (!island) {
            continue;
        }
        if (island->board_w > 0 && island->board_h > 0) {
            ui->save_island_x[i] = island->board_x;
            ui->save_island_y[i] = island->board_y;
            ui->save_island_w[i] = island->board_w;
            ui->save_island_h[i] = island->board_h;
        }
    }
}

static int ui_island_snapshot_valid(const R01sUi *ui) {
    int i;
    int n_islands;
    if (!ui || !ui->group) {
        return 0;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        if (ui->save_island_w[i] > 0 && ui->save_island_h[i] > 0) {
            return 1;
        }
    }
    return 0;
}

static void ui_arrange_islands_default(R01sUi *ui) {
    int n_islands;
    int i;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    /*
     * Pack chips into each island first. Never size frames from whatever absolute
     * chip positions happen to be live (e.g. compact-mode coords) — that creates
     * huge overlapping islands.
     */
    for (i = 0; i < n_islands; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        if (!island) {
            continue;
        }
        island->board_x = 0;
        island->board_y = 0;
        island->board_w = R01S_ISLAND_MIN_W;
        island->board_h = R01S_ISLAND_MIN_H;
        ui_pack_island_chips(ui, i);
    }
    ui_row_place_islands(ui);
}

/* Place already-sized islands in wrapping rows; chips move with their frame. */
static void ui_row_place_islands(R01sUi *ui) {
    int n_islands;
    int start_x = 40;
    int start_y = 40;
    int x = start_x;
    int y = start_y;
    int row_h = 0;
    int limit = start_x + R01S_ISLAND_ROW_MAX_W;
    int i;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        int dx;
        int dy;
        int j;

        if (!island) {
            continue;
        }
        if (island->board_w < R01S_ISLAND_MIN_W) {
            island->board_w = R01S_ISLAND_MIN_W;
        }
        if (island->board_h < R01S_ISLAND_MIN_H) {
            island->board_h = R01S_ISLAND_MIN_H;
        }
        island->board_w = r01s_grid_snap_up(island->board_w);
        island->board_h = r01s_grid_snap_up(island->board_h);
        if (i > 0 && x > start_x && x + island->board_w > limit) {
            x = start_x;
            y += row_h + R01S_ISLAND_GAP;
            row_h = 0;
        }
        x = r01s_grid_snap(x);
        y = r01s_grid_snap(y);
        dx = x - island->board_x;
        dy = y - island->board_y;
        island->board_x = x;
        island->board_y = y;
        for (j = 0; j < ui->chip_count; j++) {
            R01sEntity *e = ui->chips[j];
            if (!e || ui->chip_island[j] != (uint8_t)i) {
                continue;
            }
            r01s_entity_place(e, e->board_x + dx, e->board_y + dy);
        }
        if (island->board_h > row_h) {
            row_h = island->board_h;
        }
        x += island->board_w + R01S_ISLAND_GAP;
    }
}

/*
 * Recover island frames when islands[] was missing/empty but island_chips look
 * like valid island-relative placements (common corrupt compact save).
 */
static void ui_rebuild_islands_from_saved_chips(R01sUi *ui) {
    int n_islands;
    int i;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        int j;
        if (!island) {
            continue;
        }
        if (!island_saved_chip_layout_sane(ui, i)) {
            island->board_x = 0;
            island->board_y = 0;
            island->board_w = R01S_ISLAND_MIN_W;
            island->board_h = R01S_ISLAND_MIN_H;
            ui_pack_island_chips(ui, i);
            continue;
        }
        island->board_x = 0;
        island->board_y = 0;
        island->board_w = R01S_ISLAND_MIN_W;
        island->board_h = R01S_ISLAND_MIN_H;
        for (j = 0; j < ui->chip_count; j++) {
            R01sEntity *e = ui->chips[j];
            if (!e || ui->chip_island[j] != (uint8_t)i) {
                continue;
            }
            if (e->visual == R01S_ENTITY_VIS_IC) {
                r01s_entity_set_orient(e, (R01sPkgOrient)ui->save_chip_orient[j]);
            }
        }
        island_expand_for_saved_chips(ui, i);
        for (j = 0; j < ui->chip_count; j++) {
            if (ui->chip_island[j] != (uint8_t)i) {
                continue;
            }
            ui_chip_place_rel(ui, j, ui->save_chip_x[j], ui->save_chip_y[j]);
        }
        ui_tighten_island_to_chips(ui, i);
        if (island_chips_overlap(ui, i)) {
            ui_pack_island_chips(ui, i);
        }
    }
    ui_row_place_islands(ui);
}

/* Fit frame exactly to chip content (shrinks wasted empty space). */
static void ui_tighten_island_to_chips(R01sUi *ui, int island_index) {
    R01sIsland *island;
    int min_w;
    int min_h;

    if (!ui || !ui->group) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    island_content_min_size(ui, island_index, &min_w, &min_h);
    if (min_w < R01S_ISLAND_MIN_W) {
        min_w = R01S_ISLAND_MIN_W;
    }
    if (min_h < R01S_ISLAND_MIN_H) {
        min_h = R01S_ISLAND_MIN_H;
    }
    island->board_w = r01s_grid_snap_up(min_w);
    island->board_h = r01s_grid_snap_up(min_h);
}

/* Shelf-pack chips inside one island so they never share the same cell. */
static void ui_pack_island_chips(R01sUi *ui, int island_index) {
    R01sIsland *island;
    R01sPackItem items[R01S_BOARD_MAX_CHIPS];
    int place_x[R01S_BOARD_MAX_CHIPS];
    int place_y[R01S_BOARD_MAX_CHIPS];
    int n = 0;
    int i;
    int origin_x;
    int origin_y;
    int max_row;
    int bb_w = 0;
    int bb_h = 0;

    if (!ui || !ui->group) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!e || ui->chip_island[i] != (uint8_t)island_index || e->visual == R01S_ENTITY_VIS_NONE ||
            e->body_w <= 0 || e->body_h <= 0) {
            continue;
        }
        items[n].idx = i;
        chip_pack_footprint(e, &items[n].pw, &items[n].ph);
        n++;
    }
    if (n == 0) {
        return;
    }
    qsort(items, (size_t)n, sizeof(items[0]), pack_item_taller);
    origin_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
    origin_y = island->board_y + R01S_ISLAND_PAD_TOP;
    max_row = island->board_w - 2 * R01S_ISLAND_PAD_X - 2 * R01S_CHIP_PIN_OUT;
    if (max_row < items[0].pw) {
        max_row = items[0].pw;
    }
    pack_shelves(items, n, max_row, R01S_CHIP_GAP, origin_x, origin_y, place_x, place_y, &bb_w, &bb_h);
    for (i = 0; i < n; i++) {
        R01sEntity *e = ui->chips[items[i].idx];
        int bx = place_x[i];
        int by = place_y[i];
        int pw, ph;
        if (!e) {
            continue;
        }
        chip_pack_footprint(e, &pw, &ph);
        bx += (pw - e->body_w) / 2;
        by += (ph - e->body_h) / 2;
        r01s_entity_place(e, r01s_grid_snap(bx), r01s_grid_snap(by));
    }
    ui_tighten_island_to_chips(ui, island_index);
}

static int entity_bodies_overlap(const R01sEntity *a, const R01sEntity *b) {
    if (!a || !b) {
        return 0;
    }
    return a->board_x < b->board_x + b->body_w && a->board_x + a->body_w > b->board_x &&
           a->board_y < b->board_y + b->body_h && a->board_y + a->body_h > b->board_y;
}

static int island_chips_overlap(const R01sUi *ui, int island_index) {
    int i, j;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *a;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        a = ui->chips[i];
        if (!a || a->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        for (j = i + 1; j < ui->chip_count; j++) {
            const R01sEntity *b;
            if (ui->chip_island[j] != (uint8_t)island_index) {
                continue;
            }
            b = ui->chips[j];
            if (!b || b->visual == R01S_ENTITY_VIS_NONE) {
                continue;
            }
            if (entity_bodies_overlap(a, b)) {
                return 1;
            }
        }
    }
    return 0;
}

static int island_saved_positions_degenerate(const R01sUi *ui, int island_index) {
    int i;
    int n = 0;
    int fx = 0;
    int fy = 0;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (!ui->chips[i] || ui->chips[i]->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        if (n == 0) {
            fx = ui->save_chip_x[i];
            fy = ui->save_chip_y[i];
        } else if (ui->save_chip_x[i] != fx || ui->save_chip_y[i] != fy) {
            return 0;
        }
        n++;
    }
    return n > 1;
}

/*
 * Island-relative chip saves sometimes get corrupted into absolute board coords
 * (ry in the thousands). Reject those so we re-pack instead of inflating frames.
 */
static int island_saved_chip_layout_sane(const R01sUi *ui, int island_index) {
    int i;
    int n = 0;
    /* One island's content should stay well under the board row wrap width. */
    const int max_rel = R01S_ISLAND_ROW_MAX_W;

    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (!ui->chips[i] || ui->chips[i]->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        n++;
        if (ui->save_chip_x[i] < 0 || ui->save_chip_y[i] < 0) {
            return 0;
        }
        if (ui->save_chip_x[i] > max_rel || ui->save_chip_y[i] > max_rel) {
            return 0;
        }
    }
    if (n == 0) {
        return 1;
    }
    return !island_saved_positions_degenerate(ui, island_index);
}

static void island_expand_for_saved_chips(R01sUi *ui, int island_index) {
    R01sIsland *island;
    int i;
    int need_w = R01S_ISLAND_MIN_W;
    int need_h = R01S_ISLAND_MIN_H;

    if (!ui || !ui->group) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        int right;
        int bottom;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        right = ui->save_chip_x[i] + e->body_w + R01S_CHIP_PIN_OUT + R01S_ISLAND_PAD_X;
        bottom = ui->save_chip_y[i] + e->body_h + R01S_ISLAND_PAD_BOTTOM;
        if (right > need_w) {
            need_w = right;
        }
        if (bottom > need_h) {
            need_h = bottom;
        }
    }
    if (island->board_w < need_w) {
        island->board_w = need_w;
    }
    if (island->board_h < need_h) {
        island->board_h = need_h;
    }
}

static void ui_save_compact_layout(R01sUi *ui) {
    int i;
    if (!ui) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        ui->compact_chip_x[i] = e ? e->board_x : 0;
        ui->compact_chip_y[i] = e ? e->board_y : 0;
        ui->compact_chip_orient[i] = e ? (uint8_t)e->orient : (uint8_t)R01S_ORIENT_H;
    }
    ui->compact_saved = 1;
}

void r01s_ui_apply_saved_island_layout(R01sUi *ui) {
    int i;
    int n_islands;

    if (!ui || !ui->group || !ui->layout_saved) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);

    /* Exact frames from file — no snap/expand/pack. */
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        if (!island || ui->save_island_w[i] <= 0 || ui->save_island_h[i] <= 0) {
            continue;
        }
        island->board_x = ui->save_island_x[i];
        island->board_y = ui->save_island_y[i];
        island->board_w = ui->save_island_w[i];
        island->board_h = ui->save_island_h[i];
    }

    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        if (e->visual == R01S_ENTITY_VIS_IC) {
            r01s_entity_set_orient(e, (R01sPkgOrient)ui->save_chip_orient[i]);
        }
        ui_chip_place_rel_exact(ui, i, ui->save_chip_x[i], ui->save_chip_y[i]);
    }
}

void r01s_ui_layout_migrate_v1_chips(R01sUi *ui) {
    int i;
    if (!ui || !ui->group) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sIsland *island = r01s_island_group_at(ui->group, ui->chip_island[i]);
        if (!island) {
            continue;
        }
        ui->save_chip_x[i] -= island->board_x;
        ui->save_chip_y[i] -= island->board_y;
    }
}

void r01s_ui_load_island_layout(R01sUi *ui, int file_version) {
    int i;
    int n_islands;

    if (!ui || !ui->group || !ui->layout_saved) {
        return;
    }

    /* v1 without island frames stored absolute board coordinates. */
    if (file_version < 2 && !ui_island_snapshot_valid(ui)) {
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *e = ui->chips[i];
            if (!e) {
                continue;
            }
            if (e->visual == R01S_ENTITY_VIS_IC) {
                r01s_entity_set_orient(e, (R01sPkgOrient)ui->save_chip_orient[i]);
            }
            r01s_entity_place(e, ui->save_chip_x[i], ui->save_chip_y[i]);
        }
        ui_arrange_islands_default(ui);
        r01s_ui_snapshot_island_layout(ui);
        return;
    }

    n_islands = r01s_island_group_count(ui->group);
    if (file_version < 2) {
        for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
            R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
            if (!island || ui->save_island_w[i] <= 0 || ui->save_island_h[i] <= 0) {
                continue;
            }
            island->board_x = ui->save_island_x[i];
            island->board_y = ui->save_island_y[i];
            island->board_w = ui->save_island_w[i];
            island->board_h = ui->save_island_h[i];
        }
        r01s_ui_layout_migrate_v1_chips(ui);
    }

    /*
     * Missing/empty islands[] with leftover island_chips is a common corrupt
     * save. Rebuild frames from relative chip placements when those look sane;
     * otherwise pack from scratch. Valid saves apply verbatim.
     */
    if (!ui_island_snapshot_valid(ui)) {
        int any_sane = 0;
        for (i = 0; i < n_islands; i++) {
            if (island_saved_chip_layout_sane(ui, i)) {
                any_sane = 1;
                break;
            }
        }
        if (any_sane) {
            ui_rebuild_islands_from_saved_chips(ui);
        } else {
            ui_arrange_islands_default(ui);
        }
        r01s_ui_snapshot_island_layout(ui);
        return;
    }

    r01s_ui_apply_saved_island_layout(ui);
}

static void ui_restore_island_layout(R01sUi *ui) {
    int i;
    int n_islands;

    if (!ui || !ui->group) {
        return;
    }
    if (ui->layout_saved && ui_island_snapshot_valid(ui)) {
        r01s_ui_apply_saved_island_layout(ui);
        return;
    }
    /* Frames missing: recover from island_chips if possible. */
    n_islands = r01s_island_group_count(ui->group);
    if (ui->layout_saved) {
        int any_sane = 0;
        for (i = 0; i < n_islands; i++) {
            if (island_saved_chip_layout_sane(ui, i)) {
                any_sane = 1;
                break;
            }
        }
        if (any_sane) {
            ui_rebuild_islands_from_saved_chips(ui);
            r01s_ui_snapshot_island_layout(ui);
            return;
        }
    }
    ui_arrange_islands_default(ui);
    r01s_ui_snapshot_island_layout(ui);
}

static void ui_restore_compact_layout(R01sUi *ui) {
    int i;
    if (!ui || !ui->compact_saved) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        if (e->visual == R01S_ENTITY_VIS_IC) {
            r01s_entity_set_orient(e, (R01sPkgOrient)ui->compact_chip_orient[i]);
        }
        r01s_entity_place(e, ui->compact_chip_x[i], ui->compact_chip_y[i]);
    }
}

/*
 * Pack all drawable chips into a near-square rectangle (shelf packing over
 * several candidate row widths). Chip body is inset by pin stub margin.
 */
static void ui_apply_compact_layout(R01sUi *ui) {
    R01sPackItem items[R01S_BOARD_MAX_CHIPS];
    int place_x[R01S_BOARD_MAX_CHIPS];
    int place_y[R01S_BOARD_MAX_CHIPS];
    int best_x[R01S_BOARD_MAX_CHIPS];
    int best_y[R01S_BOARD_MAX_CHIPS];
    int n = 0;
    int i;
    int area = 0;
    int side;
    int best_score = 0x7fffffff;
    int best_w = 0;
    int best_h = 0;
    int t;

    if (!ui) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!e || e->visual == R01S_ENTITY_VIS_NONE || e->body_w <= 0 || e->body_h <= 0) {
            continue;
        }
        items[n].idx = i;
        chip_pack_footprint(e, &items[n].pw, &items[n].ph);
        area += items[n].pw * items[n].ph;
        n++;
    }
    if (n == 0) {
        return;
    }
    qsort(items, (size_t)n, sizeof(items[0]), pack_item_taller);
    side = ui_isqrt(area);
    if (side < items[0].pw) {
        side = items[0].pw;
    }

    for (t = 0; t < 24; t++) {
        int max_row = side + (t - 8) * (side / 8 + 8);
        int bb_w = 0;
        int bb_h = 0;
        int score;
        int diff;
        if (max_row < items[0].pw) {
            max_row = items[0].pw;
        }
        pack_shelves(items, n, max_row, R01S_COMPACT_GAP, R01S_COMPACT_ORIGIN_X, R01S_COMPACT_ORIGIN_Y,
                     place_x, place_y, &bb_w, &bb_h);
        diff = bb_w > bb_h ? bb_w - bb_h : bb_h - bb_w;
        /* Prefer near-square, then smaller bounding box. */
        score = diff * 4 + bb_w + bb_h;
        if (score < best_score) {
            best_score = score;
            best_w = bb_w;
            best_h = bb_h;
            memcpy(best_x, place_x, (size_t)n * sizeof(int));
            memcpy(best_y, place_y, (size_t)n * sizeof(int));
        }
    }

    for (i = 0; i < n; i++) {
        R01sEntity *e = ui->chips[items[i].idx];
        int bx = best_x[i];
        int by = best_y[i];
        int pw, ph;
        if (!e) {
            continue;
        }
        chip_pack_footprint(e, &pw, &ph);
        /* Center body inside footprint so pin stubs stay inside the cell. */
        bx += (pw - e->body_w) / 2;
        by += (ph - e->body_h) / 2;
        r01s_entity_place(e, r01s_grid_snap(bx), r01s_grid_snap(by));
        clamp_chip_to_board(e);
    }

    (void)best_w;
    (void)best_h;
    ui->pan_x = 0;
    ui->pan_y = 0;
    r01s_ui_clamp_pan(ui);
    r01s_ui_chip_z_init(ui);
}

void ui_toggle_compact(R01sUi *ui) {
    if (!ui) {
        return;
    }
    ui->drag_chip = -1;
    ui->drag_island = -1;
    ui->resize_island = -1;
    ui->selected = -1;
    ui->ctx_chip = -1;
    ui->box_sel = 0;
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));

    if (!ui->layout_compact) {
        ui_save_island_layout(ui);
        if (ui->compact_saved) {
            ui_restore_compact_layout(ui);
        } else {
            ui_apply_compact_layout(ui);
            ui_save_compact_layout(ui);
        }
        if (ui->chip_z_count != ui->chip_count) {
            r01s_ui_chip_z_init(ui);
        }
        ui->layout_compact = 1;
        ui->layout_dirty = 1;
        snprintf(ui->status, sizeof(ui->status), "compact PCB layout — click ISLANDS to restore frames");
    } else {
        ui_save_compact_layout(ui);
        ui_restore_island_layout(ui);
        ui->layout_compact = 0;
        ui->layout_dirty = 1;
        snprintf(ui->status, sizeof(ui->status), "island layout restored");
        r01s_ui_clamp_pan(ui);
    }
}

void compact_btn_rect(const R01sUi *ui, SDL_Rect *rc) {
    const char *label = (ui && ui->layout_compact) ? "ISLANDS" : "COMPACT";
    int tw = font_text_width(label) + R01S_UI_UNIT * 2;
    rc->x = R01S_LOGIC_W - tw - R01S_UI_UNIT;
    rc->y = R01S_UI_UNIT / 2;
    rc->w = tw;
    rc->h = R01S_UI_UNIT * 2;
}

void save_btn_rect(const R01sUi *ui, SDL_Rect *rc) {
    SDL_Rect cbtn;
    const char *label = "SAVE";
    int tw = font_text_width(label) + R01S_UI_UNIT * 2;
    compact_btn_rect(ui, &cbtn);
    rc->w = tw;
    rc->h = R01S_UI_UNIT * 2;
    rc->y = R01S_UI_UNIT / 2;
    rc->x = cbtn.x - tw - R01S_UI_UNIT;
}

void ui_save_layout_now(R01sUi *ui) {
    if (!ui || !ui->group) {
        return;
    }
    if (r01s_ui_layout_save(ui) == 0) {
        snprintf(ui->status, sizeof(ui->status), "layout saved");
    } else {
        snprintf(ui->status, sizeof(ui->status), "layout save failed");
    }
}

static void island_content_min_size(const R01sUi *ui, int island_index, int *min_w, int *min_h) {
    const R01sIsland *island = r01s_island_group_at(ui->group, island_index);
    int i;
    int need_w = R01S_ISLAND_MIN_W;
    int need_h = R01S_ISLAND_MIN_H;

    if (!island) {
        *min_w = R01S_ISLAND_MIN_W;
        *min_h = R01S_ISLAND_MIN_H;
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        int right, bottom;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        right = (e->board_x - island->board_x) + e->body_w + R01S_CHIP_PIN_OUT + R01S_ISLAND_PAD_X;
        bottom = (e->board_y - island->board_y) + e->body_h + R01S_ISLAND_PAD_BOTTOM;
        if (right > need_w) {
            need_w = right;
        }
        if (bottom > need_h) {
            need_h = bottom;
        }
    }
    *min_w = need_w;
    *min_h = need_h;
}

void move_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my) {
    R01sIsland *island = r01s_island_group_at_mut(ui->group, island_index);
    int nx, ny, dx, dy, i;

    if (!island) {
        return;
    }
    nx = r01s_grid_snap(board_mx - ui->drag_grab_bx);
    ny = r01s_grid_snap(board_my - ui->drag_grab_by);
    if (nx < 0) {
        nx = 0;
    }
    if (ny < 0) {
        ny = 0;
    }
    if (nx + island->board_w > R01S_BOARD_W) {
        nx = r01s_grid_snap(R01S_BOARD_W - island->board_w);
    }
    if (ny + island->board_h > R01S_BOARD_H) {
        ny = r01s_grid_snap(R01S_BOARD_H - island->board_h);
    }
    if (nx < 0) {
        nx = 0;
    }
    if (ny < 0) {
        ny = 0;
    }
    dx = nx - island->board_x;
    dy = ny - island->board_y;
    if (dx == 0 && dy == 0) {
        return;
    }
    island->board_x = nx;
    island->board_y = ny;
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        r01s_entity_place(e, e->board_x + dx, e->board_y + dy);
    }
}

static void island_chip_content_bounds(const R01sUi *ui, int island_index, int *out_l, int *out_t, int *out_r,
                                       int *out_b) {
    int i;
    int have = 0;
    int l = 0, t = 0, r = 0, b = 0;

    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e;
        int cl, ct, cr, cb;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        if (!e || e->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        cl = e->board_x - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT;
        ct = e->board_y - R01S_ISLAND_PAD_TOP;
        cr = e->board_x + e->body_w + R01S_CHIP_PIN_OUT + R01S_ISLAND_PAD_X;
        cb = e->board_y + e->body_h + R01S_ISLAND_PAD_BOTTOM;
        if (!have) {
            l = cl;
            t = ct;
            r = cr;
            b = cb;
            have = 1;
        } else {
            if (cl < l) {
                l = cl;
            }
            if (ct < t) {
                t = ct;
            }
            if (cr > r) {
                r = cr;
            }
            if (cb > b) {
                b = cb;
            }
        }
    }
    if (!have) {
        l = 0;
        t = 0;
        r = R01S_ISLAND_MIN_W;
        b = R01S_ISLAND_MIN_H;
    }
    if (out_l) {
        *out_l = l;
    }
    if (out_t) {
        *out_t = t;
    }
    if (out_r) {
        *out_r = r;
    }
    if (out_b) {
        *out_b = b;
    }
}

void resize_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my) {
    R01sIsland *island = r01s_island_group_at_mut(ui->group, island_index);
    int fixed_l, fixed_t, fixed_r, fixed_b;
    int nx, ny, nw, nh;
    int need_l, need_t, need_r, need_b;
    int i;
    int corner;

    if (!island) {
        return;
    }
    corner = ui->resize_corner;
    fixed_l = island->board_x;
    fixed_t = island->board_y;
    fixed_r = island->board_x + island->board_w;
    fixed_b = island->board_y + island->board_h;
    island_chip_content_bounds(ui, island_index, &need_l, &need_t, &need_r, &need_b);

    switch (corner) {
    case R01S_ISLAND_CORNER_BL:
        nx = board_mx;
        ny = fixed_t;
        nw = fixed_r - board_mx;
        nh = board_my - fixed_t;
        break;
    case R01S_ISLAND_CORNER_TR:
        nx = fixed_l;
        ny = board_my;
        nw = board_mx - fixed_l;
        nh = fixed_b - board_my;
        break;
    case R01S_ISLAND_CORNER_TL:
        nx = board_mx;
        ny = board_my;
        nw = fixed_r - board_mx;
        nh = fixed_b - board_my;
        break;
    case R01S_ISLAND_CORNER_BR:
    default:
        nx = fixed_l;
        ny = fixed_t;
        nw = board_mx - fixed_l;
        nh = board_my - fixed_t;
        break;
    }

    /* Keep chips inside: clamp edges that are being dragged. */
    if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
        if (nx > need_l) {
            nx = need_l;
        }
        nw = fixed_r - nx;
    } else {
        if (nx + nw < need_r) {
            nw = need_r - nx;
        }
    }
    if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
        if (ny > need_t) {
            ny = need_t;
        }
        nh = fixed_b - ny;
    } else {
        if (ny + nh < need_b) {
            nh = need_b - ny;
        }
    }

    if (nw < R01S_ISLAND_MIN_W) {
        if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
            nx = fixed_r - R01S_ISLAND_MIN_W;
            nw = R01S_ISLAND_MIN_W;
        } else {
            nw = R01S_ISLAND_MIN_W;
        }
    }
    if (nh < R01S_ISLAND_MIN_H) {
        if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
            ny = fixed_b - R01S_ISLAND_MIN_H;
            nh = R01S_ISLAND_MIN_H;
        } else {
            nh = R01S_ISLAND_MIN_H;
        }
    }

    if (nx < 0) {
        if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
            nx = 0;
            nw = fixed_r - nx;
        } else {
            nx = 0;
        }
    }
    if (ny < 0) {
        if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
            ny = 0;
            nh = fixed_b - ny;
        } else {
            ny = 0;
        }
    }
    if (nx + nw > R01S_BOARD_W) {
        if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_TR) {
            nw = R01S_BOARD_W - nx;
        } else {
            nx = R01S_BOARD_W - nw;
            if (nx < 0) {
                nx = 0;
                nw = R01S_BOARD_W;
            }
        }
    }
    if (ny + nh > R01S_BOARD_H) {
        if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_BL) {
            nh = R01S_BOARD_H - ny;
        } else {
            ny = R01S_BOARD_H - nh;
            if (ny < 0) {
                ny = 0;
                nh = R01S_BOARD_H;
            }
        }
    }

    /* Snap origin down and size up so frames stay on the universal grid. */
    nx = r01s_grid_snap(nx);
    ny = r01s_grid_snap(ny);
    if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
        nw = fixed_r - nx;
    }
    if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
        nh = fixed_b - ny;
    }
    nw = r01s_grid_snap_up(nw);
    nh = r01s_grid_snap_up(nh);
    if (nw < R01S_ISLAND_MIN_W) {
        nw = R01S_ISLAND_MIN_W;
    }
    if (nh < R01S_ISLAND_MIN_H) {
        nh = R01S_ISLAND_MIN_H;
    }
    if (nx + nw > R01S_BOARD_W) {
        nw = r01s_grid_snap(R01S_BOARD_W - nx);
    }
    if (ny + nh > R01S_BOARD_H) {
        nh = r01s_grid_snap(R01S_BOARD_H - ny);
    }

    island->board_x = nx;
    island->board_y = ny;
    island->board_w = nw;
    island->board_h = nh;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] == (uint8_t)island_index) {
            clamp_chip(ui, ui->chips[i], island_index);
        }
    }
}

