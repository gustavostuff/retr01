#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_netlist.h"
#include "retr01_sim/bus.h"
#include "breadboard.h"

#include <stdio.h>
#include <string.h>

static R01sPinNetlist *ui_board_pin_net(const R01sUi *ui) {
    R01sBoard *board;
    if (!ui || !ui->group) {
        return NULL;
    }
    board = r01s_board_from_group(ui->group);
    return board ? &board->pin_netlist : NULL;
}

/* Manual mode: schematic peer only "lives" when both tips sit on the same BB strip. */
static int ui_bb_pins_connected(const R01sUi *ui, int chip_a, int pin_a, int chip_b, int pin_b) {
    const R01sEntity *ea;
    const R01sEntity *eb;
    int num_a;
    int num_b;
    int tax, tay, tbx, tby;
    int bi;

    if (!ui || chip_a < 0 || chip_b < 0 || chip_a >= ui->chip_count || chip_b >= ui->chip_count) {
        return 0;
    }
    ea = ui->chips[chip_a];
    eb = ui->chips[chip_b];
    if (!ea || !eb || pin_a < 0 || pin_b < 0 || pin_a >= ea->pin_count || pin_b >= eb->pin_count) {
        return 0;
    }
    num_a = ea->pins[pin_a].number;
    num_b = eb->pins[pin_b].number;
    if (!ui_chip_pin_tip_board(ea, num_a, &tax, &tay) || !ui_chip_pin_tip_board(eb, num_b, &tbx, &tby)) {
        return 0;
    }
    for (bi = 0; bi < ui->chip_count; bi++) {
        const R01sEntity *be = ui->chips[bi];
        const R01sBreadboard *bb;
        int sa = -1;
        int sb = -1;
        if (!be || be->visual != R01S_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        bb = (const R01sBreadboard *)(const void *)be;
        if (!r01s_breadboard_tip_strip(bb, tax, tay, &sa)) {
            continue;
        }
        if (!r01s_breadboard_tip_strip(bb, tbx, tby, &sb)) {
            continue;
        }
        if (sa == sb) {
            return 1;
        }
    }
    return 0;
}

static int pin_skip_wire(const R01sPin *p) {
    return !p || p->dir == R01S_PIN_NC || p->dir == R01S_PIN_PWR;
}

static int pin_name_eq_dq(const char *d, const char *dq) {
    if (d[0] != 'D' || dq[0] != 'D' || dq[1] != 'Q') {
        return 0;
    }
    return strcmp(d + 1, dq + 2) == 0;
}

static int pin_name_eq_latch_d(const char *ld, const char *d) {
    if (ld[0] < '1' || ld[0] > '8' || ld[1] != 'D' || ld[2] != '\0') {
        return 0;
    }
    if (d[0] != 'D' || d[1] < '0' || d[1] > '7' || d[2] != '\0') {
        return 0;
    }
    return (ld[0] - '1') == (d[1] - '0');
}

static int pin_name_eq_latch_q(const char *lq, const char *d) {
    if (lq[0] < '1' || lq[0] > '8' || lq[1] != 'Q' || lq[2] != '\0') {
        return 0;
    }
    if (d[0] != 'D' || d[1] < '0' || d[1] > '7' || d[2] != '\0') {
        return 0;
    }
    return (lq[0] - '1') == (d[1] - '0');
}

static int pin_name_eq_beam_q_latch_q(const char *bq, const char *lq) {
    if (bq[0] != 'Q' || bq[1] < '0' || bq[1] > '7' || bq[2] != '\0') {
        return 0;
    }
    if (lq[0] < '1' || lq[0] > '8' || lq[1] != 'Q' || lq[2] != '\0') {
        return 0;
    }
    return (bq[1] - '0') == (lq[0] - '1');
}

static int pin_name_eq_mux_y_a(const char *my, const char *a) {
    if (my[0] < '1' || my[0] > '4' || my[1] != 'Y' || my[2] != '\0') {
        return 0;
    }
    if (a[0] != 'A' || a[2] != '\0' || a[1] < '0' || a[1] > '9') {
        return 0;
    }
    return (my[0] - '1') == (a[1] - '0');
}

static int pin_name_eq_latch_q_a(const char *lq, const char *a) {
    if (lq[0] < '1' || lq[0] > '8' || lq[1] != 'Q' || lq[2] != '\0') {
        return 0;
    }
    if (a[0] != 'A' || a[2] != '\0' || a[1] < '0' || a[1] > '9') {
        return 0;
    }
    return (lq[0] - '1') == (a[1] - '0');
}

static int pin_name_eq_245_ab(const char *a, const char *b) {
    char side_a;
    char side_b;
    int ia;
    int ib;
    if (!a || !b || a[0] != b[0]) {
        return 0;
    }
    side_a = a[0];
    side_b = b[0];
    if ((side_a != 'A' && side_a != 'B') || side_b != side_a) {
        return 0;
    }
    ia = (int)strtol(a + 1, NULL, 10);
    ib = (int)strtol(b + 1, NULL, 10);
    return ia >= 1 && ia <= 8 && ia == ib;
}

static int pin_signals_match(const R01sPin *pa, const R01sPin *pb) {
    const char *na;
    const char *nb;

    if (pin_skip_wire(pa) || pin_skip_wire(pb)) {
        return 0;
    }
    na = pa->name;
    nb = pb->name;
    if (!na || !nb) {
        return 0;
    }
    if (strcmp(na, nb) == 0) {
        return 1;
    }
    if (pin_name_eq_dq(na, nb) || pin_name_eq_dq(nb, na)) {
        return 1;
    }
    if (pin_name_eq_latch_d(na, nb) || pin_name_eq_latch_d(nb, na)) {
        return 1;
    }
    if (pin_name_eq_latch_q(na, nb) || pin_name_eq_latch_q(nb, na)) {
        return 1;
    }
    if (pin_name_eq_beam_q_latch_q(na, nb) || pin_name_eq_beam_q_latch_q(nb, na)) {
        return 1;
    }
    if ((na[0] == 'P' && nb[0] == 'Y' && strcmp(na + 1, nb + 1) == 0) ||
        (na[0] == 'Y' && nb[0] == 'P' && strcmp(na + 1, nb + 1) == 0)) {
        return 1;
    }
    if (pin_name_eq_mux_y_a(na, nb) || pin_name_eq_mux_y_a(nb, na)) {
        return 1;
    }
    if (pin_name_eq_latch_q_a(na, nb) || pin_name_eq_latch_q_a(nb, na)) {
        return 1;
    }
    if (pin_name_eq_245_ab(na, nb)) {
        return 1;
    }
    if ((strcmp(na, "IRQB") == 0 && strcmp(nb, "EQ#") == 0) ||
        (strcmp(na, "EQ#") == 0 && strcmp(nb, "IRQB") == 0)) {
        return 1;
    }
    return 0;
}

void r01s_ui_pin_net_build(R01sBoard *board) {
    r01s_board_netlist_rebuild(board);
}

static int ui_chip_index(const R01sUi *ui, const R01sEntity *e) {
    int i;
    if (!ui || !e) {
        return -1;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] == e) {
            return i;
        }
    }
    return -1;
}

static int ui_pin_dip_package(const R01sEntity *e, int pin_index) {
    int num;
    int dip;
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    num = e->pins[pin_index].number;
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    return num >= 1 && num <= dip;
}

int ui_hit_chip_pin(const R01sUi *ui, int lx, int ly, int *chip_out, int *pin_out) {
    int best_chip = -1;
    int best_pin = -1;
    int best_dist = 999999;
    int ci;

    if (chip_out) {
        *chip_out = -1;
    }
    if (pin_out) {
        *pin_out = -1;
    }
    if (!ui || !ui_logic_in_view(lx, ly)) {
        return 0;
    }

    for (ci = ui->chip_count - 1; ci >= 0; ci--) {
        const R01sEntity *e = ui->chips[ci];
        int pi;
        if (!e || e->visual != R01S_ENTITY_VIS_IC || ui_chip_hidden(ui, e)) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int sx;
            int sy;
            int dx;
            int dy;
            int dist;
            if (!ui_pin_dip_package(e, pi)) {
                continue;
            }
            if (!ui_chip_pin_screen_center(ui, e, pi, &sx, &sy)) {
                continue;
            }
            dx = lx - sx;
            dy = ly - sy;
            if (dx < -4 || dx > 4 || dy < -4 || dy > 4) {
                continue;
            }
            dist = dx * dx + dy * dy;
            if (dist < best_dist) {
                best_dist = dist;
                best_chip = ci;
                best_pin = pi;
            }
        }
    }

    if (best_chip < 0) {
        return 0;
    }
    if (chip_out) {
        *chip_out = best_chip;
    }
    if (pin_out) {
        *pin_out = best_pin;
    }
    return 1;
}

static int ui_pin_net_pick_peer(const R01sUi *ui, const R01sEntity *src, int pin_i, int sx, int sy,
                                int peer_chip, int peer_pin, int *best_chip, int *best_pin, int *best_dist,
                                int prefer_off_chip) {
    const R01sEntity *peer_e;
    int px;
    int py;
    int dx;
    int dy;
    int dist;
    int off_chip;

    if (peer_chip < 0 || peer_pin < 0 || !best_chip || !best_pin || !best_dist) {
        return 0;
    }
    peer_e = ui->chips[peer_chip];
    if (!peer_e || !ui_pin_dip_package(peer_e, peer_pin)) {
        return 0;
    }
    if (peer_e == src && peer_pin == pin_i) {
        return 0;
    }
    if (!ui_chip_pin_screen_center(ui, peer_e, peer_pin, &px, &py)) {
        return 0;
    }
    off_chip = (peer_e != src);
    if (prefer_off_chip && !off_chip) {
        return 0;
    }
    dx = px - sx;
    dy = py - sy;
    dist = dx * dx + dy * dy;
    if (dist < *best_dist) {
        *best_dist = dist;
        *best_chip = peer_chip;
        *best_pin = peer_pin;
        return 1;
    }
    return 0;
}

static int ui_pin_net_peer_union(R01sPinNetlist *nl, const R01sUi *ui, const R01sEntity *src, int pin_i,
                                 int src_slot, int sx, int sy, int *peer_chip_out, int *peer_pin_out) {
    int root;
    int best_chip = -1;
    int best_pin = -1;
    int best_dist = 999999;
    int pass;
    int i;

    if (!nl) {
        return 0;
    }
    root = r01s_pin_netlist_root(nl, src_slot);
    if (root < 0) {
        return 0;
    }

    /* Prefer a peer on a different IC; same-package only if nothing else is wired. */
    for (pass = 0; pass < 2; pass++) {
        int prefer_off_chip = (pass == 0);
        best_chip = -1;
        best_pin = -1;
        best_dist = 999999;
        for (i = 0; i < nl->slot_count; i++) {
            const R01sEntity *peer_e;
            int peer_chip;
            int peer_pin;
            if (r01s_pin_netlist_root(nl, i) != root) {
                continue;
            }
            if (i == src_slot) {
                continue;
            }
            peer_e = nl->slots[i].entity;
            peer_pin = nl->slots[i].pin_index;
            if (!peer_e) {
                continue;
            }
            peer_chip = ui_chip_index(ui, peer_e);
            if (peer_chip < 0) {
                continue;
            }
            (void)ui_pin_net_pick_peer(ui, src, pin_i, sx, sy, peer_chip, peer_pin, &best_chip, &best_pin,
                                       &best_dist, prefer_off_chip);
        }
        if (best_chip >= 0) {
            break;
        }
    }

    if (best_chip < 0) {
        return 0;
    }
    if (peer_chip_out) {
        *peer_chip_out = best_chip;
    }
    if (peer_pin_out) {
        *peer_pin_out = best_pin;
    }
    return 1;
}

static int ui_pin_net_peer_by_name(const R01sUi *ui, const R01sEntity *src, int pin_i, int chip_i, int sx,
                                   int sy, int *peer_chip_out, int *peer_pin_out) {
    const R01sPin *src_pin;
    int best_chip = -1;
    int best_pin = -1;
    int best_dist = 999999;
    int pass;
    int ci;

    src_pin = &src->pins[pin_i];
    if (pin_skip_wire(src_pin)) {
        return 0;
    }

    for (pass = 0; pass < 2; pass++) {
        int prefer_off_chip = (pass == 0);
        best_chip = -1;
        best_pin = -1;
        best_dist = 999999;
        for (ci = 0; ci < ui->chip_count; ci++) {
            const R01sEntity *e = ui->chips[ci];
            int pi;
            if (!e || e->visual != R01S_ENTITY_VIS_IC || ui_chip_hidden(ui, e)) {
                continue;
            }
            for (pi = 0; pi < e->pin_count; pi++) {
                if (ci == chip_i && pi == pin_i) {
                    continue;
                }
                if (!ui_pin_dip_package(e, pi)) {
                    continue;
                }
                if (!pin_signals_match(src_pin, &e->pins[pi])) {
                    continue;
                }
                (void)ui_pin_net_pick_peer(ui, src, pin_i, sx, sy, ci, pi, &best_chip, &best_pin, &best_dist,
                                           prefer_off_chip);
            }
        }
        if (best_chip >= 0) {
            break;
        }
    }

    if (best_chip < 0) {
        return 0;
    }
    if (peer_chip_out) {
        *peer_chip_out = best_chip;
    }
    if (peer_pin_out) {
        *peer_pin_out = best_pin;
    }
    return 1;
}

int ui_pin_net_peer(const R01sUi *ui, int chip_i, int pin_i, int *peer_chip_out, int *peer_pin_out) {
    R01sPinNetlist *nl = ui_board_pin_net(ui);
    const R01sEntity *src;
    const R01sPin *src_pin;
    int src_slot;
    int sx;
    int sy;

    if (peer_chip_out) {
        *peer_chip_out = -1;
    }
    if (peer_pin_out) {
        *peer_pin_out = -1;
    }
    if (!ui || !nl || chip_i < 0 || chip_i >= ui->chip_count || pin_i < 0) {
        return 0;
    }
    src = ui->chips[chip_i];
    if (!src || pin_i >= src->pin_count) {
        return 0;
    }
    src_pin = &src->pins[pin_i];
    if (pin_skip_wire(src_pin)) {
        return 0;
    }
    if (!ui_chip_pin_screen_center(ui, src, pin_i, &sx, &sy)) {
        return 0;
    }

    src_slot = r01s_pin_netlist_find_slot(nl, src, pin_i);
    if (src_slot >= 0 &&
        ui_pin_net_peer_union(nl, ui, src, pin_i, src_slot, sx, sy, peer_chip_out, peer_pin_out)) {
        if (ui->wire_mode == R01S_WIRE_MANUAL && peer_chip_out && peer_pin_out &&
            !ui_bb_pins_connected(ui, chip_i, pin_i, *peer_chip_out, *peer_pin_out)) {
            if (peer_chip_out) {
                *peer_chip_out = -1;
            }
            if (peer_pin_out) {
                *peer_pin_out = -1;
            }
            return 0;
        }
        return 1;
    }
    if (!ui_pin_net_peer_by_name(ui, src, pin_i, chip_i, sx, sy, peer_chip_out, peer_pin_out)) {
        return 0;
    }
    if (ui->wire_mode == R01S_WIRE_MANUAL && peer_chip_out && peer_pin_out &&
        !ui_bb_pins_connected(ui, chip_i, pin_i, *peer_chip_out, *peer_pin_out)) {
        if (peer_chip_out) {
            *peer_chip_out = -1;
        }
        if (peer_pin_out) {
            *peer_pin_out = -1;
        }
        return 0;
    }
    return 1;
}

static void draw_wire_hseg(SDL_Renderer *r, int x0, int x1, int y, Uint8 cr, Uint8 cg, Uint8 cb) {
    int xa;
    int xb;
    if (!r || x0 == x1) {
        return;
    }
    xa = x0 < x1 ? x0 : x1;
    xb = x0 < x1 ? x1 : x0;
    fill_rect(r, xa, y, xb - xa + 1, 1, cr, cg, cb);
}

static void draw_wire_vseg(SDL_Renderer *r, int x, int y0, int y1, Uint8 cr, Uint8 cg, Uint8 cb) {
    int ya;
    int yb;
    if (!r || y0 == y1) {
        return;
    }
    ya = y0 < y1 ? y0 : y1;
    yb = y0 < y1 ? y1 : y0;
    fill_rect(r, x, ya, 1, yb - ya + 1, cr, cg, cb);
}

static void ui_draw_pin_wire(SDL_Renderer *r, const R01sEntity *e0, int x0, int y0, const R01sEntity *e1,
                             int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb) {
    int h0;
    int h1;
    int mid_x;
    int mid_y;

    if (!r || !e0 || !e1) {
        return;
    }
    mid_x = (x0 + x1) / 2;
    mid_y = (y0 + y1) / 2;
    h0 = r01s_orient_is_horiz(e0->orient);
    h1 = r01s_orient_is_horiz(e1->orient);

    if (h0 && h1) {
        draw_wire_vseg(r, x0, y0, mid_y, cr, cg, cb);
        draw_wire_hseg(r, x0, x1, mid_y, cr, cg, cb);
        draw_wire_vseg(r, x1, mid_y, y1, cr, cg, cb);
    } else if (!h0 && !h1) {
        draw_wire_hseg(r, x0, mid_x, y0, cr, cg, cb);
        draw_wire_vseg(r, mid_x, y0, y1, cr, cg, cb);
        draw_wire_hseg(r, mid_x, x1, y1, cr, cg, cb);
    } else if (h0 && !h1) {
        draw_wire_vseg(r, x0, y0, mid_y, cr, cg, cb);
        draw_wire_hseg(r, x0, mid_x, mid_y, cr, cg, cb);
        draw_wire_vseg(r, mid_x, mid_y, y1, cr, cg, cb);
        draw_wire_hseg(r, mid_x, x1, y1, cr, cg, cb);
    } else {
        draw_wire_hseg(r, x0, mid_x, y0, cr, cg, cb);
        draw_wire_vseg(r, mid_x, y0, mid_y, cr, cg, cb);
        draw_wire_hseg(r, mid_x, x1, mid_y, cr, cg, cb);
        draw_wire_vseg(r, x1, mid_y, y1, cr, cg, cb);
    }
}

/* Draw one Manhattan wire from (chip_i, pin_i) to (peer_chip, peer_pin). */
static void ui_draw_pin_wire_pair(SDL_Renderer *r, const R01sUi *ui, int chip_i, int pin_i, int peer_chip,
                                  int peer_pin) {
    const R01sEntity *src;
    const R01sEntity *peer;
    int x0;
    int y0;
    int x1;
    int y1;
    Uint8 cr;
    Uint8 cg;
    Uint8 cb;

    if (!ui || chip_i < 0 || chip_i >= ui->chip_count || peer_chip < 0 || peer_chip >= ui->chip_count) {
        return;
    }
    src = ui->chips[chip_i];
    peer = ui->chips[peer_chip];
    if (!src || !peer || pin_i < 0 || pin_i >= src->pin_count || peer_pin < 0 ||
        peer_pin >= peer->pin_count) {
        return;
    }
    if (!ui_chip_pin_screen_center(ui, src, pin_i, &x0, &y0)) {
        return;
    }
    if (!ui_chip_pin_screen_center(ui, peer, peer_pin, &x1, &y1)) {
        return;
    }
    ui_chip_pin_rgb(ui, src->pins[pin_i].level, src->pins[pin_i].dir, &cr, &cg, &cb);
    ui_draw_pin_wire(r, src, x0, y0, peer, x1, y1, cr, cg, cb);
}

/*
 * For one source pin, draw a wire to every other IC on the same net (closest pin
 * on that IC) and mark those ICs in connected[].
 */
static void ui_draw_pin_peers_all(SDL_Renderer *r, const R01sUi *ui, int chip_i, int pin_i,
                                  uint8_t *connected) {
    R01sPinNetlist *nl = ui_board_pin_net(ui);
    const R01sEntity *src;
    const R01sPin *src_pin;
    int src_slot;
    int sx;
    int sy;
    int root = -1;
    int peer_best_pin[R01S_BOARD_MAX_CHIPS];
    int peer_best_dist[R01S_BOARD_MAX_CHIPS];
    uint8_t peer_hit[R01S_BOARD_MAX_CHIPS];
    int i;
    int ci;

    if (!ui || !nl || !connected || chip_i < 0 || chip_i >= ui->chip_count || pin_i < 0) {
        return;
    }
    src = ui->chips[chip_i];
    if (!src || pin_i >= src->pin_count) {
        return;
    }
    src_pin = &src->pins[pin_i];
    if (pin_skip_wire(src_pin) || !ui_pin_dip_package(src, pin_i)) {
        return;
    }
    if (!ui_chip_pin_screen_center(ui, src, pin_i, &sx, &sy)) {
        return;
    }

    memset(peer_hit, 0, sizeof(peer_hit));
    for (i = 0; i < R01S_BOARD_MAX_CHIPS; i++) {
        peer_best_pin[i] = -1;
        peer_best_dist[i] = 999999;
    }

    src_slot = r01s_pin_netlist_find_slot(nl, src, pin_i);
    if (src_slot >= 0) {
        root = r01s_pin_netlist_root(nl, src_slot);
    }

    if (root >= 0) {
        for (i = 0; i < nl->slot_count; i++) {
            const R01sEntity *peer_e;
            int peer_chip;
            int peer_pin;
            int px;
            int py;
            int dx;
            int dy;
            int dist;

            if (r01s_pin_netlist_root(nl, i) != root || i == src_slot) {
                continue;
            }
            peer_e = nl->slots[i].entity;
            peer_pin = nl->slots[i].pin_index;
            if (!peer_e || peer_e == src || !ui_pin_dip_package(peer_e, peer_pin)) {
                continue;
            }
            peer_chip = ui_chip_index(ui, peer_e);
            if (peer_chip < 0 || peer_chip >= R01S_BOARD_MAX_CHIPS) {
                continue;
            }
            if (ui->wire_mode == R01S_WIRE_MANUAL &&
                !ui_bb_pins_connected(ui, chip_i, pin_i, peer_chip, peer_pin)) {
                continue;
            }
            if (!ui_chip_pin_screen_center(ui, peer_e, peer_pin, &px, &py)) {
                continue;
            }
            dx = px - sx;
            dy = py - sy;
            dist = dx * dx + dy * dy;
            peer_hit[peer_chip] = 1;
            if (dist < peer_best_dist[peer_chip]) {
                peer_best_dist[peer_chip] = dist;
                peer_best_pin[peer_chip] = peer_pin;
            }
        }
    } else {
        /* Name-match fallback when the pin is not in the union-find graph. */
        for (ci = 0; ci < ui->chip_count; ci++) {
            const R01sEntity *e = ui->chips[ci];
            int pi;
            if (!e || e == src || e->visual != R01S_ENTITY_VIS_IC || ui_chip_hidden(ui, e)) {
                continue;
            }
            for (pi = 0; pi < e->pin_count; pi++) {
                int px;
                int py;
                int dx;
                int dy;
                int dist;
                if (!ui_pin_dip_package(e, pi) || !pin_signals_match(src_pin, &e->pins[pi])) {
                    continue;
                }
                if (!ui_chip_pin_screen_center(ui, e, pi, &px, &py)) {
                    continue;
                }
                dx = px - sx;
                dy = py - sy;
                dist = dx * dx + dy * dy;
                peer_hit[ci] = 1;
                if (dist < peer_best_dist[ci]) {
                    peer_best_dist[ci] = dist;
                    peer_best_pin[ci] = pi;
                }
            }
        }
    }

    for (ci = 0; ci < ui->chip_count && ci < R01S_BOARD_MAX_CHIPS; ci++) {
        if (!peer_hit[ci] || peer_best_pin[ci] < 0) {
            continue;
        }
        connected[ci] = 1;
        if (r) {
            ui_draw_pin_wire_pair(r, ui, chip_i, pin_i, ci, peer_best_pin[ci]);
        }
    }
}

/* Count distinct ICs connected to chip_i; optionally draw every off-chip wire. */
int ui_ic_connected_peers(SDL_Renderer *r, const R01sUi *ui, int chip_i) {
    const R01sEntity *src;
    uint8_t connected[R01S_BOARD_MAX_CHIPS];
    int pi;
    int ci;
    int count = 0;

    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return 0;
    }
    src = ui->chips[chip_i];
    if (!src || src->visual != R01S_ENTITY_VIS_IC) {
        return 0;
    }

    memset(connected, 0, sizeof(connected));
    for (pi = 0; pi < src->pin_count; pi++) {
        ui_draw_pin_peers_all(r, ui, chip_i, pi, connected);
    }
    for (ci = 0; ci < ui->chip_count && ci < R01S_BOARD_MAX_CHIPS; ci++) {
        if (connected[ci]) {
            count++;
        }
    }
    return count;
}

/* Ctrl+hover IC body: all off-chip connection wires. */
static int ui_draw_ic_connection_overlay(SDL_Renderer *r, R01sUi *ui) {
    int chip_i = -1;
    int kind;
    const R01sEntity *src;

    if (!ui || !r) {
        return 0;
    }
    if (!(SDL_GetModState() & KMOD_CTRL)) {
        return 0;
    }
    if (!ui_logic_in_view(ui->mouse_lx, ui->mouse_ly)) {
        return 0;
    }
    kind = hit_board_top(ui, ui->mouse_lx, ui->mouse_ly, &chip_i, NULL, NULL);
    if (kind != 1 || chip_i < 0 || chip_i >= ui->chip_count) {
        return 0;
    }
    src = ui->chips[chip_i];
    if (!src || src->visual != R01S_ENTITY_VIS_IC) {
        return 0;
    }
    (void)ui_ic_connected_peers(r, ui, chip_i);
    return 1;
}

void ui_draw_pin_wire_overlay(SDL_Renderer *r, R01sUi *ui) {
    int chip_i;
    int pin_i;
    int peer_chip;
    int peer_pin;

    if (!ui || !r) {
        return;
    }
    if (ui_draw_ic_connection_overlay(r, ui)) {
        return;
    }
    if (!ui_hit_chip_pin(ui, ui->mouse_lx, ui->mouse_ly, &chip_i, &pin_i)) {
        return;
    }
    if (!ui_pin_net_peer(ui, chip_i, pin_i, &peer_chip, &peer_pin)) {
        return;
    }
    ui_draw_pin_wire_pair(r, ui, chip_i, pin_i, peer_chip, peer_pin);
}
