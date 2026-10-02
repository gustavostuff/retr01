#include "ui.h"
#include "ui_font.h"

#include "r01a_board.h"
#include "r01a_layout.h"
#include "r01a_netlist.h"
#include "r01a_rgb_netlist.h"

#include "discrete_ic/entity.h"
#include "discrete_ic/island.h"
#include "discrete_ic/island_group.h"
#include "discrete_ic/passive.h"
#include "discrete_ic/pin_header.h"
#include "discrete_ic/types.h"
#include "discrete_ic/video_sink.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef R01A_LAYOUT_FILE
#define R01A_LAYOUT_FILE "ui_layout.json"
#endif

#define R01A_UI_SCALE 2
#define R01A_SIM_BUDGET_MS 8
#define R01A_SIM_MAX_STEPS_PER_FRAME 24
#define R01A_DOTS_PER_STEP 32
#define R01A_BOARD_MAX_CHIPS 64
#define R01A_PIN_NET_SLOTS 512
#define R01A_TRACE_MAX 256
#define R01A_TRACE_PTS 32
#define R01A_PAD 3
#define R01A_PAD_GAP 3
#define R01A_PAD_PITCH (R01A_PAD + R01A_PAD_GAP)
#define R01A_ZOOM_MAX 8
#define R01A_SEL_R 255
#define R01A_SEL_G 220
#define R01A_SEL_B 80

typedef struct R01aTrace {
    int n;
    int16_t x[R01A_TRACE_PTS];
    int16_t y[R01A_TRACE_PTS];
} R01aTrace;

typedef struct R01aPinNetSlot {
    NsEntity *entity;
    int pin_index;
} R01aPinNetSlot;

typedef struct R01aUi {
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *target;
    SDL_Texture *lcd_tex;
    int scale;
    int zoom;
    int pan_x;
    int pan_y;
    int mouse_lx;
    int mouse_ly;
    int drag_pan;
    int drag_chip;
    int drag_grab_bx;
    int drag_grab_by;
    int selected;
    int hover_chip;
    int hover_pin;
    int chip_count;
    NsEntity *chips[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_z[R01A_BOARD_MAX_CHIPS];
    int trace_n;
    R01aTrace traces[R01A_TRACE_MAX];
    int arm;
    int arm_x[R01A_TRACE_PTS];
    int arm_y[R01A_TRACE_PTS];
    int arm_n;
    int dest_chip;
    int dest_pin;
    int show_nets; /* 1 = Manual (airs + copper), 0 = Auto (hidden) */
} R01aUi;

static R01aPinNetSlot g_pin_slots[R01A_PIN_NET_SLOTS];
static int g_pin_parent[R01A_PIN_NET_SLOTS];
static int g_cu_parent[R01A_PIN_NET_SLOTS];
static int g_pin_slot_count;

static int div_floor(int a, int b) {
    if (b <= 0) {
        return a;
    }
    if (a >= 0) {
        return a / b;
    }
    return -((-a + b - 1) / b);
}

static int canvas_zoom(const R01aUi *ui) {
    int z = (ui && ui->zoom > 0) ? ui->zoom : 1;
    return z > R01A_ZOOM_MAX ? R01A_ZOOM_MAX : z;
}

static int board_sx(const R01aUi *ui, int bx) {
    return bx - ui->pan_x;
}

static int board_sy(const R01aUi *ui, int by) {
    return by - ui->pan_y;
}

static void logic_to_board(const R01aUi *ui, int lx, int ly, int *bx, int *by) {
    int z = canvas_zoom(ui);
    *bx = div_floor(lx, z) + ui->pan_x;
    *by = div_floor(ly, z) + ui->pan_y;
}

static void canvas_zoom_by(R01aUi *ui, int delta, int lx, int ly) {
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
    ui->pan_x = bx - div_floor(lx, z1);
    ui->pan_y = by - div_floor(ly, z1);
}

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

static void plot_a(SDL_Renderer *r, int x, int y, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderDrawPoint(r, x, y);
}

static int ipart(float x) {
    return (int)x;
}

static int iround(float x) {
    return (int)(x + 0.5f);
}

static float fpart(float x) {
    return x - (float)ipart(x);
}

static float rfpart(float x) {
    return 1.0f - fpart(x);
}

/* Xiaolin Wu — soft air-wire stroke. */
static void draw_soft_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb,
                           Uint8 ca) {
    int steep = (y1 > y0 ? y1 - y0 : y0 - y1) > (x1 > x0 ? x1 - x0 : x0 - x1);
    float dx;
    float dy;
    float grad;
    float xend;
    float yend;
    float xgap;
    float intery;
    int xpxl1;
    int ypxl1;
    int xpxl2;
    int ypxl2;
    int x;
    float fx0 = (float)x0;
    float fy0 = (float)y0;
    float fx1 = (float)x1;
    float fy1 = (float)y1;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (steep) {
        float t = fx0;
        fx0 = fy0;
        fy0 = t;
        t = fx1;
        fx1 = fy1;
        fy1 = t;
    }
    if (fx0 > fx1) {
        float t = fx0;
        fx0 = fx1;
        fx1 = t;
        t = fy0;
        fy0 = fy1;
        fy1 = t;
    }
    dx = fx1 - fx0;
    dy = fy1 - fy0;
    grad = dx == 0.0f ? 1.0f : dy / dx;

    xend = (float)iround(fx0);
    yend = fy0 + grad * (xend - fx0);
    xgap = rfpart(fx0 + 0.5f);
    xpxl1 = (int)xend;
    ypxl1 = ipart(yend);
    if (steep) {
        plot_a(r, ypxl1, xpxl1, cr, cg, cb, (Uint8)((float)ca * rfpart(yend) * xgap));
        plot_a(r, ypxl1 + 1, xpxl1, cr, cg, cb, (Uint8)((float)ca * fpart(yend) * xgap));
    } else {
        plot_a(r, xpxl1, ypxl1, cr, cg, cb, (Uint8)((float)ca * rfpart(yend) * xgap));
        plot_a(r, xpxl1, ypxl1 + 1, cr, cg, cb, (Uint8)((float)ca * fpart(yend) * xgap));
    }
    intery = yend + grad;

    xend = (float)iround(fx1);
    yend = fy1 + grad * (xend - fx1);
    xgap = fpart(fx1 + 0.5f);
    xpxl2 = (int)xend;
    ypxl2 = ipart(yend);
    if (steep) {
        plot_a(r, ypxl2, xpxl2, cr, cg, cb, (Uint8)((float)ca * rfpart(yend) * xgap));
        plot_a(r, ypxl2 + 1, xpxl2, cr, cg, cb, (Uint8)((float)ca * fpart(yend) * xgap));
    } else {
        plot_a(r, xpxl2, ypxl2, cr, cg, cb, (Uint8)((float)ca * rfpart(yend) * xgap));
        plot_a(r, xpxl2, ypxl2 + 1, cr, cg, cb, (Uint8)((float)ca * fpart(yend) * xgap));
    }

    for (x = xpxl1 + 1; x <= xpxl2 - 1; x++) {
        if (steep) {
            plot_a(r, ipart(intery), x, cr, cg, cb, (Uint8)((float)ca * rfpart(intery)));
            plot_a(r, ipart(intery) + 1, x, cr, cg, cb, (Uint8)((float)ca * fpart(intery)));
        } else {
            plot_a(r, x, ipart(intery), cr, cg, cb, (Uint8)((float)ca * rfpart(intery)));
            plot_a(r, x, ipart(intery) + 1, cr, cg, cb, (Uint8)((float)ca * fpart(intery)));
        }
        intery += grad;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_hard_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawLine(r, x0, y0, x1, y1);
}

static void draw_ants_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint32 now) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int n = adx > ady ? adx : ady;
    int i;
    int phase = (int)((now / 80u) % 8u);
    if (n < 1) {
        return;
    }
    for (i = 0; i <= n; i++) {
        int x = x0 + dx * i / n;
        int y = y0 + dy * i / n;
        if (((i + phase) & 7) < 4) {
            SDL_SetRenderDrawColor(r, 230, 230, 80, 255);
            SDL_RenderDrawPoint(r, x, y);
        }
    }
}

static void snap_octant(int x0, int y0, int x1, int y1, int *ox, int *oy) {
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

static void bind_chips(R01aUi *ui, R01aBoard *board) {
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

static void move_entity(NsEntity *e, int bx, int by) {
    if (!e) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        NsPassive *p = (NsPassive *)e;
        ns_passive_set_pivot(p, p->pivot_x + (bx - e->board_x), p->pivot_y + (by - e->board_y));
        return;
    }
    ns_entity_place(e, bx, by);
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
    return 2 + (half > 1 ? (half - 1) * R01A_PAD_PITCH : 0) + R01A_PAD;
}

static int ic_across(const NsEntity *e) {
    return (e && e->pkg_wid_mm >= 13) ? 22 : 12;
}

static int ic_body_w(const NsEntity *e) {
    if (!e) {
        return 1;
    }
    return ns_orient_is_horiz(e->orient) ? ic_along(e) : ic_across(e);
}

static int ic_body_h(const NsEntity *e) {
    if (!e) {
        return 1;
    }
    return ns_orient_is_horiz(e->orient) ? ic_across(e) : ic_along(e);
}

static int is_passive_glyph(const NsEntity *e) {
    return e && (e->visual == NS_ENTITY_VIS_PASSIVE || e->visual == NS_ENTITY_VIS_OSC ||
                 e->visual == NS_ENTITY_VIS_PIN_HDR);
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

static int glyph_w(const NsEntity *e) {
    int n;
    if (e && e->visual == NS_ENTITY_VIS_PIN_HDR) {
        int cols;
        int rows;
        header_grid(e, &cols, &rows);
        n = cols;
    } else {
        n = (e && e->pin_count > 0) ? e->pin_count : 1;
    }
    return 2 + (n - 1) * R01A_PAD_PITCH + R01A_PAD;
}

static int glyph_h(const NsEntity *e) {
    if (e && e->visual == NS_ENTITY_VIS_PIN_HDR) {
        int cols;
        int rows;
        header_grid(e, &cols, &rows);
        return 2 + (rows - 1) * R01A_PAD_PITCH + R01A_PAD;
    }
    return 9;
}

static void passive_kind_rgb(NsPassiveKind kind, Uint8 *cr, Uint8 *cg, Uint8 *cb) {
    switch (kind) {
    case NS_PASSIVE_R:
        *cr = 220;
        *cg = 140;
        *cb = 50;
        break;
    case NS_PASSIVE_CCAP:
        *cr = 40;
        *cg = 170;
        *cb = 155;
        break;
    case NS_PASSIVE_ECAP:
        *cr = 150;
        *cg = 70;
        *cb = 200;
        break;
    case NS_PASSIVE_D:
        *cr = 210;
        *cg = 70;
        *cb = 110;
        break;
    case NS_PASSIVE_OSC:
    case NS_PASSIVE_OSC4LEGS:
    case NS_PASSIVE_XTAL:
        *cr = 190;
        *cg = 200;
        *cb = 55;
        break;
    default:
        *cr = 70;
        *cg = 100;
        *cb = 155;
        break;
    }
}

static void entity_fill_rgb(const NsEntity *e, Uint8 *cr, Uint8 *cg, Uint8 *cb) {
    if (e && e->visual == NS_ENTITY_VIS_PASSIVE) {
        passive_kind_rgb(((const NsPassive *)e)->kind, cr, cg, cb);
        return;
    }
    if (e && e->visual == NS_ENTITY_VIS_OSC) {
        passive_kind_rgb(NS_PASSIVE_OSC4LEGS, cr, cg, cb);
        return;
    }
    *cr = 70;
    *cg = 100;
    *cb = 155;
}

static int pin_center(const NsEntity *e, int pin_index, int *cx, int *cy) {
    int num;
    int dip;
    int half;
    int side_pin1;
    int idx;
    int span;
    int row;
    int margin;
    int reverse;
    int along;
    int horiz;
    if (!e || pin_index < 0 || pin_index >= e->pin_count || !cx || !cy) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_PIN_HDR) {
        int cols;
        int rows;
        int col;
        int row;
        int w = glyph_w(e);
        int h = glyph_h(e);
        int along_x;
        int along_y;
        int mx;
        int my;
        header_grid(e, &cols, &rows);
        col = pin_index % cols;
        row = pin_index / cols;
        along_x = (cols > 1) ? (cols - 1) * R01A_PAD_PITCH : 0;
        along_y = (rows > 1) ? (rows - 1) * R01A_PAD_PITCH : 0;
        mx = (w - along_x) / 2;
        my = (h - along_y) / 2;
        if (mx < 1) {
            mx = 1;
        }
        if (my < 1) {
            my = 1;
        }
        *cx = e->board_x + mx + col * R01A_PAD_PITCH;
        *cy = e->board_y + my + row * R01A_PAD_PITCH;
        return 1;
    }
    if (is_passive_glyph(e)) {
        int n = e->pin_count;
        int w = glyph_w(e);
        int along_row = (n > 1) ? (n - 1) * R01A_PAD_PITCH : 0;
        int m = (w - along_row) / 2;
        if (m < 1) {
            m = 1;
        }
        *cx = e->board_x + m + pin_index * R01A_PAD_PITCH;
        *cy = e->board_y + glyph_h(e) + 1;
        return 1;
    }
    num = e->pins[pin_index].number;
    dip = dip_count(e);
    if (num < 1 || num > dip || dip < 2) {
        return ns_entity_pin_tip_board(e, num, cx, cy);
    }
    half = dip / 2;
    side_pin1 = num <= half;
    idx = side_pin1 ? (num - 1) : (dip - num);
    horiz = ns_orient_is_horiz(e->orient);
    span = horiz ? ic_body_w(e) : ic_body_h(e);
    row = (half > 1) ? (half - 1) * R01A_PAD_PITCH : 0;
    margin = (span - row) / 2;
    if (margin < 1) {
        margin = 1;
    }
    reverse = (e->orient == NS_ORIENT_180 || e->orient == NS_ORIENT_270);
    along = reverse ? (margin + (half > 0 ? (half - 1 - idx) : 0) * R01A_PAD_PITCH)
                    : (margin + idx * R01A_PAD_PITCH);
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

static int hit_pin_at(const R01aUi *ui, int bx, int by, int *chip_out, int *pin_out) {
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
    if (!e || e->visual == NS_ENTITY_VIS_DISPLAY) {
        return 0;
    }
    if (is_passive_glyph(e)) {
        return bx >= e->board_x && by >= e->board_y && bx < e->board_x + glyph_w(e) &&
               by < e->board_y + glyph_h(e);
    }
    return bx >= e->board_x && by >= e->board_y && bx < e->board_x + ic_body_w(e) &&
           by < e->board_y + ic_body_h(e);
}

static int hit_top_chip(const R01aUi *ui, int bx, int by) {
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

static NsEntity *ui_ent(const R01aUi *ui, const char *ref) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] && ui->chips[i]->refdes && strcmp(ui->chips[i]->refdes, ref) == 0) {
            return ui->chips[i];
        }
    }
    return NULL;
}

static int pin_net_find(NsEntity *e, int pin_index) {
    int i;
    for (i = 0; i < g_pin_slot_count; i++) {
        if (g_pin_slots[i].entity == e && g_pin_slots[i].pin_index == pin_index) {
            return i;
        }
    }
    return -1;
}

static int pin_net_add(NsEntity *e, int pin_index) {
    int i = pin_net_find(e, pin_index);
    if (i >= 0) {
        return i;
    }
    if (!e || pin_index < 0 || g_pin_slot_count >= R01A_PIN_NET_SLOTS) {
        return -1;
    }
    i = g_pin_slot_count++;
    g_pin_slots[i].entity = e;
    g_pin_slots[i].pin_index = pin_index;
    g_pin_parent[i] = i;
    return i;
}

static int pin_net_add_name(NsEntity *e, const char *name) {
    const NsPin *pin;
    int idx;
    if (!e || !name) {
        return -1;
    }
    pin = ns_entity_pin_named_const(e, name);
    if (!pin) {
        return -1;
    }
    idx = (int)(pin - e->pins);
    if (idx < 0 || idx >= e->pin_count) {
        return -1;
    }
    return pin_net_add(e, idx);
}

static int pin_net_root(int s) {
    while (s >= 0 && s < g_pin_slot_count && g_pin_parent[s] != s) {
        g_pin_parent[s] = g_pin_parent[g_pin_parent[s]];
        s = g_pin_parent[s];
    }
    return s;
}

static void pin_net_union(int a, int b) {
    int ra = pin_net_root(a);
    int rb = pin_net_root(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        g_pin_parent[rb] = ra;
    }
}

static void pin_net_link(NsEntity *a, const char *an, NsEntity *b, const char *bn) {
    int sa = pin_net_add_name(a, an);
    int sb = pin_net_add_name(b, bn);
    if (sa >= 0 && sb >= 0) {
        pin_net_union(sa, sb);
    }
}

static void pin_net_link_fn(NsEntity *a, const char *an, NsEntity *b, const char *bn) {
    pin_net_link(a, an, b, bn);
}

static void pin_net_build(R01aUi *ui) {
    NsEntity *u04 = ui_ent(ui, "U04");
    NsEntity *u74 = ui_ent(ui, "U74");
    NsEntity *y2 = ui_ent(ui, "Y2");
    NsEntity *bx = ui_ent(ui, "UPLDX");
    NsEntity *by = ui_ent(ui, "UPLDY");
    NsEntity *u24 = ui_ent(ui, "U24");
    NsEntity *j2 = ui_ent(ui, "J2");
    NsEntity *r12 = ui_ent(ui, "R12");
    NsEntity *r13 = ui_ent(ui, "R13");
    NsEntity *c6 = ui_ent(ui, "C6");
    NsEntity *c7 = ui_ent(ui, "C7");
    NsEntity *e1 = ui_ent(ui, "E1");
    int i;
    char iname[8];
    char aname[4];

    g_pin_slot_count = 0;
    if (!u04 || !u74 || !y2 || !bx || !by || !u24 || !j2) {
        return;
    }

    pin_net_link(u04, "VCC", u74, "VCC");
    pin_net_link(u04, "VCC", bx, "VCC");
    pin_net_link(u04, "VCC", by, "VCC");
    pin_net_link(u04, "VCC", u24, "VCC");
    pin_net_link(u04, "VCC", u24, "VPP");
    pin_net_link(u04, "VCC", bx, "RES#");
    pin_net_link(u04, "VCC", by, "RES#");
    pin_net_link(u04, "VCC", u24, "PGM#");
    pin_net_link(u74, "VCC", u74, "1PRE#");
    pin_net_link(u74, "VCC", u74, "1CLR#");
    pin_net_link(u74, "VCC", u74, "2PRE#");
    pin_net_link(u74, "VCC", u74, "2CLR#");
    if (e1) {
        pin_net_link(u04, "VCC", e1, "+");
    }
    for (i = 0; i < R01A_BYPASS_N; i++) {
        NsEntity *cap = ui_ent(ui, r01a_bypass_pairs[i].cap);
        NsEntity *ic = ui_ent(ui, r01a_bypass_pairs[i].ic);
        if (cap && ic) {
            pin_net_link(cap, "1", ic, "VCC");
        }
    }

    pin_net_link(u04, "GND", u74, "GND");
    pin_net_link(u04, "GND", bx, "GND");
    pin_net_link(u04, "GND", by, "GND");
    pin_net_link(u04, "GND", u24, "GND");
    pin_net_link(u04, "GND", u24, "CE#");
    pin_net_link(u04, "GND", u24, "OE#");
    pin_net_link(u04, "GND", j2, "GND");
    pin_net_link(u04, "GND", j2, "GND2");
    pin_net_link(u04, "GND", u04, "3A");
    pin_net_link(u04, "GND", u04, "4A");
    pin_net_link(u04, "GND", u04, "5A");
    pin_net_link(u04, "GND", u04, "6A");
    if (e1) {
        pin_net_link(u04, "GND", e1, "-");
    }
    for (i = 0; i < R01A_BYPASS_N; i++) {
        NsEntity *cap = ui_ent(ui, r01a_bypass_pairs[i].cap);
        NsEntity *ic = ui_ent(ui, r01a_bypass_pairs[i].ic);
        if (cap && ic) {
            pin_net_link(cap, "2", ic, "GND");
        }
    }
    if (c6) {
        pin_net_link(u04, "GND", c6, "2");
    }
    if (c7) {
        pin_net_link(u04, "GND", c7, "2");
    }
    for (i = 6; i <= 13; i++) {
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(u04, "GND", u24, aname);
    }

    pin_net_link(u04, "1A", y2, "1");
    if (r13) {
        pin_net_link(u04, "1A", r13, "1");
    }
    if (c6) {
        pin_net_link(u04, "1A", c6, "1");
    }
    pin_net_link(u04, "1Y", y2, "2");
    if (r13) {
        pin_net_link(u04, "1Y", r13, "2");
    }
    if (c7) {
        pin_net_link(u04, "1Y", c7, "1");
    }
    pin_net_link(u04, "1Y", u04, "2A");
    pin_net_link(u04, "2Y", u74, "1CLK");
    pin_net_link(u74, "1/Q", u74, "1D");
    pin_net_link(u74, "1Q", u74, "2CLK");
    pin_net_link(u74, "2/Q", u74, "2D");
    if (r12) {
        pin_net_link(u74, "2Q", r12, "1");
        pin_net_link(r12, "2", bx, "CLK");
    } else {
        pin_net_link(u74, "2Q", bx, "CLK");
    }
    pin_net_link(bx, "HWRAP", by, "CLK");
    pin_net_link(bx, "CSYNC", j2, "CSYNC");
    for (i = 0; i < 6; i++) {
        snprintf(iname, sizeof(iname), "INDEX%d", i);
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(bx, iname, u24, aname);
    }
    r01a_netlist_link_dac_rgbs(pin_net_link_fn, u04, u24, j2, ui_ent(ui, "R1"), ui_ent(ui, "R2"),
                               ui_ent(ui, "R3"), ui_ent(ui, "R4"), ui_ent(ui, "R5"), ui_ent(ui, "R6"),
                               ui_ent(ui, "R7"), ui_ent(ui, "R8"), ui_ent(ui, "R9"), ui_ent(ui, "R10"),
                               ui_ent(ui, "R11"));
}

static int pins_share_net(NsEntity *a, int ap, NsEntity *b, int bp) {
    int sa = pin_net_find(a, ap);
    int sb = pin_net_find(b, bp);
    if (sa < 0 || sb < 0) {
        return 0;
    }
    return pin_net_root(sa) == pin_net_root(sb);
}

static int cu_root(int s) {
    while (s >= 0 && s < g_pin_slot_count && g_cu_parent[s] != s) {
        g_cu_parent[s] = g_cu_parent[g_cu_parent[s]];
        s = g_cu_parent[s];
    }
    return s;
}

static void cu_union(int a, int b) {
    int ra = cu_root(a);
    int rb = cu_root(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        g_cu_parent[rb] = ra;
    }
}

static int slot_at_xy(const R01aUi *ui, int x, int y) {
    int ci = -1;
    int pi = -1;
    if (!hit_pin_at(ui, x, y, &ci, &pi)) {
        return -1;
    }
    return pin_net_find(ui->chips[ci], pi);
}

static int traces_share_point(const R01aTrace *a, const R01aTrace *b) {
    int i;
    int j;
    for (i = 0; i < a->n; i++) {
        for (j = 0; j < b->n; j++) {
            if (a->x[i] == b->x[j] && a->y[i] == b->y[j]) {
                return 1;
            }
        }
    }
    return 0;
}

static int trace_first_slot(const R01aUi *ui, const R01aTrace *t) {
    int i;
    for (i = 0; i < t->n; i++) {
        int s = slot_at_xy(ui, t->x[i], t->y[i]);
        if (s >= 0) {
            return s;
        }
    }
    return -1;
}

static void copper_rebuild(const R01aUi *ui) {
    int i;
    int j;
    for (i = 0; i < g_pin_slot_count; i++) {
        g_cu_parent[i] = i;
    }
    for (i = 0; i < ui->trace_n; i++) {
        int first = -1;
        for (j = 0; j < ui->traces[i].n; j++) {
            int s = slot_at_xy(ui, ui->traces[i].x[j], ui->traces[i].y[j]);
            if (s < 0) {
                continue;
            }
            if (first >= 0) {
                cu_union(first, s);
            } else {
                first = s;
            }
        }
    }
    for (i = 0; i < ui->trace_n; i++) {
        int sa = trace_first_slot(ui, &ui->traces[i]);
        if (sa < 0) {
            continue;
        }
        for (j = i + 1; j < ui->trace_n; j++) {
            int sb;
            if (!traces_share_point(&ui->traces[i], &ui->traces[j])) {
                continue;
            }
            sb = trace_first_slot(ui, &ui->traces[j]);
            if (sb >= 0) {
                cu_union(sa, sb);
            }
        }
    }
}

static int copper_connected(NsEntity *a, int ap, NsEntity *b, int bp) {
    int sa = pin_net_find(a, ap);
    int sb = pin_net_find(b, bp);
    if (sa < 0 || sb < 0) {
        return 0;
    }
    return cu_root(sa) == cu_root(sb);
}

static void nearest_open_partner(const R01aUi *ui, int chip, int pin, int *oc, int *op) {
    NsEntity *src = ui->chips[chip];
    int best = -1;
    int i;
    int pi;
    *oc = -1;
    *op = -1;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int ax;
            int ay;
            int bx;
            int by;
            int d;
            if (i == chip && pi == pin) {
                continue;
            }
            if (!pins_share_net(src, pin, e, pi) || copper_connected(src, pin, e, pi)) {
                continue;
            }
            if (!pin_center(src, pin, &ax, &ay) || !pin_center(e, pi, &bx, &by)) {
                continue;
            }
            d = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
            if (best < 0 || d < best) {
                best = d;
                *oc = i;
                *op = pi;
            }
        }
    }
}

static void draw_pad(SDL_Renderer *r, const R01aUi *ui, int cx, int cy, int hover) {
    int hx = R01A_PAD / 2;
    int x = board_sx(ui, cx - hx);
    int y = board_sy(ui, cy - hx);
    if (hover) {
        fill_rect(r, x, y, R01A_PAD, R01A_PAD, 230, 230, 230);
    } else {
        fill_rect(r, x, y, R01A_PAD, R01A_PAD, 186, 186, 186);
    }
}

static void draw_ic(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected, int hover_pin) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int bw = ic_body_w(e);
    int bh = ic_body_h(e);
    int i;
    fill_rect(r, x, y, bw, bh, selected ? 56 : 40, selected ? 56 : 40, selected ? 56 : 40);
    if (selected) {
        draw_rect(r, x, y, bw, bh, R01A_SEL_R, R01A_SEL_G, R01A_SEL_B);
    }
    for (i = 0; i < e->pin_count; i++) {
        int cx;
        int cy;
        if (!pin_center(e, i, &cx, &cy)) {
            continue;
        }
        draw_pad(r, ui, cx, cy, i == hover_pin);
    }
}

static void draw_passive_glyph(SDL_Renderer *r, const R01aUi *ui, NsEntity *e, int selected, int hover_pin) {
    int i;
    int w = glyph_w(e);
    int h = glyph_h(e);
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    Uint8 cr;
    Uint8 cg;
    Uint8 cb;
    entity_fill_rgb(e, &cr, &cg, &cb);
    fill_rect(r, x, y, w, h, cr, cg, cb);
    if (selected) {
        draw_rect(r, x, y, w, h, R01A_SEL_R, R01A_SEL_G, R01A_SEL_B);
    }
    for (i = 0; i < e->pin_count; i++) {
        int cx;
        int cy;
        if (!pin_center(e, i, &cx, &cy)) {
            continue;
        }
        draw_pad(r, ui, cx, cy, i == hover_pin);
    }
}

static const char *mode_label(const R01aUi *ui) {
    return ui->show_nets ? "Manual" : "Auto";
}

static void mode_btn_rect(const R01aUi *ui, SDL_Rect *rc) {
    int tw = r01a_font_text_width(mode_label(ui));
    rc->x = 4;
    rc->y = 2;
    rc->w = tw + 12;
    rc->h = r01a_font_line_h() + 4;
}

static void draw_mode_btn(SDL_Renderer *r, const R01aUi *ui) {
    SDL_Rect rc;
    mode_btn_rect(ui, &rc);
    fill_rect(r, rc.x, rc.y, rc.w, rc.h, 18, 18, 22);
    draw_rect(r, rc.x, rc.y, rc.w, rc.h, ui->show_nets ? 220 : 160, ui->show_nets ? 180 : 160, 80);
    r01a_font_draw(r, rc.x + 6, rc.y + 2, mode_label(ui), 230, 230, 200);
}

static void draw_legend(SDL_Renderer *r) {
    static const struct {
        const char *lab;
        Uint8 cr;
        Uint8 cg;
        Uint8 cb;
    } rows[] = {
        {"Resistor", 220, 140, 50},     {"Ceramic", 40, 170, 155}, {"Electrolytic", 150, 70, 200},
        {"Crystal", 190, 200, 55},      {"Diode", 210, 70, 110},   {"Header", 70, 100, 155},
    };
    int n = (int)(sizeof(rows) / sizeof(rows[0]));
    int i;
    int row_h = r01a_font_line_h() + 2;
    int lab_w = 0;
    int box_w;
    int box_h;
    int x;
    int y;
    for (i = 0; i < n; i++) {
        int tw = r01a_font_text_width(rows[i].lab);
        if (tw > lab_w) {
            lab_w = tw;
        }
    }
    box_w = 16 + lab_w;
    box_h = n * row_h + 4;
    x = NS_LOGIC_W - box_w - 4;
    y = NS_LOGIC_H - box_h - 4;
    fill_rect(r, x, y, box_w, box_h, 12, 12, 16);
    draw_rect(r, x, y, box_w, box_h, 70, 70, 80);
    for (i = 0; i < n; i++) {
        int iy = y + 2 + i * row_h;
        fill_rect(r, x + 3, iy + 2, 6, 6, rows[i].cr, rows[i].cg, rows[i].cb);
        r01a_font_draw(r, x + 12, iy, rows[i].lab, 210, 210, 210);
    }
}

static void draw_lcd(SDL_Renderer *r, R01aUi *ui, NsVideoSink *sink, int selected) {
    const uint8_t *rgb;
    SDL_Rect dst;
    int lcd_w;
    int lcd_h;
    ns_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
    rgb = ns_video_sink_rgb(sink);
    if (!rgb) {
        fill_rect(r, board_sx(ui, sink->base.board_x), board_sy(ui, sink->base.board_y), lcd_w, lcd_h, 0, 0,
                  0);
        draw_rect(r, board_sx(ui, sink->base.board_x) - 1, board_sy(ui, sink->base.board_y) - 1, lcd_w + 2,
                  lcd_h + 2, selected ? R01A_SEL_R : 180, selected ? R01A_SEL_G : 180,
                  selected ? R01A_SEL_B : 180);
        return;
    }
    if (!ui->lcd_tex) {
        ui->lcd_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, NS_VIDEO_W,
                                        NS_VIDEO_H);
        if (ui->lcd_tex) {
            SDL_SetTextureScaleMode(ui->lcd_tex, SDL_ScaleModeNearest);
        }
    }
    if (!ui->lcd_tex) {
        return;
    }
    SDL_UpdateTexture(ui->lcd_tex, NULL, rgb, NS_VIDEO_W * 3);
    dst.x = board_sx(ui, sink->base.board_x);
    dst.y = board_sy(ui, sink->base.board_y);
    dst.w = lcd_w;
    dst.h = lcd_h;
    SDL_RenderCopy(r, ui->lcd_tex, NULL, &dst);
    draw_rect(r, dst.x - 1, dst.y - 1, lcd_w + 2, lcd_h + 2, selected ? R01A_SEL_R : 180,
              selected ? R01A_SEL_G : 180, selected ? R01A_SEL_B : 180);
}

static Uint8 air_alpha(void) {
    Uint32 t = SDL_GetTicks() % 1000u;
    if (t < 500u) {
        return (Uint8)(t * 255u / 500u);
    }
    return (Uint8)((1000u - t) * 255u / 500u);
}

static void draw_air_wires(SDL_Renderer *r, const R01aUi *ui) {
    int i;
    int j;
    Uint8 a = air_alpha();
    for (i = 0; i < g_pin_slot_count; i++) {
        NsEntity *ea = g_pin_slots[i].entity;
        int pa = g_pin_slots[i].pin_index;
        int ax;
        int ay;
        int best = -1;
        int bx = 0;
        int by = 0;
        if (!pin_center(ea, pa, &ax, &ay)) {
            continue;
        }
        for (j = i + 1; j < g_pin_slot_count; j++) {
            NsEntity *eb = g_pin_slots[j].entity;
            int pb = g_pin_slots[j].pin_index;
            int tx;
            int ty;
            int d;
            if (pin_net_root(i) != pin_net_root(j) || copper_connected(ea, pa, eb, pb)) {
                continue;
            }
            if (!pin_center(eb, pb, &tx, &ty)) {
                continue;
            }
            d = (ax - tx) * (ax - tx) + (ay - ty) * (ay - ty);
            if (best < 0 || d < best) {
                best = d;
                bx = tx;
                by = ty;
            }
        }
        if (best >= 0) {
            draw_soft_line(r, board_sx(ui, ax), board_sy(ui, ay), board_sx(ui, bx), board_sy(ui, by), 40,
                           220, 80, a);
        }
    }
}

static void draw_traces(SDL_Renderer *r, const R01aUi *ui) {
    int i;
    int s;
    for (i = 0; i < ui->trace_n; i++) {
        const R01aTrace *t = &ui->traces[i];
        for (s = 0; s + 1 < t->n; s++) {
            draw_hard_line(r, board_sx(ui, t->x[s]), board_sy(ui, t->y[s]), board_sx(ui, t->x[s + 1]),
                           board_sy(ui, t->y[s + 1]), 20, 200, 70);
        }
    }
}

static void draw_arm(SDL_Renderer *r, const R01aUi *ui, int mx, int my) {
    int s;
    int lx;
    int ly;
    int sx;
    int sy;
    Uint32 now;
    if (!ui->arm || ui->arm_n < 1) {
        return;
    }
    for (s = 0; s + 1 < ui->arm_n; s++) {
        draw_hard_line(r, board_sx(ui, ui->arm_x[s]), board_sy(ui, ui->arm_y[s]),
                       board_sx(ui, ui->arm_x[s + 1]), board_sy(ui, ui->arm_y[s + 1]), 20, 200, 70);
    }
    lx = ui->arm_x[ui->arm_n - 1];
    ly = ui->arm_y[ui->arm_n - 1];
    snap_octant(lx, ly, mx, my, &sx, &sy);
    draw_hard_line(r, board_sx(ui, lx), board_sy(ui, ly), board_sx(ui, sx), board_sy(ui, sy), 40, 140, 50);
    if (ui->dest_chip >= 0 && ui->dest_pin >= 0) {
        int dx;
        int dy;
        if (pin_center(ui->chips[ui->dest_chip], ui->dest_pin, &dx, &dy)) {
            now = SDL_GetTicks();
            draw_ants_line(r, board_sx(ui, sx), board_sy(ui, sy), board_sx(ui, dx), board_sy(ui, dy), now);
        }
    }
}

static void update_scale(R01aUi *ui) {
    int ww;
    int wh;
    int sx;
    int sy;
    SDL_GetWindowSize(ui->win, &ww, &wh);
    sx = ww / NS_LOGIC_W;
    sy = wh / NS_LOGIC_H;
    ui->scale = sx < sy ? sx : sy;
    if (ui->scale < 1) {
        ui->scale = 1;
    }
}

static void logic_from_window(const R01aUi *ui, int wx, int wy, int *lx, int *ly) {
    int ww;
    int wh;
    int draw_w;
    int draw_h;
    int ox;
    int oy;
    SDL_GetWindowSize(ui->win, &ww, &wh);
    draw_w = NS_LOGIC_W * ui->scale;
    draw_h = NS_LOGIC_H * ui->scale;
    ox = (ww - draw_w) / 2;
    oy = (wh - draw_h) / 2;
    *lx = (wx - ox) / ui->scale;
    *ly = (wy - oy) / ui->scale;
}

static void present_frame(R01aUi *ui) {
    int ww;
    int wh;
    int draw_w;
    int draw_h;
    SDL_Rect dst;
    update_scale(ui);
    SDL_GetWindowSize(ui->win, &ww, &wh);
    draw_w = NS_LOGIC_W * ui->scale;
    draw_h = NS_LOGIC_H * ui->scale;
    dst.x = (ww - draw_w) / 2;
    dst.y = (wh - draw_h) / 2;
    dst.w = draw_w;
    dst.h = draw_h;
    SDL_SetRenderTarget(ui->rend, NULL);
    SDL_SetRenderDrawColor(ui->rend, 0, 0, 0, 255);
    SDL_RenderClear(ui->rend);
    if (ui->target) {
        SDL_RenderCopy(ui->rend, ui->target, NULL, &dst);
    }
    SDL_RenderPresent(ui->rend);
}

static void draw_frame(R01aUi *ui, R01aBoard *board) {
    int rank;
    (void)board;
    copper_rebuild(ui);
    SDL_SetRenderTarget(ui->rend, ui->target);
    SDL_SetRenderDrawColor(ui->rend, 0, 0, 0, 255);
    SDL_RenderClear(ui->rend);
    SDL_RenderSetScale(ui->rend, (float)canvas_zoom(ui), (float)canvas_zoom(ui));

    if (ui->show_nets) {
        int mx;
        int my;
        draw_air_wires(ui->rend, ui);
        draw_traces(ui->rend, ui);
        logic_to_board(ui, ui->mouse_lx, ui->mouse_ly, &mx, &my);
        draw_arm(ui->rend, ui, mx, my);
    }

    for (rank = 0; rank < ui->chip_count; rank++) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        int sel;
        int hp;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e) {
            continue;
        }
        sel = ci == ui->selected;
        hp = (ci == ui->hover_chip) ? ui->hover_pin : -1;
        if (e->visual == NS_ENTITY_VIS_DISPLAY) {
            draw_lcd(ui->rend, ui, (NsVideoSink *)e, sel);
        } else if (is_passive_glyph(e)) {
            draw_passive_glyph(ui->rend, ui, e, sel, hp);
        } else {
            draw_ic(ui->rend, ui, e, sel, hp);
        }
    }

    SDL_RenderSetScale(ui->rend, 1.0f, 1.0f);
    draw_mode_btn(ui->rend, ui);
    draw_legend(ui->rend);
    present_frame(ui);
}

static void arm_begin(R01aUi *ui, int chip, int pin) {
    int cx;
    int cy;
    if (!pin_center(ui->chips[chip], pin, &cx, &cy)) {
        return;
    }
    ui->arm = 1;
    ui->arm_n = 1;
    ui->arm_x[0] = (int16_t)cx;
    ui->arm_y[0] = (int16_t)cy;
    nearest_open_partner(ui, chip, pin, &ui->dest_chip, &ui->dest_pin);
}

static void arm_cancel(R01aUi *ui) {
    ui->arm = 0;
    ui->arm_n = 0;
    ui->dest_chip = -1;
    ui->dest_pin = -1;
}

static void arm_commit(R01aUi *ui) {
    R01aTrace *t;
    int i;
    if (!ui->arm || ui->arm_n < 2 || ui->trace_n >= R01A_TRACE_MAX) {
        arm_cancel(ui);
        return;
    }
    t = &ui->traces[ui->trace_n++];
    t->n = ui->arm_n;
    for (i = 0; i < ui->arm_n; i++) {
        t->x[i] = (int16_t)ui->arm_x[i];
        t->y[i] = (int16_t)ui->arm_y[i];
    }
    arm_cancel(ui);
}

static void arm_add_point(R01aUi *ui, int x, int y) {
    int lx;
    int ly;
    int sx;
    int sy;
    if (!ui->arm || ui->arm_n < 1 || ui->arm_n >= R01A_TRACE_PTS) {
        return;
    }
    lx = ui->arm_x[ui->arm_n - 1];
    ly = ui->arm_y[ui->arm_n - 1];
    snap_octant(lx, ly, x, y, &sx, &sy);
    if (sx == lx && sy == ly) {
        return;
    }
    ui->arm_x[ui->arm_n] = sx;
    ui->arm_y[ui->arm_n] = sy;
    ui->arm_n++;
}

static void arm_add_to_pad(R01aUi *ui, int px, int py) {
    int lx;
    int ly;
    if (!ui->arm || ui->arm_n < 1) {
        return;
    }
    arm_add_point(ui, px, py);
    lx = ui->arm_x[ui->arm_n - 1];
    ly = ui->arm_y[ui->arm_n - 1];
    if (lx == px && ly == py) {
        return;
    }
    if (ui->arm_n >= R01A_TRACE_PTS) {
        return;
    }
    if (lx != px && ly != py) {
        ui->arm_x[ui->arm_n] = px;
        ui->arm_y[ui->arm_n] = ly;
        ui->arm_n++;
        if (ui->arm_n >= R01A_TRACE_PTS) {
            return;
        }
    }
    ui->arm_x[ui->arm_n] = px;
    ui->arm_y[ui->arm_n] = py;
    ui->arm_n++;
}

static void rotate_selected(R01aUi *ui) {
    NsEntity *e;
    if (ui->selected < 0 || ui->selected >= ui->chip_count) {
        return;
    }
    e = ui->chips[ui->selected];
    if (!e) {
        return;
    }
    if (is_passive_glyph(e) || e->visual == NS_ENTITY_VIS_DISPLAY) {
        return;
    }
    ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
}

static void sim_frame(R01aBoard *board) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 budget = (freq * (Uint64)R01A_SIM_BUDGET_MS) / 1000u;
    int n = 0;
    while (n < R01A_SIM_MAX_STEPS_PER_FRAME) {
        r01a_board_step_dots(board, (uint32_t)R01A_DOTS_PER_STEP);
        n++;
        if ((SDL_GetPerformanceCounter() - t0) >= budget) {
            break;
        }
    }
}

static int handle_event(R01aUi *ui, R01aBoard *board, const SDL_Event *e, int lx, int ly) {
    int bx = 0;
    int by = 0;
    (void)board;
    ui->mouse_lx = lx;
    ui->mouse_ly = ly;
    logic_to_board(ui, lx, ly, &bx, &by);
    if (!hit_pin_at(ui, bx, by, &ui->hover_chip, &ui->hover_pin)) {
        ui->hover_chip = -1;
        ui->hover_pin = -1;
    }

    if (e->type == SDL_MOUSEWHEEL) {
        int dy = e->wheel.y;
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            dy = -dy;
        }
        if (SDL_GetModState() & KMOD_CTRL) {
            if (dy > 0) {
                canvas_zoom_by(ui, 1, lx, ly);
            } else if (dy < 0) {
                canvas_zoom_by(ui, -1, lx, ly);
            }
            return 1;
        }
        ui->pan_x -= e->wheel.x * 24;
        ui->pan_y -= dy * 24;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        ui->pan_x -= e->motion.xrel / (ui->scale > 0 ? ui->scale : 1);
        ui->pan_y -= e->motion.yrel / (ui->scale > 0 ? ui->scale : 1);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        if (ui->arm) {
            arm_cancel(ui);
        }
        ui->drag_pan = 1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_RIGHT) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        NsEntity *e_drag = ui->chips[ui->drag_chip];
        if (e_drag) {
            move_entity(e_drag, bx - ui->drag_grab_bx, by - ui->drag_grab_by);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ui->drag_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int ci = -1;
        int pi = -1;
        SDL_Rect btn;
        mode_btn_rect(ui, &btn);
        if (lx >= btn.x && ly >= btn.y && lx < btn.x + btn.w && ly < btn.y + btn.h) {
            ui->show_nets = !ui->show_nets;
            if (!ui->show_nets) {
                arm_cancel(ui);
            }
            return 1;
        }
        if (!ui->show_nets) {
            ci = hit_top_chip(ui, bx, by);
            if (ci >= 0) {
                ui->selected = ci;
                ui->drag_chip = ci;
                ui->drag_grab_bx = bx - ui->chips[ci]->board_x;
                ui->drag_grab_by = by - ui->chips[ci]->board_y;
            } else {
                ui->selected = -1;
            }
            return 1;
        }
        if (hit_pin_at(ui, bx, by, &ci, &pi)) {
            int cx;
            int cy;
            pin_center(ui->chips[ci], pi, &cx, &cy);
            if (ui->arm) {
                arm_add_to_pad(ui, cx, cy);
                arm_commit(ui);
            } else {
                arm_begin(ui, ci, pi);
            }
            ui->selected = ci;
            return 1;
        }
        if (ui->arm) {
            arm_add_point(ui, bx, by);
            return 1;
        }
        ci = hit_top_chip(ui, bx, by);
        if (ci >= 0) {
            ui->selected = ci;
            ui->drag_chip = ci;
            ui->drag_grab_bx = bx - ui->chips[ci]->board_x;
            ui->drag_grab_by = by - ui->chips[ci]->board_y;
            return 1;
        }
        ui->selected = -1;
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            if (ui->arm) {
                arm_cancel(ui);
                return 1;
            }
            return 2;
        }
        if (e->key.keysym.sym == SDLK_BACKSPACE && ui->arm && ui->arm_n > 1) {
            ui->arm_n--;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_RETURN && ui->arm) {
            arm_commit(ui);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_a && !e->key.repeat) {
            ui->show_nets = !ui->show_nets;
            if (!ui->show_nets) {
                arm_cancel(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_r) {
            rotate_selected(ui);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_SPACE && !e->key.repeat) {
            board->running = !board->running;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_LEFT) {
            ui->pan_x -= 24;
        }
        if (e->key.keysym.sym == SDLK_RIGHT) {
            ui->pan_x += 24;
        }
        if (e->key.keysym.sym == SDLK_UP) {
            ui->pan_y -= 24;
        }
        if (e->key.keysym.sym == SDLK_DOWN) {
            ui->pan_y += 24;
        }
        return 1;
    }
    return 0;
}

int r01a_ui_run(R01aBoard *board) {
    R01aUi ui;
    int quit = 0;
    int dummy_air = 1;
    if (!board) {
        return 1;
    }
    memset(&ui, 0, sizeof(ui));
    ui.selected = -1;
    ui.drag_chip = -1;
    ui.hover_chip = -1;
    ui.hover_pin = -1;
    ui.dest_chip = -1;
    ui.dest_pin = -1;
    ui.zoom = 1;
    ui.show_nets = 1;
    r01a_board_set_wire_mode(board, R01A_WIRE_AUTO);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    (void)r01a_font_init();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    ui.win = SDL_CreateWindow("Retr01 Tier A", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              NS_LOGIC_W * R01A_UI_SCALE, NS_LOGIC_H * R01A_UI_SCALE, SDL_WINDOW_RESIZABLE);
    if (!ui.win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        r01a_font_shutdown();
        SDL_Quit();
        return 1;
    }
    ui.rend = SDL_CreateRenderer(ui.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui.rend) {
        ui.rend = SDL_CreateRenderer(ui.win, -1, 0);
    }
    if (!ui.rend) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(ui.win);
        r01a_font_shutdown();
        SDL_Quit();
        return 1;
    }
    ui.target = SDL_CreateTexture(ui.rend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, NS_LOGIC_W,
                                  NS_LOGIC_H);
    if (!ui.target) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ui.rend);
        SDL_DestroyWindow(ui.win);
        r01a_font_shutdown();
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(ui.target, SDL_ScaleModeNearest);
    ui.scale = R01A_UI_SCALE;
    dummy_air = ui.show_nets;
    (void)r01a_layout_load(R01A_LAYOUT_FILE, board, &ui.pan_x, &ui.pan_y, &ui.zoom, &dummy_air);
    ui.show_nets = dummy_air ? 1 : 0;
    r01a_board_set_wire_mode(board, R01A_WIRE_AUTO);
    bind_chips(&ui, board);
    pin_net_build(&ui);

    while (!quit) {
        SDL_Event ev;
        update_scale(&ui);
        while (SDL_PollEvent(&ev)) {
            int lx = 0;
            int ly = 0;
            int rc;
            if (ev.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                logic_from_window(&ui, ev.button.x, ev.button.y, &lx, &ly);
            } else if (ev.type == SDL_MOUSEMOTION) {
                logic_from_window(&ui, ev.motion.x, ev.motion.y, &lx, &ly);
            } else {
                int mx = 0;
                int my = 0;
                SDL_GetMouseState(&mx, &my);
                logic_from_window(&ui, mx, my, &lx, &ly);
            }
            rc = handle_event(&ui, board, &ev, lx, ly);
            if (rc == 2) {
                quit = 1;
            }
        }
        if (board->running) {
            sim_frame(board);
        }
        draw_frame(&ui, board);
    }

    (void)r01a_layout_save(R01A_LAYOUT_FILE, board, ui.pan_x, ui.pan_y, canvas_zoom(&ui),
                          ui.show_nets ? 1 : 0);
    if (ui.lcd_tex) {
        SDL_DestroyTexture(ui.lcd_tex);
    }
    if (ui.target) {
        SDL_DestroyTexture(ui.target);
    }
    SDL_DestroyRenderer(ui.rend);
    SDL_DestroyWindow(ui.win);
    r01a_font_shutdown();
    SDL_Quit();
    return 0;
}
