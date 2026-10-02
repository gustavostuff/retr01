#include "r01a_ui.h"

#include "discrete_ic/island.h"
#include "discrete_ic/island_group.h"
#include "discrete_ic/breadboard.h"
#include "discrete_ic/pin_header.h"

#include <string.h>

int div_floor(int a, int b) {
    if (b <= 0) {
        return a;
    }
    if (a >= 0) {
        return a / b;
    }
    return -((-a + b - 1) / b);
}

int canvas_zoom(const R01aUi *ui) {
    int z = (ui && ui->zoom > 0) ? ui->zoom : 1;
    return z > R01A_ZOOM_MAX ? R01A_ZOOM_MAX : z;
}

int board_sx(const R01aUi *ui, int bx) {
    return bx - ui->pan_x;
}

int board_sy(const R01aUi *ui, int by) {
    return by - ui->pan_y;
}

void logic_to_board(const R01aUi *ui, int lx, int ly, int *bx, int *by) {
    int z = canvas_zoom(ui);
    *bx = div_floor(lx, z) + ui->pan_x;
    *by = div_floor(ly, z) + ui->pan_y;
}

void canvas_zoom_by(R01aUi *ui, int delta, int lx, int ly) {
    int z0 = canvas_zoom(ui);
    int z1 = z0 + delta;
    int bx;
    int by;
    if (z1 < 1) {
        z1 = 1;
    }
    if (z1 > R01A_ZOOM_MAX) {
        z1 = R01A_ZOOM_MAX;
    }
    if (z1 == z0) {
        return;
    }
    logic_to_board(ui, lx, ly, &bx, &by);
    ui->zoom = z1;
    ui->pan_x = snap_grid(bx - div_floor(lx, z1));
    ui->pan_y = snap_grid(by - div_floor(ly, z1));
}

/* Keep board point (gx, gy) under logical cursor (lx, ly). */

void pan_lock_point(R01aUi *ui, int gx, int gy, int lx, int ly) {
    int z = canvas_zoom(ui);
    ui->pan_x = snap_grid(gx - div_floor(lx, z));
    ui->pan_y = snap_grid(gy - div_floor(ly, z));
}

void snap_octant(int x0, int y0, int x1, int y1, int *ox, int *oy) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    if (adx == 0 && ady == 0) {
        *ox = x0;
        *oy = y0;
        return;
    }
    if (ady * 2 < adx) {
        *ox = x1;
        *oy = y0;
        return;
    }
    if (adx * 2 < ady) {
        *ox = x0;
        *oy = y1;
        return;
    }
    {
        int m = adx < ady ? adx : ady;
        *ox = x0 + (dx < 0 ? -m : m);
        *oy = y0 + (dy < 0 ? -m : m);
    }
}

static int skip_entity(const NsEntity *e) {
    return !e || e->visual == NS_ENTITY_VIS_BREADBOARD;
}

void bind_chips(R01aUi *ui, R01aBoard *board) {
    NsIsland *island = ns_island_group_at_mut(r01a_board_group(board), 0);
    int i;
    ui->chip_count = 0;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count && ui->chip_count < R01A_BOARD_MAX_CHIPS; i++) {
        NsEntity *e = island->entities[i];
        if (skip_entity(e)) {
            continue;
        }
        ui->chips[ui->chip_count] = e;
        ui->chip_z[ui->chip_count] = (uint8_t)ui->chip_count;
        ui->chip_count++;
    }
}

int snap_grid(int v) {
    int g = R01A_GRID;
    if (v >= 0) {
        return ((v + g / 2) / g) * g;
    }
    return -(((-v + g / 2) / g) * g);
}

/* Pin-center lattice: 1, 4, 7… so a 3×3 pad fills one grid cell. */

int snap_pin(int v) {
    return snap_grid(v - R01A_PAD / 2) + R01A_PAD / 2;
}

void move_entity(NsEntity *e, int bx, int by) {
    if (!e) {
        return;
    }
    bx = snap_grid(bx);
    by = snap_grid(by);
    if (e->visual == NS_ENTITY_VIS_BREADBOARD) {
        ns_entity_place(e, bx, by);
        ns_breadboard_sync_body((NsBreadboard *)e);
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        NsPassive *p = (NsPassive *)e;
        ns_passive_set_pivot(p, p->pivot_x + (bx - e->board_x), p->pivot_y + (by - e->board_y));
        return;
    }
    ns_entity_place(e, bx, by);
}

void snap_placed_chips(R01aUi *ui) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        move_entity(e, e->board_x, e->board_y);
    }
}

static int dip_count(const NsEntity *e) {
    if (!e) {
        return 0;
    }
    return e->dip_pins > 0 ? e->dip_pins : e->pin_count;
}

static int ic_along(const NsEntity *e) {
    int half = dip_count(e) / 2;
    if (half < 1) {
        half = 1;
    }
    return R01A_GRID + (half > 1 ? (half - 1) * R01A_PAD_PITCH : 0);
}

static int ic_across(const NsEntity *e) {
    return (e && e->pkg_wid_mm >= 13) ? 21 : 12;
}

int ic_body_w(const NsEntity *e) {
    if (!e) {
        return 1;
    }
    return ns_orient_is_horiz(e->orient) ? ic_along(e) : ic_across(e);
}

int ic_body_h(const NsEntity *e) {
    if (!e) {
        return 1;
    }
    return ns_orient_is_horiz(e->orient) ? ic_across(e) : ic_along(e);
}

int is_passive_glyph(const NsEntity *e) {
    return e && (e->visual == NS_ENTITY_VIS_PASSIVE || e->visual == NS_ENTITY_VIS_OSC ||
                 e->visual == NS_ENTITY_VIS_PIN_HDR);
}

int is_axial_passive(const NsEntity *e) {
    NsPassiveKind k;
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_OSC && e->pin_count == 2) {
        return 1;
    }
    if (e->visual != NS_ENTITY_VIS_PASSIVE) {
        return 0;
    }
    k = ((const NsPassive *)e)->kind;
    return k == NS_PASSIVE_R || k == NS_PASSIVE_CCAP || k == NS_PASSIVE_ECAP || k == NS_PASSIVE_D ||
           k == NS_PASSIVE_XTAL || k == NS_PASSIVE_OSC;
}

static void header_grid(const NsEntity *e, int *cols, int *rows) {
    int c = 1;
    int rw = 1;
    if (e && e->visual == NS_ENTITY_VIS_PIN_HDR) {
        ns_pin_header_grid(e, &c, &rw);
    }
    if (c < 1) {
        c = 1;
    }
    if (rw < 1) {
        rw = 1;
    }
    if (cols) {
        *cols = c;
    }
    if (rows) {
        *rows = rw;
    }
}

int glyph_w(const NsEntity *e) {
    int n;
    if (is_axial_passive(e)) {
        return 12;
    }
    if (e && e->visual == NS_ENTITY_VIS_PIN_HDR) {
        int cols;
        int rows;
        header_grid(e, &cols, &rows);
        return cols * R01A_HDR_CELL;
    }
    n = (e && e->pin_count > 0) ? e->pin_count : 1;
    return R01A_GRID + (n > 1 ? (n - 1) * R01A_PAD_PITCH : 0);
}

int glyph_h(const NsEntity *e) {
    if (e && e->visual == NS_ENTITY_VIS_PIN_HDR) {
        int cols;
        int rows;
        header_grid(e, &cols, &rows);
        return rows * R01A_HDR_CELL;
    }
    return 9;
}

int pin_center(const NsEntity *e, int pin_index, int *cx, int *cy) {
    int num;
    int dip;
    int half;
    int side_pin1;
    int idx;
    int reverse;
    int along;
    if (!e || pin_index < 0 || pin_index >= e->pin_count || !cx || !cy) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_PIN_HDR) {
        int cols;
        int rows;
        int col;
        int row;
        header_grid(e, &cols, &rows);
        col = pin_index % cols;
        row = pin_index / cols;
        *cx = e->board_x + R01A_HDR_PAD + R01A_PAD / 2 + col * R01A_HDR_CELL;
        *cy = e->board_y + R01A_HDR_PAD + R01A_PAD / 2 + row * R01A_HDR_CELL;
        return 1;
    }
    if (is_axial_passive(e)) {
        int h = glyph_h(e);
        *cy = e->board_y + (h / R01A_GRID) * (R01A_GRID / 2) + 1;
        if ((pin_index & 1) == 0) {
            *cx = e->board_x - 2;
        } else {
            *cx = e->board_x + glyph_w(e) + 1;
        }
        return 1;
    }
    if (is_passive_glyph(e)) {
        *cx = e->board_x + 1 + pin_index * R01A_PAD_PITCH;
        *cy = e->board_y + glyph_h(e) + 1;
        return 1;
    }
    /* ICs: electrical tips (same lattice used by breadboard routing). */
    if (e->visual == NS_ENTITY_VIS_IC) {
        return ns_entity_pin_tip_board(e, e->pins[pin_index].number, cx, cy);
    }
    num = e->pins[pin_index].number;
    dip = dip_count(e);
    if (num < 1 || num > dip || dip < 2) {
        return ns_entity_pin_tip_board(e, num, cx, cy);
    }
    half = dip / 2;
    side_pin1 = num <= half;
    idx = side_pin1 ? (num - 1) : (dip - num);
    reverse = (e->orient == NS_ORIENT_180 || e->orient == NS_ORIENT_270);
    along = 1 + (reverse ? ((half > 0 ? (half - 1 - idx) : 0) * R01A_PAD_PITCH)
                         : (idx * R01A_PAD_PITCH));
    switch (e->orient) {
    case NS_ORIENT_90:
        *cy = e->board_y + along;
        *cx = side_pin1 ? (e->board_x - 2) : (e->board_x + ic_body_w(e) + 1);
        break;
    case NS_ORIENT_180:
        *cx = e->board_x + along;
        *cy = side_pin1 ? (e->board_y + ic_body_h(e) + 1) : (e->board_y - 2);
        break;
    case NS_ORIENT_270:
        *cy = e->board_y + along;
        *cx = side_pin1 ? (e->board_x + ic_body_w(e) + 1) : (e->board_x - 2);
        break;
    case NS_ORIENT_0:
    default:
        *cx = e->board_x + along;
        *cy = side_pin1 ? (e->board_y + ic_body_h(e) + 1) : (e->board_y - 2);
        break;
    }
    return 1;
}

static int pad_contains(int cx, int cy, int bx, int by) {
    int hx = R01A_PAD / 2;
    return bx >= cx - hx && bx <= cx + hx && by >= cy - hx && by <= cy + hx;
}

int hit_pin_at(const R01aUi *ui, int bx, int by, int *chip_out, int *pin_out) {
    int rank;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        int i;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (i = 0; i < e->pin_count; i++) {
            int cx;
            int cy;
            if (!pin_center(e, i, &cx, &cy)) {
                continue;
            }
            if (pad_contains(cx, cy, bx, by)) {
                if (chip_out) {
                    *chip_out = ci;
                }
                if (pin_out) {
                    *pin_out = i;
                }
                return 1;
            }
        }
    }
    return 0;
}

static int hit_chip_body(const NsEntity *e, int bx, int by) {
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_DISPLAY) {
        return bx >= e->board_x && by >= e->board_y && bx < e->board_x + e->body_w &&
               by < e->board_y + e->body_h;
    }
    if (is_passive_glyph(e)) {
        return bx >= e->board_x && by >= e->board_y && bx < e->board_x + glyph_w(e) &&
               by < e->board_y + glyph_h(e);
    }
    {
        int pin = 2;
        int x0 = e->board_x;
        int y0 = e->board_y;
        int x1 = x0 + e->body_w;
        int y1 = y0 + e->body_h;
        if (ns_orient_is_horiz(e->orient)) {
            return bx >= x0 && bx < x1 && by >= y0 - pin && by < y1 + pin;
        }
        return by >= y0 && by < y1 && bx >= x0 - pin && bx < x1 + pin;
    }
}

int hit_top_chip(const R01aUi *ui, int bx, int by) {
    int rank;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        if (hit_chip_body(ui->chips[ci], bx, by)) {
            return ci;
        }
    }
    return -1;
}

NsEntity *ui_ent(const R01aUi *ui, const char *ref) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] && ui->chips[i]->refdes && strcmp(ui->chips[i]->refdes, ref) == 0) {
            return ui->chips[i];
        }
    }
    return NULL;
}
