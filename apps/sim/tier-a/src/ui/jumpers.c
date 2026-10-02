#include "r01a_ui.h"

#include <string.h>

static const Uint8 k_jumper_rgb[R01A_JUMPER_COLORS][3] = {
    {220, 50, 50},  {230, 140, 40}, {230, 200, 50}, {50, 180, 70},  {50, 120, 220},
    {160, 70, 200}, {50, 50, 50},   {230, 230, 230}, {150, 95, 55}, {150, 150, 155},
    {220, 160, 40},
};

void jumper_wheel_color(R01aUi *ui, int delta) {
    int n = R01A_JUMPER_COLORS;
    if (!ui || n <= 0) {
        return;
    }
    ui->jumper_color_i = (ui->jumper_color_i + delta) % n;
    if (ui->jumper_color_i < 0) {
        ui->jumper_color_i += n;
    }
}

void jumper_arm_rgb(const R01aUi *ui, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i = ui ? ui->jumper_color_i : 0;
    if (i < 0 || i >= R01A_JUMPER_COLORS) {
        i = 0;
    }
    if (r) {
        *r = k_jumper_rgb[i][0];
    }
    if (g) {
        *g = k_jumper_rgb[i][1];
    }
    if (b) {
        *b = k_jumper_rgb[i][2];
    }
}

static int jumper_hole_free(const R01aUi *ui, const NsBreadboard *bb, NsPbHole hole) {
    int wx;
    int wy;
    if (!ui || !bb || !ns_breadboard_hole_exists(hole)) {
        return 0;
    }
    ns_breadboard_hole_world(bb, hole, &wx, &wy);
    return !chip_pin_on_hole(ui, bb, hole);
}

static int manhattan_h_first(int ax, int ay, int bx, int by) {
    int adx = bx - ax;
    int ady = by - ay;
    if (adx < 0) {
        adx = -adx;
    }
    if (ady < 0) {
        ady = -ady;
    }
    return adx >= ady;
}

static void elbow_pts(int ax, int ay, int bx, int by, int h_first, int jog, int *x, int *y) {
    int x1;
    int y1;
    int x2;
    int y2;
    x[0] = ax;
    y[0] = ay;
    x[3] = bx;
    y[3] = by;
    if (ax == bx && ay == by) {
        x[1] = ax;
        y[1] = ay;
        x[2] = bx;
        y[2] = by;
        return;
    }
    if (jog == 0) {
        jog = 6;
    }
    if (h_first && ay != by) {
        x1 = (ax + bx) / 2;
        if (x1 == ax || x1 == bx) {
            x1 = ax + (bx >= ax ? jog : -jog);
        }
        y1 = ay;
        x2 = x1;
        y2 = by;
    } else if (!h_first && ax != bx) {
        x1 = ax;
        y1 = (ay + by) / 2;
        if (y1 == ay || y1 == by) {
            y1 = ay + (by >= ay ? jog : -jog);
        }
        x2 = bx;
        y2 = y1;
    } else if (ay == by) {
        x1 = ax;
        y1 = ay + jog;
        x2 = bx;
        y2 = y1;
    } else {
        x1 = ax + jog;
        y1 = ay;
        x2 = x1;
        y2 = by;
    }
    x[1] = x1;
    y[1] = y1;
    x[2] = x2;
    y[2] = y2;
}

static int jumper_world_ends(const R01aUi *ui, const R01aJumper *j, int *x0, int *y0, int *x1, int *y1) {
    const NsBreadboard *bba;
    const NsBreadboard *bbb;
    if (!j) {
        return 0;
    }
    bba = ui_bb_named(ui, r01a_jumper_a_ref(j));
    bbb = ui_bb_named(ui, r01a_jumper_b_ref(j));
    if (!bba || !bbb) {
        return 0;
    }
    ns_breadboard_hole_world(bba, j->a, x0, y0);
    ns_breadboard_hole_world(bbb, j->b, x1, y1);
    return 1;
}

static void draw_jumper_path(SDL_Renderer *r, int ax, int ay, int bx, int by, int h_first, int mid, int route,
                             Uint8 cr, Uint8 cg, Uint8 cb) {
    int x[4];
    int y[4];
    int s;
    if (route) {
        x[0] = ax;
        y[0] = ay;
        x[3] = bx;
        y[3] = by;
        if (h_first && ay != by) {
            x[1] = mid;
            y[1] = ay;
            x[2] = mid;
            y[2] = by;
        } else {
            x[1] = ax;
            y[1] = mid;
            x[2] = bx;
            y[2] = mid;
        }
    } else {
        elbow_pts(ax, ay, bx, by, manhattan_h_first(ax, ay, bx, by), 6, x, y);
    }
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    for (s = 0; s < 3; s++) {
        draw_hard_line(r, x[s], y[s], x[s + 1], y[s + 1], cr, cg, cb);
    }
}

void draw_jumpers(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board) {
    int i;
    if (!r || !ui || !board) {
        return;
    }
    for (i = 0; i < board->jumper_count; i++) {
        const R01aJumper *j = &board->jumpers[i];
        int ax;
        int ay;
        int bx;
        int by;
        if (!jumper_world_ends(ui, j, &ax, &ay, &bx, &by)) {
            continue;
        }
        draw_jumper_path(r, board_sx(ui, ax), board_sy(ui, ay), board_sx(ui, bx), board_sy(ui, by), j->h_first,
                         j->mid, j->route, j->r, j->g, j->bcol);
    }
    if (ui->jumper_arm && ui->jumper_bb) {
        int sx;
        int sy;
        int mx;
        int my;
        NsBreadboard *hover_bb;
        NsPbHole hole;
        int ex;
        int ey;
        ns_breadboard_hole_world(ui->jumper_bb, ui->jumper_from, &sx, &sy);
        logic_to_board(ui, ui->mouse_lx, ui->mouse_ly, &mx, &my);
        hover_bb = hit_breadboard(ui, mx, my, &hole);
        if (hover_bb && jumper_hole_free(ui, hover_bb, hole)) {
            ns_breadboard_hole_world(hover_bb, hole, &ex, &ey);
        } else {
            ex = mx;
            ey = my;
        }
        {
            Uint8 cr;
            Uint8 cg;
            Uint8 cb;
            jumper_arm_rgb(ui, &cr, &cg, &cb);
            draw_jumper_path(r, board_sx(ui, sx), board_sy(ui, sy), board_sx(ui, ex), board_sy(ui, ey),
                             manhattan_h_first(sx, sy, ex, ey), (sx + ex) / 2, 0, cr, cg, cb);
        }
    }
}

void jumpers_cancel(R01aUi *ui) {
    if (!ui) {
        return;
    }
    ui->jumper_arm = 0;
    ui->jumper_bb = NULL;
}

int jumpers_try_click(R01aUi *ui, R01aBoard *board, int bx, int by, int chip_i) {
    NsBreadboard *bb;
    NsPbHole hole;
    if (!ui || !board || !ui->show_nets) {
        return 0;
    }
    if (!ui->jumper_mode && !ui->jumper_arm) {
        return 0;
    }
    bb = hit_breadboard(ui, bx, by, &hole);
    if (!bb || !jumper_hole_free(ui, bb, hole)) {
        return 0;
    }
    if (chip_i >= 0) {
        return 0;
    }
    if (ui->jumper_arm) {
        if (ui->jumper_bb == bb && ui->jumper_from.col == hole.col && ui->jumper_from.lane == hole.lane) {
            jumpers_cancel(ui);
            return 1;
        }
        if (ui->jumper_bb) {
            uint8_t cr;
            uint8_t cg;
            uint8_t cb;
            jumper_arm_rgb(ui, &cr, &cg, &cb);
            if (r01a_board_jumper_add_across(board, ui->jumper_bb, ui->jumper_from, bb, hole, cr, cg, cb)) {
                hist_after(ui);
            }
        }
        jumpers_cancel(ui);
        return 1;
    }
    ui->jumper_arm = 1;
    ui->jumper_from = hole;
    ui->jumper_bb = bb;
    return 1;
}
