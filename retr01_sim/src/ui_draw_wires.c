#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/netlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int abs_i(int v) {
    return v < 0 ? -v : v;
}

static int ref_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static int route_matches(const R01sWireRoute *w, const char *ra, const char *pa, const char *rb,
                         const char *pb) {
    if (ref_eq(w->ref_a, ra) && ref_eq(w->pin_a, pa) && ref_eq(w->ref_b, rb) && ref_eq(w->pin_b, pb)) {
        return 1;
    }
    if (ref_eq(w->ref_a, rb) && ref_eq(w->pin_a, pb) && ref_eq(w->ref_b, ra) && ref_eq(w->pin_b, pa)) {
        return 1;
    }
    return 0;
}

R01sWireRoute *ui_wire_find(R01sUi *ui, const char *ref_a, const char *pin_a, const char *ref_b,
                            const char *pin_b) {
    int i;
    if (!ui) {
        return NULL;
    }
    for (i = 0; i < ui->wire_count; i++) {
        if (route_matches(&ui->wires[i], ref_a, pin_a, ref_b, pin_b)) {
            return &ui->wires[i];
        }
    }
    return NULL;
}

R01sWireRoute *ui_wire_ensure(R01sUi *ui, const R01sNetEdge *edge) {
    R01sWireRoute *w;
    if (!ui || !edge || !edge->a || !edge->b || !edge->a->refdes || !edge->b->refdes) {
        return NULL;
    }
    w = ui_wire_find(ui, edge->a->refdes, edge->pin_a, edge->b->refdes, edge->pin_b);
    if (w) {
        return w;
    }
    if (ui->wire_count >= R01S_WIRE_MAX_EDGES) {
        return NULL;
    }
    w = &ui->wires[ui->wire_count++];
    memset(w, 0, sizeof(*w));
    snprintf(w->ref_a, sizeof(w->ref_a), "%s", edge->a->refdes);
    snprintf(w->pin_a, sizeof(w->pin_a), "%s", edge->pin_a);
    snprintf(w->ref_b, sizeof(w->ref_b), "%s", edge->b->refdes);
    snprintf(w->pin_b, sizeof(w->pin_b), "%s", edge->pin_b);
    return w;
}

/* Orthodiagonal auto path: diagonal min(|dx|,|dy|) then finish H or V. */
static int auto_path_points(int x0, int y0, int x1, int y1, int *xs, int *ys, int max_pts) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = abs_i(dx);
    int ady = abs_i(dy);
    int n = 0;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    int diag;

    if (max_pts < 2) {
        return 0;
    }
    xs[n] = x0;
    ys[n] = y0;
    n++;
    if (dx == 0 || dy == 0 || adx == ady) {
        xs[n] = x1;
        ys[n] = y1;
        n++;
        return n;
    }
    diag = adx < ady ? adx : ady;
    if (n < max_pts) {
        xs[n] = x0 + sx * diag;
        ys[n] = y0 + sy * diag;
        n++;
    }
    if (n < max_pts) {
        xs[n] = x1;
        ys[n] = y1;
        n++;
    }
    return n;
}

static int wire_polyline(R01sUi *ui, const R01sWireRoute *route, int ax, int ay, int bx, int by, int *xs,
                         int *ys, int max_pts) {
    int n = 0;
    int i;
    (void)ui;
    if (max_pts < 2) {
        return 0;
    }
    xs[n] = ax;
    ys[n] = ay;
    n++;
    if (route) {
        for (i = 0; i < route->nverts && n < max_pts - 1; i++) {
            xs[n] = route->vx[i];
            ys[n] = route->vy[i];
            n++;
        }
    }
    if (!route || route->nverts < 1) {
        /* Replace with auto path (endpoints only so far). */
        return auto_path_points(ax, ay, bx, by, xs, ys, max_pts);
    }
    if (n < max_pts) {
        xs[n] = bx;
        ys[n] = by;
        n++;
    }
    return n;
}

static void draw_seg(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_RenderDrawLine(r, x0, y0, x1, y1);
    /* Slight thicken for visibility. */
    if (x0 == x1) {
        SDL_RenderDrawLine(r, x0 + 1, y0, x1 + 1, y1);
    } else if (y0 == y1) {
        SDL_RenderDrawLine(r, x0, y0 + 1, x1, y1 + 1);
    } else {
        SDL_RenderDrawLine(r, x0 + 1, y0, x1 + 1, y1);
    }
}

static R01sLevel net_level(const R01sEntity *a, const char *pa, const R01sEntity *b, const char *pb) {
    R01sLevel la = r01s_entity_sense(a, pa);
    R01sLevel lb = r01s_entity_sense(b, pb);
    if (la == R01S_LVL_X || lb == R01S_LVL_X) {
        return R01S_LVL_X;
    }
    if (la == R01S_LVL_H || lb == R01S_LVL_H) {
        return R01S_LVL_H;
    }
    if (la == R01S_LVL_L || lb == R01S_LVL_L) {
        return R01S_LVL_L;
    }
    return R01S_LVL_Z;
}

void ui_draw_connections(R01sUi *ui, SDL_Renderer *r) {
    int ei;
    Uint32 ticks;
    int pulse;
    if (!ui || !ui_layout_conn(ui) || !r) {
        return;
    }
    ticks = SDL_GetTicks();
    pulse = (int)((ticks / 120u) & 1u);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (ei = 0; ei < r01s_net_edge_count(); ei++) {
        const R01sNetEdge *edge = r01s_net_edge_at(ei);
        R01sEntity *ea;
        R01sEntity *eb;
        R01sWireRoute *route;
        int ax, ay, bx, by;
        int xs[R01S_WIRE_MAX_VERTS + 4];
        int ys[R01S_WIRE_MAX_VERTS + 4];
        int n;
        int i;
        R01sLevel lvl;
        Uint8 pr, pg, pb;
        Uint8 alpha;
        R01sPinDir dir = R01S_PIN_IO;

        if (!edge || !edge->a || !edge->b) {
            continue;
        }
        ea = edge->a;
        eb = edge->b;
        if (ea->visual == R01S_ENTITY_VIS_NONE || eb->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        if (!r01s_ui_chip_pin_tip(ea, edge->pin_a, &ax, &ay) ||
            !r01s_ui_chip_pin_tip(eb, edge->pin_b, &bx, &by)) {
            continue;
        }
        route = ui_wire_ensure(ui, edge);
        n = wire_polyline(ui, route, ax, ay, bx, by, xs, ys, (int)(sizeof(xs) / sizeof(xs[0])));
        if (n < 2) {
            continue;
        }
        lvl = net_level(ea, edge->pin_a, eb, edge->pin_b);
        pin_level_rgb(lvl, dir, &pr, &pg, &pb);
        alpha = 160;
        if (lvl == R01S_LVL_H) {
            alpha = pulse ? 255 : 140; /* blink when driven high */
        } else if (lvl == R01S_LVL_X) {
            alpha = pulse ? 230 : 120;
        }
        for (i = 0; i + 1 < n; i++) {
            int x0 = ui_board_sx(ui, xs[i]);
            int y0 = ui_board_sy(ui, ys[i]);
            int x1 = ui_board_sx(ui, xs[i + 1]);
            int y1 = ui_board_sy(ui, ys[i + 1]);
            draw_seg(r, x0, y0, x1, y1, pr, pg, pb, alpha);
        }
        /* Vertex handles */
        if (route) {
            for (i = 0; i < route->nverts; i++) {
                int hx = ui_board_sx(ui, route->vx[i]);
                int hy = ui_board_sy(ui, route->vy[i]);
                fill_rect(r, hx - 2, hy - 2, 4, 4, 240, 240, 100);
            }
        }
    }
}

static int dist2_point_seg(int px, int py, int x0, int y0, int x1, int y1, int *out_tx, int *out_ty) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int len2 = dx * dx + dy * dy;
    int tnum;
    int tx, ty;
    if (len2 < 1) {
        tx = x0;
        ty = y0;
    } else {
        tnum = (px - x0) * dx + (py - y0) * dy;
        if (tnum < 0) {
            tx = x0;
            ty = y0;
        } else if (tnum > len2) {
            tx = x1;
            ty = y1;
        } else {
            tx = x0 + (int)((long)dx * tnum / len2);
            ty = y0 + (int)((long)dy * tnum / len2);
        }
    }
    if (out_tx) {
        *out_tx = tx;
    }
    if (out_ty) {
        *out_ty = ty;
    }
    {
        int ex = px - tx;
        int ey = py - ty;
        return ex * ex + ey * ey;
    }
}

static int snap_ortho45(int from_x, int from_y, int *x, int *y) {
    int dx = *x - from_x;
    int dy = *y - from_y;
    int adx = abs_i(dx);
    int ady = abs_i(dy);
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    if (adx < 3) {
        *x = from_x;
        return 1;
    }
    if (ady < 3) {
        *y = from_y;
        return 1;
    }
    /* Prefer 45 when close. */
    if (abs_i(adx - ady) <= 4) {
        int d = (adx + ady) / 2;
        *x = from_x + sx * d;
        *y = from_y + sy * d;
        return 1;
    }
    if (adx > ady) {
        *y = from_y;
    } else {
        *x = from_x;
    }
    return 1;
}

int ui_conn_ctrl_click(R01sUi *ui, int board_x, int board_y) {
    int ei;
    int best_d2 = 12 * 12; /* ~12px hit radius in board space */
    int best_edge = -1;
    int best_seg = -1;
    int best_tx = 0;
    int best_ty = 0;
    R01sWireRoute *route;
    const R01sNetEdge *edge;
    int ax, ay, bx, by;
    int xs[R01S_WIRE_MAX_VERTS + 4];
    int ys[R01S_WIRE_MAX_VERTS + 4];
    int n;
    int i;

    if (!ui || !ui_layout_conn(ui)) {
        return 0;
    }

    for (ei = 0; ei < r01s_net_edge_count(); ei++) {
        edge = r01s_net_edge_at(ei);
        if (!edge || !edge->a || !edge->b) {
            continue;
        }
        if (!r01s_ui_chip_pin_tip(edge->a, edge->pin_a, &ax, &ay) ||
            !r01s_ui_chip_pin_tip(edge->b, edge->pin_b, &bx, &by)) {
            continue;
        }
        route = ui_wire_ensure(ui, edge);
        n = wire_polyline(ui, route, ax, ay, bx, by, xs, ys, (int)(sizeof(xs) / sizeof(xs[0])));
        for (i = 0; i + 1 < n; i++) {
            int tx, ty;
            int d2 = dist2_point_seg(board_x, board_y, xs[i], ys[i], xs[i + 1], ys[i + 1], &tx, &ty);
            if (d2 < best_d2) {
                best_d2 = d2;
                best_edge = ei;
                best_seg = i;
                best_tx = tx;
                best_ty = ty;
            }
        }
    }
    if (best_edge < 0 || best_seg < 0) {
        return 0;
    }
    edge = r01s_net_edge_at(best_edge);
    route = ui_wire_ensure(ui, edge);
    if (!route || route->nverts >= R01S_WIRE_MAX_VERTS) {
        snprintf(ui->status, sizeof(ui->status), "wire vertex limit");
        return 1;
    }
    /* Snap toward H/V/45 relative to previous polyline point. */
    {
        int prev_x, prev_y;
        if (!r01s_ui_chip_pin_tip(edge->a, edge->pin_a, &ax, &ay) ||
            !r01s_ui_chip_pin_tip(edge->b, edge->pin_b, &bx, &by)) {
            return 1;
        }
        n = wire_polyline(ui, route, ax, ay, bx, by, xs, ys, (int)(sizeof(xs) / sizeof(xs[0])));
        if (best_seg < 0 || best_seg >= n) {
            return 1;
        }
        prev_x = xs[best_seg];
        prev_y = ys[best_seg];
        snap_ortho45(prev_x, prev_y, &best_tx, &best_ty);
        best_tx = r01s_grid_snap(best_tx);
        best_ty = r01s_grid_snap(best_ty);
    }
    /* Insert intermediate vertex. Empty routes: single bend at click. */
    {
        int insert_at = best_seg;
        if (route->nverts < 1) {
            insert_at = 0;
        } else if (insert_at > route->nverts) {
            insert_at = route->nverts;
        }
        for (i = route->nverts; i > insert_at; i--) {
            route->vx[i] = route->vx[i - 1];
            route->vy[i] = route->vy[i - 1];
        }
        route->vx[insert_at] = best_tx;
        route->vy[insert_at] = best_ty;
        route->nverts++;
    }
    ui->layout_dirty = 1;
    snprintf(ui->status, sizeof(ui->status), "wire vertex added (%d)", route->nverts);
    return 1;
}
