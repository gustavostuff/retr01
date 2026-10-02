#include "r01a_board.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/passive.h"

static int dip_pin_hi(const NsEntity *e) {
    int dip;
    if (!e) {
        return 0;
    }
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    return dip > 0 ? dip : e->pin_count;
}

static void score_shift(const NsEntity *e, const NsBreadboard *bb, int dx, int dy, int *holes, int *conflicts) {
    int n;
    int count = 0;
    int conf = 0;
    int pin_hi = dip_pin_hi(e);
    int keys[NS_MAX_PINS];
    int nkey = 0;
    int i;
    int j;

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

int r01a_snap_entity_to_breadboard(NsEntity *e, NsBreadboard *bb) {
    int n;
    int col;
    int lane;
    int best_count = 0;
    int best_conf = NS_MAX_PINS;
    int best_dx = 0;
    int best_dy = 0;
    int pin_hi;
    int ox;
    int oy;

    if (!e || !bb) {
        return 0;
    }
    ox = e->board_x;
    oy = e->board_y;
    pin_hi = dip_pin_hi(e);
    if (pin_hi < 1) {
        return 0;
    }
    for (col = 0; col < NS_PB_COLS; col++) {
        for (lane = NS_PB_LANE_A; lane <= NS_PB_LANE_J; lane++) {
            NsPbHole h = {col, lane};
            int hx;
            int hy;
            if (!ns_breadboard_hole_exists(h)) {
                continue;
            }
            ns_breadboard_hole_world(bb, h, &hx, &hy);
            for (n = 1; n <= pin_hi; n++) {
                int tx;
                int ty;
                int dx;
                int dy;
                int count;
                int conf;
                ns_entity_place(e, ox, oy);
                if (!ns_entity_pin_tip_board(e, n, &tx, &ty)) {
                    continue;
                }
                dx = hx - tx;
                dy = hy - ty;
                score_shift(e, bb, dx, dy, &count, &conf);
                if (count > best_count || (count == best_count && conf < best_conf)) {
                    best_count = count;
                    best_conf = conf;
                    best_dx = dx;
                    best_dy = dy;
                }
            }
        }
    }
    ns_entity_place(e, ox, oy);
    if (best_count < 1) {
        return 0;
    }
    ns_entity_place(e, ox + best_dx, oy + best_dy);
    return best_count;
}

void r01a_place_pin_on_hole(NsEntity *e, int pin, NsBreadboard *bb, NsPbHole h) {
    int tx;
    int ty;
    int hx;
    int hy;
    if (!e || !bb) {
        return;
    }
    ns_entity_place(e, 0, 0);
    if (!ns_entity_pin_tip_board(e, pin, &tx, &ty)) {
        return;
    }
    ns_breadboard_hole_world(bb, h, &hx, &hy);
    ns_entity_place(e, hx - tx, hy - ty);
}

static int tip_on_hole(const NsBreadboard *bb, int wx, int wy, NsPbHole *out) {
    return ns_breadboard_tip_strip(bb, wx, wy, NULL) && ns_breadboard_hit_hole(bb, wx, wy, out);
}

int r01a_snap_passive_to_breadboard(NsPassive *p, NsBreadboard *bb) {
    int col;
    int lane;
    int best_score = 0;
    int best_d2 = 0x7fffffff;
    int save_px = 0;
    int save_py = 0;
    int save_e0 = 0;
    int save_e1 = 0;
    int ox;
    int oy;
    int otx;
    int oty;
    NsPbHole near;
    int col_min;
    int col_max;
    int max_move = NS_PB_PITCH * 2;

    if (!p || !bb) {
        return 0;
    }
    ox = p->pivot_x;
    oy = p->pivot_y;
    ns_passive_set_pivot(p, ox, oy);
    if (!ns_passive_tip_board(p, 1, &otx, &oty)) {
        return 0;
    }
    /* Only snap when a tip is already over / next to a hole — never teleport from afar. */
    if (!ns_breadboard_hit_hole(bb, otx, oty, &near)) {
        return 0;
    }
    {
        int hx0;
        int hy0;
        int ddx;
        int ddy;
        ns_breadboard_hole_world(bb, near, &hx0, &hy0);
        ddx = hx0 - otx;
        ddy = hy0 - oty;
        if (ddx * ddx + ddy * ddy > max_move * max_move) {
            return 0;
        }
    }
    col_min = near.col - 2;
    col_max = near.col + 2;
    if (col_min < 0) {
        col_min = 0;
    }
    if (col_max >= NS_PB_COLS) {
        col_max = NS_PB_COLS - 1;
    }
    for (col = col_min; col <= col_max; col++) {
        for (lane = NS_PB_LANE_A; lane <= NS_PB_LANE_J; lane++) {
            NsPbHole h1 = {col, lane};
            NsPbHole h2;
            int hx1;
            int hy1;
            int ext;
            int t1x;
            int t1y;
            int t2x;
            int t2y;
            int score = 0;
            int d2;
            int mdx;
            int mdy;

            if (!ns_breadboard_hole_exists(h1)) {
                continue;
            }
            ns_breadboard_hole_world(bb, h1, &hx1, &hy1);
            mdx = hx1 - otx;
            mdy = hy1 - oty;
            if (mdx * mdx + mdy * mdy > max_move * max_move) {
                continue;
            }
            ns_passive_set_pivot(p, ox, oy);
            ns_passive_set_leg_ext(p, 1, 0);
            ns_passive_set_leg_ext(p, 2, 0);
            if (!ns_passive_tip_board(p, 1, &t1x, &t1y)) {
                continue;
            }
            ns_passive_set_pivot(p, ox + (hx1 - t1x), oy + (hy1 - t1y));
            if (!tip_on_hole(bb, hx1, hy1, NULL)) {
                continue;
            }
            if (ns_passive_tip_board(p, 2, &t2x, &t2y) && tip_on_hole(bb, t2x, t2y, &h2)) {
                score = 2;
            } else {
                for (ext = 1; ext <= 64; ext++) {
                    ns_passive_set_leg_ext(p, 2, ext);
                    if (!ns_passive_tip_board(p, 2, &t2x, &t2y)) {
                        continue;
                    }
                    if (tip_on_hole(bb, t2x, t2y, &h2)) {
                        score = 2;
                        break;
                    }
                }
                if (score < 2) {
                    continue;
                }
            }
            d2 = mdx * mdx + mdy * mdy;
            if (score > best_score || (score == best_score && d2 < best_d2)) {
                best_score = score;
                best_d2 = d2;
                save_px = p->pivot_x;
                save_py = p->pivot_y;
                save_e0 = p->leg_ext[0];
                save_e1 = p->leg_ext[1];
            }
        }
    }
    if (best_score < 2) {
        ns_passive_set_pivot(p, ox, oy);
        ns_passive_set_leg_ext(p, 1, 0);
        ns_passive_set_leg_ext(p, 2, 0);
        return 0;
    }
    ns_passive_set_pivot(p, save_px, save_py);
    ns_passive_set_leg_ext(p, 1, save_e0);
    ns_passive_set_leg_ext(p, 2, save_e1);
    return best_score;
}

void r01a_seat_passive_on_holes(NsPassive *p, NsBreadboard *bb, NsPbHole h1, NsPbHole h2) {
    int t1x;
    int t1y;
    int t2x;
    int t2y;
    int hx1;
    int hy1;
    int ext;
    if (!p || !bb) {
        return;
    }
    ns_breadboard_hole_world(bb, h1, &hx1, &hy1);
    ns_passive_set_pivot(p, 0, 0);
    ns_passive_set_leg_ext(p, 1, 0);
    ns_passive_set_leg_ext(p, 2, 0);
    if (!ns_passive_tip_board(p, 1, &t1x, &t1y)) {
        return;
    }
    ns_passive_set_pivot(p, hx1 - t1x, hy1 - t1y);
    for (ext = 0; ext <= 64; ext++) {
        ns_passive_set_leg_ext(p, 2, ext);
        if (!ns_passive_tip_board(p, 2, &t2x, &t2y)) {
            continue;
        }
        if (ns_breadboard_tip_strip(bb, t2x, t2y, NULL)) {
            NsPbHole got;
            if (ns_breadboard_hit_hole(bb, t2x, t2y, &got) && got.col == h2.col && got.lane == h2.lane) {
                return;
            }
        }
    }
    (void)h2;
}

int r01a_breadboard_rail_power(const R01aBoard *board, const NsBreadboard *bb, int *pos_out, int *neg_out) {
    if (pos_out) {
        *pos_out = 0;
    }
    if (neg_out) {
        *neg_out = 0;
    }
    if (!board || !bb || !bb->base.refdes) {
        return 0;
    }
    if (strcmp(bb->base.refdes, "BB1") == 0) {
        if (pos_out) {
            *pos_out = 1;
        }
        if (neg_out) {
            *neg_out = 1;
        }
        return 1;
    }
    return 0;
}
