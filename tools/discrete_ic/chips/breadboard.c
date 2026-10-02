#include "discrete_ic/breadboard.h"

#include <stdio.h>
#include <string.h>

static int lane_hole_y(int lane) {
    int y = NS_PB_MARGIN + lane * NS_PB_PITCH;
    if (lane > NS_PB_LANE_TOP_NEG) {
        y += NS_PB_GAP_RAIL;
    }
    if (lane > NS_PB_LANE_E) {
        y += NS_PB_GAP_TRENCH;
    }
    if (lane > NS_PB_LANE_J) {
        y += NS_PB_GAP_RAIL;
    }
    return y;
}

static int board_short_px(void) {
    int n = lane_hole_y(NS_PB_LANE_BOT_NEG) + NS_PB_HOLE + NS_PB_MARGIN;
#ifdef R01A_BB_3PX
    /* Keep W/H on the 3 px lattice so a grid-snapped board_x/y seats every hole. */
    if (n % 3) {
        n += 3 - (n % 3);
    }
#endif
    return n;
}

static int board_long_px(void) {
    int n = NS_PB_MARGIN + (NS_PB_COLS - 1) * NS_PB_PITCH + NS_PB_HOLE + NS_PB_MARGIN;
#ifdef R01A_BB_3PX
    if (n % 3) {
        n += 3 - (n % 3);
    }
#endif
    return n;
}

void ns_breadboard_body_size(NsPkgOrient orient, int *w, int *h) {
    int long_px = board_long_px();
    int short_px = board_short_px();
    if (!ns_orient_is_horiz(orient)) {
        if (w) {
            *w = short_px;
        }
        if (h) {
            *h = long_px;
        }
    } else {
        if (w) {
            *w = long_px;
        }
        if (h) {
            *h = short_px;
        }
    }
}

void ns_breadboard_sync_body(NsBreadboard *bb) {
    int w = 0;
    int h = 0;
    if (!bb) {
        return;
    }
    ns_breadboard_body_size(bb->base.orient, &w, &h);
    bb->base.body_w = w;
    bb->base.body_h = h;
}

static void bb_reset(NsEntity *e) {
    NsBreadboard *bb = (NsBreadboard *)e;
    if (bb) {
        bb->hover_valid = 0;
    }
}

static void bb_eval(NsEntity *e) {
    (void)e;
}

static void bb_tick(NsEntity *e) {
    (void)e;
}

static void bb_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable BB_VT = {
    bb_reset,
    bb_eval,
    bb_tick,
    bb_destroy,
};

void ns_breadboard_init(NsBreadboard *bb, const char *refdes) {
    int w = 0;
    int h = 0;
    if (!bb) {
        return;
    }
    memset(bb, 0, sizeof(*bb));
    snprintf(bb->refdes_buf, sizeof(bb->refdes_buf), "%s", refdes ? refdes : "BB1");
    ns_entity_init(&bb->base, &BB_VT, "BREADBOARD", bb->refdes_buf);
    bb->base.impl = bb;
    ns_breadboard_body_size(NS_ORIENT_H, &w, &h);
    ns_entity_set_glyph(&bb->base, NS_ENTITY_VIS_BREADBOARD, w, h);
    bb->base.orient = NS_ORIENT_H;
    ns_breadboard_sync_body(bb);
}

NsEntity *ns_breadboard_entity(NsBreadboard *bb) {
    return bb ? &bb->base : NULL;
}

static int rail_hole_exists(int col) {
#ifdef R01A_BB_3PX
    int local;
    if (col < 0 || col >= NS_PB_COLS) {
        return 0;
    }
    if (col >= NS_PB_RAIL_GAP_COL && col < NS_PB_RAIL_GAP_COL + NS_PB_RAIL_GAP_LEN) {
        return 0;
    }
    local = col < NS_PB_RAIL_GAP_COL ? col : (col - (NS_PB_RAIL_GAP_COL + NS_PB_RAIL_GAP_LEN));
    if (local < 0 || local >= NS_PB_RAIL_HALF) {
        return 0;
    }
    return (local % (NS_PB_RAIL_GROUP + 1)) != NS_PB_RAIL_GROUP;
#else
    if (col < NS_PB_RAIL_SEG) {
        return (col % (NS_PB_RAIL_GROUP + 1)) != NS_PB_RAIL_GROUP;
    }
    if (col >= NS_PB_RAIL_GAP_END) {
        int local = col - NS_PB_RAIL_GAP_END;
        return (local % (NS_PB_RAIL_GROUP + 1)) != NS_PB_RAIL_GROUP;
    }
    return 0;
#endif
}

int ns_breadboard_hole_exists(NsPbHole h) {
    if (h.col < 0 || h.col >= NS_PB_COLS) {
        return 0;
    }
    if (h.lane == NS_PB_LANE_TOP_POS || h.lane == NS_PB_LANE_TOP_NEG ||
        h.lane == NS_PB_LANE_BOT_POS || h.lane == NS_PB_LANE_BOT_NEG) {
        return rail_hole_exists(h.col);
    }
    if (h.lane < 0 || h.lane >= NS_PB_LANE_COUNT) {
        return 0;
    }
    return 1;
}

int ns_breadboard_strip_id(NsPbHole h) {
    if (h.lane >= NS_PB_LANE_A && h.lane <= NS_PB_LANE_E) {
        return h.col;
    }
    if (h.lane >= NS_PB_LANE_F && h.lane <= NS_PB_LANE_J) {
        return NS_PB_COLS + h.col;
    }
    /* Painted rail gap is visual. Each + or - lane is one bus. */
    if (h.lane == NS_PB_LANE_TOP_POS) {
        return NS_PB_COLS * 2 + 0;
    }
    if (h.lane == NS_PB_LANE_TOP_NEG) {
        return NS_PB_COLS * 2 + 1;
    }
    if (h.lane == NS_PB_LANE_BOT_POS) {
        return NS_PB_COLS * 2 + 2;
    }
    if (h.lane == NS_PB_LANE_BOT_NEG) {
        return NS_PB_COLS * 2 + 3;
    }
    return 0;
}

void ns_breadboard_hole_world(const NsBreadboard *bb, NsPbHole h, int *wx, int *wy) {
    int lx = NS_PB_MARGIN + h.col * NS_PB_PITCH + NS_PB_PITCH / 2;
    int ly = lane_hole_y(h.lane) + NS_PB_PITCH / 2;
    int bx = bb ? bb->base.board_x : 0;
    int by = bb ? bb->base.board_y : 0;
    int long_px = board_long_px();
    int short_px = board_short_px();
    if (!wx || !wy) {
        return;
    }
    if (!bb) {
        *wx = lx;
        *wy = ly;
        return;
    }
    switch (bb->base.orient) {
    case NS_ORIENT_90:
        *wx = bx + (short_px - NS_PB_HOLE - ly);
        *wy = by + lx;
        break;
    case NS_ORIENT_180:
        *wx = bx + (long_px - NS_PB_HOLE - lx);
        *wy = by + (short_px - NS_PB_HOLE - ly);
        break;
    case NS_ORIENT_270:
        *wx = bx + ly;
        *wy = by + (long_px - NS_PB_HOLE - lx);
        break;
    case NS_ORIENT_0:
    default:
        *wx = bx + lx;
        *wy = by + ly;
        break;
    }
}

int ns_breadboard_hit_hole(const NsBreadboard *bb, int wx, int wy, NsPbHole *out) {
    int col;
    int lane;
    int best_d2 = (NS_PB_PITCH * NS_PB_PITCH) / 2 + 1;
    int found = 0;
    NsPbHole best = {0, 0};
    int bw;
    int bh;
    int lx;
    int ly;
    int col0;
    int lane0;
    int c;
    int l;

    if (!bb) {
        return 0;
    }
    bw = bb->base.body_w;
    bh = bb->base.body_h;
    if (wx < bb->base.board_x - NS_PB_PITCH || wy < bb->base.board_y - NS_PB_PITCH ||
        wx >= bb->base.board_x + bw + NS_PB_PITCH || wy >= bb->base.board_y + bh + NS_PB_PITCH) {
        return 0;
    }

    /* Inverse of hole_world for the common orientations — O(1) candidate window. */
    {
        int bx = bb->base.board_x;
        int by = bb->base.board_y;
        int long_px = board_long_px();
        int short_px = board_short_px();
        switch (bb->base.orient) {
        case NS_ORIENT_90:
            ly = short_px - NS_PB_HOLE - (wx - bx);
            lx = wy - by;
            break;
        case NS_ORIENT_180:
            lx = long_px - NS_PB_HOLE - (wx - bx);
            ly = short_px - NS_PB_HOLE - (wy - by);
            break;
        case NS_ORIENT_270:
            ly = wx - bx;
            lx = long_px - NS_PB_HOLE - (wy - by);
            break;
        case NS_ORIENT_0:
        default:
            lx = wx - bx;
            ly = wy - by;
            break;
        }
    }
    col0 = (lx - NS_PB_MARGIN) / NS_PB_PITCH;
    if (col0 < 0) {
        col0 = 0;
    }
    if (col0 >= NS_PB_COLS) {
        col0 = NS_PB_COLS - 1;
    }
    /* Nearest lanes by local y (rail gaps make this approximate — check ±2). */
    lane0 = 0;
    {
        int best_ady = 0x7fffffff;
        for (l = 0; l < NS_PB_LANE_COUNT; l++) {
            int cy = lane_hole_y(l) + NS_PB_PITCH / 2;
            int ady = cy - ly;
            if (ady < 0) {
                ady = -ady;
            }
            if (ady < best_ady) {
                best_ady = ady;
                lane0 = l;
            }
        }
    }
    for (c = col0 - 2; c <= col0 + 2; c++) {
        if (c < 0 || c >= NS_PB_COLS) {
            continue;
        }
        for (l = lane0 - 2; l <= lane0 + 2; l++) {
            NsPbHole h;
            int hx, hy, dx, dy, d2;
            if (l < 0 || l >= NS_PB_LANE_COUNT) {
                continue;
            }
            h.col = c;
            h.lane = l;
            if (!ns_breadboard_hole_exists(h)) {
                continue;
            }
            ns_breadboard_hole_world(bb, h, &hx, &hy);
            dx = hx - wx;
            dy = hy - wy;
            d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = h;
                found = 1;
            }
        }
    }
    if (found && out) {
        *out = best;
    }
    return found;
}

void ns_breadboard_set_hover(NsBreadboard *bb, int valid, NsPbHole h) {
    if (!bb) {
        return;
    }
    bb->hover_valid = valid ? 1 : 0;
    bb->hover = h;
}

void ns_breadboard_clear_hover(NsBreadboard *bb) {
    if (bb) {
        bb->hover_valid = 0;
    }
}

int ns_breadboard_tip_strip(const NsBreadboard *bb, int wx, int wy, int *strip_out) {
    NsPbHole h;
    int hx, hy;
    if (!bb || !ns_breadboard_hit_hole(bb, wx, wy, &h)) {
        return 0;
    }
    ns_breadboard_hole_world(bb, h, &hx, &hy);
    if (hx != wx || hy != wy) {
        return 0;
    }
    if (strip_out) {
        *strip_out = ns_breadboard_strip_id(h);
    }
    return 1;
}

static void fill_lane_band(SDL_Renderer *ren, const NsBreadboard *bb, int screen_x, int screen_y, int lane0,
                           int lane1, Uint8 R, Uint8 G, Uint8 B) {
    NsPbHole h0 = {0, lane0};
    NsPbHole h1 = {NS_PB_COLS - 1, lane1};
    int x0, y0, x1, y1;
    SDL_Rect band;
    int bx = bb->base.board_x;
    int by = bb->base.board_y;
    ns_breadboard_hole_world(bb, h0, &x0, &y0);
    ns_breadboard_hole_world(bb, h1, &x1, &y1);
    x0 = screen_x + (x0 - bx);
    y0 = screen_y + (y0 - by);
    x1 = screen_x + (x1 - bx);
    y1 = screen_y + (y1 - by);
    if (ns_orient_is_horiz(bb->base.orient)) {
        band.x = ((x0 < x1) ? x0 : x1) - NS_PB_PITCH / 2;
        band.y = ((y0 < y1) ? y0 : y1) - NS_PB_PITCH / 2;
        band.w = ((x0 > x1) ? x0 - x1 : x1 - x0) + NS_PB_PITCH;
        band.h = ((y0 > y1) ? y0 - y1 : y1 - y0) + NS_PB_PITCH;
    } else {
        band.x = ((x0 < x1) ? x0 : x1) - NS_PB_PITCH / 2;
        band.y = ((y0 < y1) ? y0 : y1) - NS_PB_PITCH / 2;
        band.w = ((x0 > x1) ? x0 - x1 : x1 - x0) + NS_PB_PITCH;
        band.h = ((y0 > y1) ? y0 - y1 : y1 - y0) + NS_PB_PITCH;
    }
    SDL_SetRenderDrawColor(ren, R, G, B, 255);
    SDL_RenderFillRect(ren, &band);
}

static void draw_hole(SDL_Renderer *ren, int x, int y, int lane, int pos_pwr, int neg_pwr, int highlight) {
    int is_pos = (lane == NS_PB_LANE_TOP_POS || lane == NS_PB_LANE_BOT_POS);
    int is_neg = (lane == NS_PB_LANE_TOP_NEG || lane == NS_PB_LANE_BOT_NEG);
    if (NS_PB_HOLE_INSET > 0 || NS_PB_HOLE > 1) {
        SDL_Rect cell = {x - NS_PB_PITCH / 2, y - NS_PB_PITCH / 2, NS_PB_PITCH, NS_PB_PITCH};
        SDL_Rect hole = {x - NS_PB_HOLE / 2, y - NS_PB_HOLE / 2, NS_PB_HOLE, NS_PB_HOLE};
        if (highlight) {
            SDL_SetRenderDrawColor(ren, 80, 200, 255, 255);
            SDL_RenderFillRect(ren, &cell);
        }
        if (is_pos && pos_pwr) {
            SDL_SetRenderDrawColor(ren, 200, 45, 45, 255);
            SDL_RenderFillRect(ren, &hole);
        } else if (is_neg && neg_pwr) {
            SDL_SetRenderDrawColor(ren, 45, 70, 200, 255);
            SDL_RenderFillRect(ren, &hole);
        } else {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderFillRect(ren, &hole);
        }
        return;
    }
    (void)pos_pwr;
    (void)neg_pwr;
    (void)lane;
    if (highlight) {
        SDL_SetRenderDrawColor(ren, 80, 200, 255, 255);
    } else {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    }
    SDL_RenderDrawPoint(ren, x, y);
}

void ns_breadboard_draw(SDL_Renderer *r, const NsBreadboard *bb, int screen_x, int screen_y, int selected) {
    ns_breadboard_draw_power(r, bb, screen_x, screen_y, selected, 0, 0);
}

void ns_breadboard_draw_power(SDL_Renderer *r, const NsBreadboard *bb, int screen_x, int screen_y, int selected,
                              int pos_rail_power, int neg_rail_power) {
    int col;
    int lane;
    int bw;
    int bh;
    SDL_Rect body;
    int hover_strip = -1;
    int bx;
    int by;

    if (!r || !bb) {
        return;
    }
    bw = bb->base.body_w;
    bh = bb->base.body_h;
    bx = bb->base.board_x;
    by = bb->base.board_y;
    body.x = screen_x;
    body.y = screen_y;
    body.w = bw;
    body.h = bh;

    SDL_SetRenderDrawColor(r, 232, 228, 214, 255);
    SDL_RenderFillRect(r, &body);

    fill_lane_band(r, bb, screen_x, screen_y, NS_PB_LANE_TOP_POS, NS_PB_LANE_TOP_POS, 242, 190, 190);
    fill_lane_band(r, bb, screen_x, screen_y, NS_PB_LANE_TOP_NEG, NS_PB_LANE_TOP_NEG, 185, 195, 235);
    fill_lane_band(r, bb, screen_x, screen_y, NS_PB_LANE_BOT_POS, NS_PB_LANE_BOT_POS, 242, 190, 190);
    fill_lane_band(r, bb, screen_x, screen_y, NS_PB_LANE_BOT_NEG, NS_PB_LANE_BOT_NEG, 185, 195, 235);

    /* Trench between E and F. */
    {
        NsPbHole he = {0, NS_PB_LANE_E};
        NsPbHole hf = {0, NS_PB_LANE_F};
        int xe, ye, xf, yf;
        SDL_Rect trench;
        ns_breadboard_hole_world(bb, he, &xe, &ye);
        ns_breadboard_hole_world(bb, hf, &xf, &yf);
        xe = screen_x + (xe - bx);
        ye = screen_y + (ye - by);
        xf = screen_x + (xf - bx);
        yf = screen_y + (yf - by);
        if (ns_orient_is_horiz(bb->base.orient)) {
            trench.x = screen_x + NS_PB_MARGIN / 2;
            trench.y = ye + NS_PB_HOLE;
            trench.w = bw - NS_PB_MARGIN;
            trench.h = (yf - ye) - NS_PB_HOLE;
            if (trench.h < 1) {
                trench.h = 1;
            }
        } else {
            trench.x = xf + NS_PB_HOLE;
            trench.y = screen_y + NS_PB_MARGIN / 2;
            trench.w = (xe - xf) - NS_PB_HOLE;
            trench.h = bh - NS_PB_MARGIN;
            if (trench.w < 1) {
                trench.w = 1;
            }
        }
        SDL_SetRenderDrawColor(r, 200, 195, 180, 255);
        SDL_RenderFillRect(r, &trench);
    }

    SDL_SetRenderDrawColor(r, 180, 170, 150, 255);
    SDL_RenderDrawRect(r, &body);
    if (selected) {
        SDL_SetRenderDrawColor(r, 40, 180, 80, 255);
        SDL_RenderDrawRect(r, &body);
        body.x--;
        body.y--;
        body.w += 2;
        body.h += 2;
        SDL_RenderDrawRect(r, &body);
    }

    if (bb->hover_valid) {
        hover_strip = ns_breadboard_strip_id(bb->hover);
    }

    for (col = 0; col < NS_PB_COLS; col++) {
        for (lane = 0; lane < NS_PB_LANE_COUNT; lane++) {
            NsPbHole h = {col, lane};
            int hx, hy;
            int hl = 0;
            if (!ns_breadboard_hole_exists(h)) {
                continue;
            }
            ns_breadboard_hole_world(bb, h, &hx, &hy);
            hx = screen_x + (hx - bx);
            hy = screen_y + (hy - by);
            if (hover_strip >= 0 && ns_breadboard_strip_id(h) == hover_strip) {
                hl = 1;
            }
            draw_hole(r, hx, hy, lane, pos_rail_power, neg_rail_power, hl);
        }
    }
}
