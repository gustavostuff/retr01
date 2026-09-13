#include "netlist_sim/breadboard.h"

#include <string.h>

/* Strips: 0..COLS-1 = A-E per col, COLS..2*COLS-1 = F-J,
 * then 8 rail halves (4 lanes x left/right of mid-break). */
#define NS_PB_STRIPS (NS_PB_COLS * 2 + 8)

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
    return lane_hole_y(NS_PB_LANE_BOT_NEG) + NS_PB_HOLE + NS_PB_MARGIN;
}

static int board_long_px(void) {
    return NS_PB_MARGIN + (NS_PB_COLS - 1) * NS_PB_PITCH + NS_PB_HOLE + NS_PB_MARGIN;
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
    ns_entity_init(&bb->base, &BB_VT, "BREADBOARD", refdes ? refdes : "BB1");
    bb->base.impl = bb;
    ns_breadboard_body_size(NS_ORIENT_H, &w, &h);
    ns_entity_set_glyph(&bb->base, NS_ENTITY_VIS_BREADBOARD, w, h);
    bb->base.orient = NS_ORIENT_H;
    ns_breadboard_sync_body(bb);
}

NsEntity *ns_breadboard_entity(NsBreadboard *bb) {
    return bb ? &bb->base : NULL;
}

int ns_breadboard_hole_exists(NsPbHole h) {
    if (h.col < 0 || h.col >= NS_PB_COLS) {
        return 0;
    }
    if (h.lane == NS_PB_LANE_TOP_POS || h.lane == NS_PB_LANE_TOP_NEG ||
        h.lane == NS_PB_LANE_BOT_POS || h.lane == NS_PB_LANE_BOT_NEG) {
        int local;
        if (h.col < NS_PB_RAIL_SEG) {
            local = h.col;
        } else if (h.col >= NS_PB_RAIL_GAP_END) {
            local = h.col - NS_PB_RAIL_GAP_END;
        } else {
            return 0;
        }
        return (local % (NS_PB_RAIL_GROUP + 1)) != NS_PB_RAIL_GROUP;
    }
    if (h.lane < 0 || h.lane >= NS_PB_LANE_COUNT) {
        return 0;
    }
    return 1;
}

int ns_breadboard_strip_id(NsPbHole h) {
    int rail_half;
    if (h.lane >= NS_PB_LANE_A && h.lane <= NS_PB_LANE_E) {
        return h.col;
    }
    if (h.lane >= NS_PB_LANE_F && h.lane <= NS_PB_LANE_J) {
        return NS_PB_COLS + h.col;
    }
    /* Power rails: mid-break splits each lane into two independent buses. */
    rail_half = (h.col >= NS_PB_RAIL_GAP_END) ? 1 : 0;
    if (h.lane == NS_PB_LANE_TOP_POS) {
        return NS_PB_COLS * 2 + 0 + rail_half * 4;
    }
    if (h.lane == NS_PB_LANE_TOP_NEG) {
        return NS_PB_COLS * 2 + 1 + rail_half * 4;
    }
    if (h.lane == NS_PB_LANE_BOT_POS) {
        return NS_PB_COLS * 2 + 2 + rail_half * 4;
    }
    if (h.lane == NS_PB_LANE_BOT_NEG) {
        return NS_PB_COLS * 2 + 3 + rail_half * 4;
    }
    return 0;
}

void ns_breadboard_hole_world(const NsBreadboard *bb, NsPbHole h, int *wx, int *wy) {
    int lx = NS_PB_MARGIN + h.col * NS_PB_PITCH;
    int ly = lane_hole_y(h.lane);
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

    if (!bb) {
        return 0;
    }
    bw = bb->base.body_w;
    bh = bb->base.body_h;
    if (wx < bb->base.board_x - NS_PB_PITCH || wy < bb->base.board_y - NS_PB_PITCH ||
        wx >= bb->base.board_x + bw + NS_PB_PITCH || wy >= bb->base.board_y + bh + NS_PB_PITCH) {
        return 0;
    }
    for (col = 0; col < NS_PB_COLS; col++) {
        for (lane = 0; lane < NS_PB_LANE_COUNT; lane++) {
            NsPbHole h = {col, lane};
            int hx, hy, dx, dy, d2;
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

static void draw_hole(SDL_Renderer *ren, int x, int y, int highlight) {
    if (highlight) {
        SDL_SetRenderDrawColor(ren, 80, 200, 255, 255);
    } else {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    }
    SDL_RenderDrawPoint(ren, x, y);
}

void ns_breadboard_draw(SDL_Renderer *r, const NsBreadboard *bb, int screen_x, int screen_y, int selected) {
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
            draw_hole(r, hx, hy, hl);
        }
    }

    /* + / - marks on top rails. */
    {
        NsPbHole hp = {0, NS_PB_LANE_TOP_POS};
        NsPbHole hn = {0, NS_PB_LANE_TOP_NEG};
        int px, py, nx, ny;
        ns_breadboard_hole_world(bb, hp, &px, &py);
        ns_breadboard_hole_world(bb, hn, &nx, &ny);
        px = screen_x + (px - bx);
        py = screen_y + (py - by);
        nx = screen_x + (nx - bx);
        ny = screen_y + (ny - by);
        SDL_SetRenderDrawColor(r, 200, 50, 50, 255);
        SDL_RenderDrawLine(r, px - 2, py, px + 2, py);
        SDL_RenderDrawLine(r, px, py - 2, px, py + 2);
        SDL_SetRenderDrawColor(r, 50, 70, 180, 255);
        SDL_RenderDrawLine(r, nx - 2, ny, nx + 2, ny);
    }
}
