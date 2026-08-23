#include "retr01_sim/traces.h"

#include "retr01_sim/bus.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void r01s_traces_clear(R01sTraceMap *m) {
    if (m) {
        memset(m, 0, sizeof(*m));
    }
}

int r01s_traces_add(R01sTraceMap *m, R01sEntity *ea, const char *pin_a, R01sEntity *eb, const char *pin_b,
                    int net_id, int lane) {
    R01sTraceLink *L;
    if (!m || !ea || !eb || !pin_a || !pin_b || m->link_count >= R01S_TRACE_MAX_LINKS) {
        return -1;
    }
    L = &m->links[m->link_count++];
    L->ea = ea;
    L->eb = eb;
    L->pin_a = pin_a;
    L->pin_b = pin_b;
    L->net_id = net_id;
    L->lane = lane;
    return 0;
}

int r01s_pin_tip_xy(const R01sEntity *e, const char *pin_name, int *ox, int *oy) {
    int i, rows, pitch, num, side_left, idx;
    int x, y, py;
    if (!e || !pin_name || !ox || !oy) {
        return -1;
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].name && strcmp(e->pins[i].name, pin_name) == 0) {
            break;
        }
    }
    if (i >= e->pin_count) {
        return -1;
    }
    rows = (e->pin_count + 1) / 2;
    if (rows < 1) {
        rows = 1;
    }
    pitch = rows > 1 ? (e->body_h - 16) / (rows - 1) : 0;
    num = e->pins[i].number;
    side_left = num <= rows;
    idx = side_left ? (num - 1) : (e->pin_count - num);
    if (idx < 0) {
        idx = 0;
    }
    x = e->board_x;
    y = e->board_y;
    py = y + 8 + idx * pitch;
    if (side_left) {
        *ox = x - 10;
    } else {
        *ox = x + e->body_w + 10;
    }
    *oy = py;
    return 0;
}

static int push_seg(R01sTraceMap *m, int x0, int y0, int x1, int y1, int net_id, R01sLevel level) {
    R01sTraceSeg *s;
    if (m->seg_count >= R01S_TRACE_MAX_SEGS) {
        return -1;
    }
    if (x0 == x1 && y0 == y1) {
        return 0;
    }
    /* Normalize direction for stable crossing tests. */
    if (x0 > x1 || (x0 == x1 && y0 > y1)) {
        int t;
        t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
    }
    s = &m->segs[m->seg_count++];
    s->x0 = x0;
    s->y0 = y0;
    s->x1 = x1;
    s->y1 = y1;
    s->net_id = net_id;
    s->level = level;
    return 0;
}

static void route_link(R01sTraceMap *m, const R01sTraceLink *L) {
    int ax, ay, bx, by;
    int a_out_x, b_out_x;
    int mid_x, lane_y;
    R01sLevel lvl;
    int a_left, b_left;

    if (r01s_pin_tip_xy(L->ea, L->pin_a, &ax, &ay) != 0) {
        return;
    }
    if (r01s_pin_tip_xy(L->eb, L->pin_b, &bx, &by) != 0) {
        return;
    }
    lvl = r01s_entity_sense(L->ea, L->pin_a);
    if (lvl == R01S_LVL_Z) {
        lvl = r01s_entity_sense(L->eb, L->pin_b);
    }

    /* Exit horizontally away from each package body. */
    a_left = ax < L->ea->board_x + L->ea->body_w / 2;
    b_left = bx < L->eb->board_x + L->eb->body_w / 2;
    a_out_x = a_left ? ax - 14 - L->lane * 2 : ax + 14 + L->lane * 2;
    b_out_x = b_left ? bx - 14 - L->lane * 2 : bx + 14 + L->lane * 2;

    /* Shared vertical channel between chips, offset by lane. */
    mid_x = (a_out_x + b_out_x) / 2 + L->lane * 3;
    lane_y = (ay + by) / 2; /* unused; keep Manhattan H-V-H */

    (void)lane_y;
    push_seg(m, ax, ay, a_out_x, ay, L->net_id, lvl);
    push_seg(m, a_out_x, ay, mid_x, ay, L->net_id, lvl);
    push_seg(m, mid_x, ay, mid_x, by, L->net_id, lvl);
    push_seg(m, mid_x, by, b_out_x, by, L->net_id, lvl);
    push_seg(m, b_out_x, by, bx, by, L->net_id, lvl);
}

void r01s_traces_rebuild(R01sTraceMap *m) {
    int i;
    if (!m) {
        return;
    }
    m->seg_count = 0;
    for (i = 0; i < m->link_count; i++) {
        route_link(m, &m->links[i]);
    }
}

static void level_rgb(R01sLevel lvl, Uint8 *R, Uint8 *G, Uint8 *B) {
    switch (lvl) {
    case R01S_LVL_H:
        *R = 90;
        *G = 210;
        *B = 110;
        break;
    case R01S_LVL_L:
        *R = 70;
        *G = 90;
        *B = 80;
        break;
    case R01S_LVL_X:
        *R = 220;
        *G = 90;
        *B = 180;
        break;
    default:
        *R = 100;
        *G = 120;
        *B = 110;
        break;
    }
}

static int seg_is_h(const R01sTraceSeg *s) {
    return s->y0 == s->y1;
}

static int seg_is_v(const R01sTraceSeg *s) {
    return s->x0 == s->x1;
}

static int cross_hv(const R01sTraceSeg *h, const R01sTraceSeg *v, int *cx, int *cy) {
    /* h horizontal at y=h->y0, x in [x0,x1]; v vertical at x=v->x0, y in [y0,y1] */
    int hx0 = h->x0, hx1 = h->x1, hy = h->y0;
    int vx = v->x0, vy0 = v->y0, vy1 = v->y1;
    if (hx0 > hx1) {
        int t = hx0;
        hx0 = hx1;
        hx1 = t;
    }
    if (vy0 > vy1) {
        int t = vy0;
        vy0 = vy1;
        vy1 = t;
    }
    if (vx <= hx0 || vx >= hx1) {
        return 0;
    }
    if (hy <= vy0 || hy >= vy1) {
        return 0;
    }
    /* Skip if shared endpoint (T / join). */
    if ((vx == hx0 || vx == hx1) && (hy == vy0 || hy == vy1)) {
        return 0;
    }
    *cx = vx;
    *cy = hy;
    return 1;
}

static int cmp_int(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

static void draw_h_jumps(SDL_Renderer *r, int x0, int x1, int y, const int *jumps, int nj) {
    int xs[64];
    int n = 0, i, cur;
    int jr = R01S_TRACE_JUMP_R;

    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (i = 0; i < nj && n < 64; i++) {
        if (jumps[i] > x0 + jr && jumps[i] < x1 - jr) {
            xs[n++] = jumps[i];
        }
    }
    qsort(xs, (size_t)n, sizeof(int), cmp_int);

    cur = x0;
    for (i = 0; i < n; i++) {
        int jx = xs[i];
        int k;
        int px0, py0;
        if (jx - jr > cur) {
            SDL_RenderDrawLine(r, cur, y, jx - jr, y);
        }
        px0 = jx - jr;
        py0 = y;
        for (k = 1; k <= 10; k++) {
            double ang = M_PI * (double)k / 10.0;
            int px = jx - (int)(cos(ang) * (double)jr);
            int py = y - (int)(sin(ang) * (double)jr);
            SDL_RenderDrawLine(r, px0, py0, px, py);
            px0 = px;
            py0 = py;
        }
        cur = jx + jr;
    }
    if (cur < x1) {
        SDL_RenderDrawLine(r, cur, y, x1, y);
    }
}

static void draw_v_plain(SDL_Renderer *r, int x, int y0, int y1) {
    SDL_RenderDrawLine(r, x, y0, x, y1);
}

void r01s_traces_draw(const R01sTraceMap *m, SDL_Renderer *r, int pan_x, int pan_y) {
    int i, j;
    if (!m || !r) {
        return;
    }
    for (i = 0; i < m->seg_count; i++) {
        const R01sTraceSeg *s = &m->segs[i];
        Uint8 R, G, B;
        int x0 = s->x0 - pan_x, y0 = s->y0 - pan_y;
        int x1 = s->x1 - pan_x, y1 = s->y1 - pan_y;
        level_rgb(s->level, &R, &G, &B);
        SDL_SetRenderDrawColor(r, R, G, B, 255);

        if (seg_is_v(s)) {
            draw_v_plain(r, x0, y0, y1);
            continue;
        }
        if (seg_is_h(s)) {
            int jumps[64];
            int nj = 0;
            /* Crossings with earlier vertical segments of other nets */
            for (j = 0; j < m->seg_count && nj < 64; j++) {
                const R01sTraceSeg *o = &m->segs[j];
                int cx, cy;
                R01sTraceSeg h = *s, v = *o;
                if (!seg_is_v(o) || o->net_id == s->net_id) {
                    continue;
                }
                /* Compare in board space */
                h.x0 = s->x0;
                h.x1 = s->x1;
                h.y0 = s->y0;
                h.y1 = s->y1;
                if (cross_hv(&h, &v, &cx, &cy)) {
                    jumps[nj++] = cx - pan_x;
                }
            }
            if (nj == 0) {
                SDL_RenderDrawLine(r, x0, y0, x1, y1);
            } else {
                draw_h_jumps(r, x0, x1, y0, jumps, nj);
            }
        }
    }
}
