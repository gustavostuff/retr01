#include "discrete_ic/pin_header.h"

#include "discrete_ic/pin.h"

#include <string.h>

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderFillRect(r, &rc);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawRect(r, &rc);
}

static void pin_pixel(SDL_Renderer *r, int x, int y, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawPoint(r, x, y);
}

void ns_pin_header_level_rgb(NsLevel lvl, NsPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
    if (dir == NS_PIN_PWR) {
        *pr = 220;
        *pg = 70;
        *pb = 70;
        return;
    }
    if (dir == NS_PIN_NC) {
        *pr = 70;
        *pg = 70;
        *pb = 70;
        return;
    }
    switch (lvl) {
    case NS_LVL_H:
        *pr = 70;
        *pg = 210;
        *pb = 90;
        break;
    case NS_LVL_L:
        *pr = 28;
        *pg = 32;
        *pb = 30;
        break;
    case NS_LVL_X:
        *pr = 220;
        *pg = 80;
        *pb = 200;
        break;
    default:
        *pr = 120;
        *pg = 125;
        *pb = 110;
        break;
    }
}

void ns_pin_header_grid(const NsEntity *e, int *cols, int *rows) {
    int c = 1;
    int rw = 1;
    if (e && e->visual == NS_ENTITY_VIS_PIN_HDR) {
        if (e->pkg_len_mm > 0) {
            c = e->pkg_len_mm;
        }
        if (e->pkg_wid_mm > 0) {
            rw = e->pkg_wid_mm;
        }
    }
    if (cols) {
        *cols = c;
    }
    if (rows) {
        *rows = rw;
    }
}

void ns_pin_header_refresh_body(NsEntity *e) {
    int cols;
    int rows;
    int w;
    int h;
    if (!e || e->visual != NS_ENTITY_VIS_PIN_HDR) {
        return;
    }
    ns_pin_header_grid(e, &cols, &rows);
    w = cols * NS_PIN_HDR_CELL_PX;
    h = rows * NS_PIN_HDR_CELL_PX;
    if (!ns_orient_is_horiz(e->orient)) {
        e->body_w = h;
        e->body_h = w;
    } else {
        e->body_w = w;
        e->body_h = h;
    }
}

void ns_entity_set_pin_header(NsEntity *e, int cols, int rows) {
    if (!e) {
        return;
    }
    if (cols < 1) {
        cols = 1;
    }
    if (rows < 1) {
        rows = 1;
    }
    e->visual = NS_ENTITY_VIS_PIN_HDR;
    e->dip_pins = 0;
    e->pkg_len_mm = cols;
    e->pkg_wid_mm = rows;
    e->orient = NS_ORIENT_0;
    ns_pin_header_refresh_body(e);
}

static int pin_grid_index(const NsEntity *e, int pin_num, int *col, int *row) {
    int cols;
    int rows;
    int i;
    if (!e || pin_num < 1) {
        return 0;
    }
    ns_pin_header_grid(e, &cols, &rows);
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].number == pin_num) {
            int idx = i;
            if (col) {
                *col = idx % cols;
            }
            if (row) {
                *row = idx / cols;
            }
            return 1;
        }
    }
    return 0;
}

static void cell_origin(const NsEntity *e, int col, int row, int *ox, int *oy) {
    int cols;
    int rows;
    int gx;
    int gy;
    ns_pin_header_grid(e, &cols, &rows);
    gx = col * NS_PIN_HDR_CELL_PX;
    gy = row * NS_PIN_HDR_CELL_PX;
    switch (e->orient) {
    case NS_ORIENT_90:
        if (ox) {
            *ox = (rows - 1 - row) * NS_PIN_HDR_CELL_PX;
        }
        if (oy) {
            *oy = col * NS_PIN_HDR_CELL_PX;
        }
        break;
    case NS_ORIENT_180:
        if (ox) {
            *ox = (cols - 1 - col) * NS_PIN_HDR_CELL_PX;
        }
        if (oy) {
            *oy = (rows - 1 - row) * NS_PIN_HDR_CELL_PX;
        }
        break;
    case NS_ORIENT_270:
        if (ox) {
            *ox = row * NS_PIN_HDR_CELL_PX;
        }
        if (oy) {
            *oy = (cols - 1 - col) * NS_PIN_HDR_CELL_PX;
        }
        break;
    case NS_ORIENT_0:
    default:
        if (ox) {
            *ox = gx;
        }
        if (oy) {
            *oy = gy;
        }
        break;
    }
}

int ns_pin_header_pin_tip_board(const NsEntity *e, int pin_num, int *tbx, int *tby) {
    int col;
    int row;
    int ox;
    int oy;
    int reach = 2;
    if (!e || !tbx || !tby || !pin_grid_index(e, pin_num, &col, &row)) {
        return 0;
    }
    cell_origin(e, col, row, &ox, &oy);
    switch (e->orient) {
    case NS_ORIENT_90:
        *tbx = e->board_x + ox + NS_PIN_HDR_CELL_PX / 2;
        *tby = e->board_y + oy + NS_PIN_HDR_CELL_PX + reach;
        break;
    case NS_ORIENT_180:
        *tbx = e->board_x + ox + NS_PIN_HDR_CELL_PX / 2;
        *tby = e->board_y + oy - 1 - reach;
        break;
    case NS_ORIENT_270:
        *tbx = e->board_x + ox - 1 - reach;
        *tby = e->board_y + oy + NS_PIN_HDR_CELL_PX / 2;
        break;
    case NS_ORIENT_0:
    default:
        *tbx = e->board_x + ox + NS_PIN_HDR_CELL_PX / 2;
        *tby = e->board_y + oy + NS_PIN_HDR_CELL_PX + reach;
        break;
    }
    return 1;
}

int ns_pin_header_hit(const NsEntity *e, int bx, int by) {
    if (!e || e->visual != NS_ENTITY_VIS_PIN_HDR) {
        return 0;
    }
    return bx >= e->board_x && by >= e->board_y && bx < e->board_x + e->body_w &&
           by < e->board_y + e->body_h;
}

void ns_pin_header_draw(SDL_Renderer *r, const NsEntity *e, int sx, int sy, int selected) {
    int cols;
    int rows;
    int i;
    if (!r || !e || e->visual != NS_ENTITY_VIS_PIN_HDR) {
        return;
    }
    ns_pin_header_grid(e, &cols, &rows);
    fill_rect(r, sx, sy, e->body_w, e->body_h, 0, 0, 0);
    for (i = 0; i < cols * rows && i < e->pin_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int ox;
        int oy;
        Uint8 cr;
        Uint8 cg;
        Uint8 cb;
        const NsPin *p = &e->pins[i];
        cell_origin(e, col, row, &ox, &oy);
        ns_pin_header_level_rgb(p->level, p->dir, &cr, &cg, &cb);
        pin_pixel(r, sx + ox + NS_PIN_HDR_CELL_PX / 2, sy + oy + NS_PIN_HDR_CELL_PX / 2, cr, cg, cb);
    }
    if (selected) {
        draw_rect(r, sx, sy, e->body_w, e->body_h, 255, 220, 80);
    } else {
        draw_rect(r, sx, sy, e->body_w, e->body_h, 48, 52, 56);
    }
}
