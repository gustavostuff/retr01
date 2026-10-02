#include "r01a_ui.h"

#include "discrete_ic/island.h"

#include <string.h>

void bind_breadboards(R01aUi *ui, R01aBoard *board) {
    NsIsland *island;
    int i;
    if (!ui || !board) {
        return;
    }
    ui->bb_count = 0;
    island = ns_island_group_at_mut(r01a_board_group(board), 0);
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count && ui->bb_count < R01A_BB_MAX; i++) {
        NsEntity *e = island->entities[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        ui->bbs[ui->bb_count++] = (NsBreadboard *)e;
    }
}

NsBreadboard *ui_bb_named(const R01aUi *ui, const char *ref) {
    int i;
    if (!ui || !ref) {
        return NULL;
    }
    for (i = 0; i < ui->bb_count; i++) {
        if (ui->bbs[i] && ui->bbs[i]->base.refdes && strcmp(ui->bbs[i]->base.refdes, ref) == 0) {
            return ui->bbs[i];
        }
    }
    return NULL;
}

static int bb_body_hit(const NsBreadboard *bb, int bx, int by) {
    const NsEntity *e;
    if (!bb) {
        return 0;
    }
    e = &bb->base;
    return bx >= e->board_x && by >= e->board_y && bx < e->board_x + e->body_w && by < e->board_y + e->body_h;
}

int hit_bb_body(const R01aUi *ui, int bx, int by, int *bb_i_out) {
    int i;
    for (i = ui->bb_count - 1; i >= 0; i--) {
        if (bb_body_hit(ui->bbs[i], bx, by)) {
            if (bb_i_out) {
                *bb_i_out = i;
            }
            return 1;
        }
    }
    return 0;
}

NsBreadboard *hit_breadboard(const R01aUi *ui, int mx, int my, NsPbHole *hole) {
    int i;
    for (i = ui->bb_count - 1; i >= 0; i--) {
        NsBreadboard *bb = ui->bbs[i];
        NsPbHole h;
        if (!bb) {
            continue;
        }
        if (ns_breadboard_hit_hole(bb, mx, my, &h)) {
            if (hole) {
                *hole = h;
            }
            return bb;
        }
    }
    return NULL;
}

int chip_pin_on_hole(const R01aUi *ui, const NsBreadboard *bb, NsPbHole hole) {
    int wx;
    int wy;
    int ci;
    if (!ui || !bb) {
        return 0;
    }
    ns_breadboard_hole_world(bb, hole, &wx, &wy);
    for (ci = 0; ci < ui->chip_count; ci++) {
        NsEntity *e = ui->chips[ci];
        int pi;
        if (!e) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int tx;
            int ty;
            if (!pin_center(e, pi, &tx, &ty)) {
                continue;
            }
            if (tx == wx && ty == wy) {
                return 1;
            }
        }
    }
    return 0;
}

void ui_sync_wire_mode(R01aUi *ui, R01aBoard *board) {
    if (!ui || !board) {
        return;
    }
    r01a_board_set_wire_mode(board, ui->show_nets ? R01A_WIRE_MANUAL : R01A_WIRE_AUTO);
}

void draw_breadboards(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board) {
    int i;
    if (!r || !ui) {
        return;
    }
    for (i = 0; i < ui->bb_count; i++) {
        NsBreadboard *bb = ui->bbs[i];
        int sel = (i == ui->selected_bb);
        int pos = 0;
        int neg = 0;
        if (!bb) {
            continue;
        }
        if (board) {
            (void)r01a_breadboard_rail_power(board, bb, &pos, &neg);
        }
        ns_breadboard_draw_power(r, bb, board_sx(ui, bb->base.board_x), board_sy(ui, bb->base.board_y), sel,
                                 pos, neg);
    }
}

static void score_shift_ui(const NsEntity *e, const NsBreadboard *bb, int dx, int dy, int *holes, int *conflicts) {
    int n;
    int count = 0;
    int conf = 0;
    int pin_hi = e && e->dip_pins > 0 ? e->dip_pins : (e ? e->pin_count : 0);
    int keys[NS_MAX_PINS];
    int nkey = 0;
    int i;
    int j;

    if (!e || !bb || pin_hi < 1) {
        if (holes) {
            *holes = 0;
        }
        if (conflicts) {
            *conflicts = 0;
        }
        return;
    }
    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        int strip;
        if (!ns_entity_pin_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        tx += dx;
        ty += dy;
        if (!ns_breadboard_tip_strip(bb, tx, ty, &strip)) {
            continue;
        }
        if (nkey < NS_MAX_PINS) {
            keys[nkey++] = strip;
        }
        count++;
    }
    for (i = 0; i < nkey; i++) {
        for (j = i + 1; j < nkey; j++) {
            if (keys[i] == keys[j]) {
                conf++;
            }
        }
    }
    if (holes) {
        *holes = count;
    }
    if (conflicts) {
        *conflicts = conf;
    }
}

void snap_chip_to_breadboard(R01aUi *ui, int chip_i) {
    NsEntity *e;
    int n;
    int bi;
    int pin_hi;
    int best_count = 0;
    int best_conf = NS_MAX_PINS;
    int best_dx = 0;
    int best_dy = 0;

    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    pin_hi = e && e->dip_pins > 0 ? e->dip_pins : (e ? e->pin_count : 0);
    if (!e || pin_hi < 1) {
        return;
    }
    if (e->visual != NS_ENTITY_VIS_IC && e->visual != NS_ENTITY_VIS_PIN_HDR) {
        return;
    }
    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        if (!ns_entity_pin_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        for (bi = 0; bi < ui->bb_count; bi++) {
            NsBreadboard *bb = ui->bbs[bi];
            NsPbHole h;
            int hx;
            int hy;
            int dx;
            int dy;
            int count;
            int conf;
            if (!bb || !ns_breadboard_hit_hole(bb, tx, ty, &h)) {
                continue;
            }
            ns_breadboard_hole_world(bb, h, &hx, &hy);
            dx = hx - tx;
            dy = hy - ty;
            score_shift_ui(e, bb, dx, dy, &count, &conf);
            if (count > best_count || (count == best_count && conf < best_conf)) {
                best_count = count;
                best_conf = conf;
                best_dx = dx;
                best_dy = dy;
            }
        }
    }
    if (best_count < 1) {
        return;
    }
    /* Reject long jumps: snap is for seating already over the board. */
    if (best_dx * best_dx + best_dy * best_dy > (NS_PB_PITCH * 2) * (NS_PB_PITCH * 2)) {
        return;
    }
    ns_entity_place(e, snap_grid(e->board_x + best_dx), snap_grid(e->board_y + best_dy));
}

static void snap_passive_part(R01aUi *ui, int chip_i) {
    NsEntity *e;
    int bi;
    NsPassive *p;
    int best = 0;
    int opx;
    int opy;
    int oe0;
    int oe1;
    int bpx;
    int bpy;
    int be0;
    int be1;

    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e || (e->visual != NS_ENTITY_VIS_PASSIVE && e->visual != NS_ENTITY_VIS_OSC)) {
        return;
    }
    p = (NsPassive *)e;
    opx = p->pivot_x;
    opy = p->pivot_y;
    oe0 = p->leg_ext[0];
    oe1 = p->leg_ext[1];
    bpx = opx;
    bpy = opy;
    be0 = oe0;
    be1 = oe1;
    for (bi = 0; bi < ui->bb_count; bi++) {
        NsBreadboard *bb = ui->bbs[bi];
        int score;
        if (!bb) {
            continue;
        }
        ns_passive_set_pivot(p, opx, opy);
        ns_passive_set_leg_ext(p, 1, oe0);
        ns_passive_set_leg_ext(p, 2, oe1);
        score = r01a_snap_passive_to_breadboard(p, bb);
        if (score > best) {
            best = score;
            bpx = p->pivot_x;
            bpy = p->pivot_y;
            be0 = p->leg_ext[0];
            be1 = p->leg_ext[1];
        }
    }
    if (best >= 2) {
        ns_passive_set_pivot(p, bpx, bpy);
        ns_passive_set_leg_ext(p, 1, be0);
        ns_passive_set_leg_ext(p, 2, be1);
    } else {
        ns_passive_set_pivot(p, opx, opy);
        ns_passive_set_leg_ext(p, 1, oe0);
        ns_passive_set_leg_ext(p, 2, oe1);
    }
}

void snap_part_to_breadboard(R01aUi *ui, int chip_i) {
    NsEntity *e;
    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_IC || e->visual == NS_ENTITY_VIS_PIN_HDR) {
        snap_chip_to_breadboard(ui, chip_i);
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE || e->visual == NS_ENTITY_VIS_OSC) {
        snap_passive_part(ui, chip_i);
    }
}
