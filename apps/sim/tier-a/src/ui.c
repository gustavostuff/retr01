#include "ui.h"
#include "ui_font.h"

#include "r01a_board.h"

#include "netlist_sim/board_layout.h"
#include "netlist_sim/entity.h"
#include "netlist_sim/island.h"
#include "netlist_sim/island_group.h"
#include "netlist_sim/outline.h"
#include "netlist_sim/types.h"
#include "netlist_sim/ui_assets.h"
#include "netlist_sim/video_sink.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define R01A_UI_SCALE 2
#define R01A_SIM_BUDGET_MS 8
#define R01A_SIM_MAX_STEPS_PER_FRAME 24
#define R01A_DOTS_PER_STEP 32
#define R01A_BOARD_MAX_CHIPS 16
#define R01A_PAN_OVERSCROLL (NS_LOGIC_W / 2)
#define R01A_UI_TOOLTIP_DELAY_MS 400
#define R01A_SEL_YELLOW_R 255
#define R01A_SEL_YELLOW_G 220
#define R01A_SEL_YELLOW_B 80

typedef struct R01aUi {
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *target;
    SDL_Texture *lcd_tex;
    int scale;
    NsEntity *chips[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_z[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_sel[R01A_BOARD_MAX_CHIPS];
    int sel_start_x[R01A_BOARD_MAX_CHIPS];
    int sel_start_y[R01A_BOARD_MAX_CHIPS];
    int chip_count;
    int selected;
    int pan_x;
    int pan_y;
    int drag_pan;
    int drag_chip;
    int drag_grab_bx;
    int drag_grab_by;
    int drag_last_x;
    int drag_last_y;
    int sel_drag_ox;
    int sel_drag_oy;
    int box_sel;
    int box_bx0, box_by0, box_bx1, box_by1;
    int mouse_lx;
    int mouse_ly;
    int tip_stable_mx;
    int tip_stable_my;
    Uint32 tip_show_at;
} R01aUi;

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

static void fill_rect_a(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_Rect rc = {x, y, w, h};
    if (A == 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

static int board_sx(const R01aUi *ui, int bx) {
    return bx - ui->pan_x;
}

static int board_sy(const R01aUi *ui, int by) {
    return by - ui->pan_y;
}

static void logic_to_board(const R01aUi *ui, int lx, int ly, int *bx, int *by) {
    *bx = lx + ui->pan_x;
    *by = ly + ui->pan_y;
}

static void clamp_pan(R01aUi *ui) {
    int over = R01A_PAN_OVERSCROLL;
    int min_x = -over;
    int min_y = -over;
    int max_x = NS_BOARD_W + over - NS_LOGIC_W;
    int max_y = NS_BOARD_H + over - NS_LOGIC_H;
    if (max_x < min_x) {
        max_x = min_x;
    }
    if (max_y < min_y) {
        max_y = min_y;
    }
    if (ui->pan_x < min_x) {
        ui->pan_x = min_x;
    }
    if (ui->pan_y < min_y) {
        ui->pan_y = min_y;
    }
    if (ui->pan_x > max_x) {
        ui->pan_x = max_x;
    }
    if (ui->pan_y > max_y) {
        ui->pan_y = max_y;
    }
}

static void clamp_chip(NsEntity *e) {
    int over = NS_BOARD_W / 2;
    int min_x = -over;
    int min_y = -over;
    int max_x;
    int max_y;
    int bx;
    int by;

    if (!e) {
        return;
    }
    max_x = NS_BOARD_W + over - e->body_w;
    max_y = NS_BOARD_H + over - e->body_h;
    if (max_x < min_x) {
        max_x = min_x;
    }
    if (max_y < min_y) {
        max_y = min_y;
    }
    bx = e->board_x;
    by = e->board_y;
    if (bx < min_x) {
        bx = min_x;
    }
    if (by < min_y) {
        by = min_y;
    }
    if (bx > max_x) {
        bx = max_x;
    }
    if (by > max_y) {
        by = max_y;
    }
    ns_entity_place(e, bx, by);
}

static int chip_hidden(const NsEntity *e) {
    (void)e;
    return 0;
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
        int zi;
        if (!e) {
            continue;
        }
        zi = ui->chip_count;
        ui->chips[zi] = e;
        ui->chip_z[zi] = (uint8_t)zi;
        ui->chip_count++;
    }
}

static void chip_z_raise(R01aUi *ui, int chip_i) {
    int r;
    int dst = 0;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    for (r = 0; r < ui->chip_count; r++) {
        if (ui->chip_z[r] != (uint8_t)chip_i) {
            ui->chip_z[dst++] = ui->chip_z[r];
        }
    }
    ui->chip_z[dst] = (uint8_t)chip_i;
}

static int sel_count(const R01aUi *ui) {
    int i;
    int n = 0;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_sel[i]) {
            n++;
        }
    }
    return n;
}

static void sel_clear(R01aUi *ui) {
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    ui->selected = -1;
}

static void sel_set_one(R01aUi *ui, int chip_i) {
    sel_clear(ui);
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    ui->chip_sel[chip_i] = 1;
    ui->selected = chip_i;
}

static void sel_toggle(R01aUi *ui, int chip_i) {
    int i;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    ui->chip_sel[chip_i] = ui->chip_sel[chip_i] ? 0 : 1;
    if (ui->chip_sel[chip_i]) {
        ui->selected = chip_i;
        return;
    }
    if (ui->selected == chip_i) {
        ui->selected = -1;
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chip_sel[i]) {
                ui->selected = i;
                break;
            }
        }
    }
}

static int chip_in_box(const NsEntity *e, int x0, int y0, int x1, int y1) {
    int tmp;
    if (!e) {
        return 0;
    }
    if (x0 > x1) {
        tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    return e->board_x < x1 && e->board_x + e->body_w > x0 && e->board_y < y1 && e->board_y + e->body_h > y0;
}

static void sel_from_box(R01aUi *ui, int additive) {
    int i;
    int first = -1;
    if (!additive) {
        sel_clear(ui);
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (chip_hidden(ui->chips[i]) || !chip_in_box(ui->chips[i], ui->box_bx0, ui->box_by0, ui->box_bx1, ui->box_by1)) {
            continue;
        }
        ui->chip_sel[i] = 1;
        if (first < 0) {
            first = i;
        }
    }
    if (first >= 0) {
        ui->selected = first;
    }
}

static void begin_sel_drag(R01aUi *ui, int board_mx, int board_my) {
    int i;
    ui->sel_drag_ox = board_mx;
    ui->sel_drag_oy = board_my;
    for (i = 0; i < ui->chip_count; i++) {
        ui->sel_start_x[i] = ui->chips[i] ? ui->chips[i]->board_x : 0;
        ui->sel_start_y[i] = ui->chips[i] ? ui->chips[i]->board_y : 0;
    }
}

static void move_chip_drag(R01aUi *ui, int chip_i, int board_mx, int board_my) {
    NsEntity *e;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e) {
        return;
    }
    ns_entity_place(e, board_mx - ui->drag_grab_bx, board_my - ui->drag_grab_by);
    clamp_chip(e);
}

static void move_selection_drag(R01aUi *ui, int board_mx, int board_my) {
    int i;
    int dx = board_mx - ui->sel_drag_ox;
    int dy = board_my - ui->sel_drag_oy;
    for (i = 0; i < ui->chip_count; i++) {
        if (!ui->chip_sel[i] || !ui->chips[i]) {
            continue;
        }
        ns_entity_place(ui->chips[i], ui->sel_start_x[i] + dx, ui->sel_start_y[i] + dy);
        clamp_chip(ui->chips[i]);
    }
}

static int hit_chip_body(const R01aUi *ui, const NsEntity *e, int lx, int ly) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int pad = NS_CHIP_PIN_OUT;
    return lx >= x - pad && lx < x + e->body_w + pad && ly >= y - pad && ly < y + e->body_h + pad;
}

static int hit_top_chip(const R01aUi *ui, int lx, int ly) {
    int rank;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        if (ci < 0 || ci >= ui->chip_count || !ui->chips[ci] || chip_hidden(ui->chips[ci])) {
            continue;
        }
        if (hit_chip_body(ui, ui->chips[ci], lx, ly)) {
            return ci;
        }
    }
    return -1;
}

static int rotate_selected(R01aUi *ui) {
    int i;
    int n = 0;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e;
        if (!ui->chip_sel[i] && i != ui->selected) {
            continue;
        }
        e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_IC) {
            continue;
        }
        ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
        clamp_chip(e);
        n++;
    }
    return n;
}

static void pin_level_rgb(NsLevel lvl, NsPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
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

static void dip_pin_pos(const NsEntity *e, int pin_num, int *along, int *side_pin1) {
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int half = dip / 2;
    int idx;
    int span;
    int pitch = NS_DIP_PIN_PITCH_PX;
    int row_span;
    int margin;
    int reverse;

    if (dip <= 0 || pin_num <= 0 || pin_num > dip) {
        *side_pin1 = 1;
        *along = ns_orient_is_horiz(e->orient) ? (e->body_w / 2) : (e->body_h / 2);
        return;
    }
    *side_pin1 = pin_num <= half;
    idx = *side_pin1 ? (pin_num - 1) : (dip - pin_num);
    span = ns_orient_is_horiz(e->orient) ? e->body_w : e->body_h;
    row_span = (half > 1) ? (half - 1) * pitch : 0;
    margin = (span - row_span) / 2;
    if (margin < 1) {
        margin = 1;
    }
    reverse = (e->orient == NS_ORIENT_180 || e->orient == NS_ORIENT_270);
    if (reverse) {
        *along = margin + (half > 0 ? (half - 1 - idx) : 0) * pitch;
    } else {
        *along = margin + idx * pitch;
    }
}

static void blit_pin_rot(SDL_Renderer *r, int dx, int dy, int rot, const Uint8 *tint_rgb) {
    int w = NS_UI_PIN_W;
    int h = NS_UI_PIN_H;
    int ox0 = NS_UI_PIN_OX;
    int oy0 = NS_UI_PIN_OY;
    int sx, sy, ox, oy, ow, oh;
    int tl_x, tl_y;
    int org_x, org_y;

    if (!r) {
        return;
    }
    if (rot == 1 || rot == 3) {
        ow = h;
        oh = w;
    } else {
        ow = w;
        oh = h;
    }
    if (rot == 0) {
        org_x = ox0;
        org_y = oy0;
    } else if (rot == 1) {
        org_x = h - 1 - oy0;
        org_y = ox0;
    } else if (rot == 2) {
        org_x = w - 1 - ox0;
        org_y = h - 1 - oy0;
    } else {
        org_x = oy0;
        org_y = w - 1 - ox0;
    }
    tl_x = dx - org_x;
    tl_y = dy - org_y;
    for (oy = 0; oy < oh; oy++) {
        for (ox = 0; ox < ow; ox++) {
            const uint8_t *p;
            if (rot == 0) {
                sx = ox;
                sy = oy;
            } else if (rot == 1) {
                sx = oy;
                sy = h - 1 - ox;
            } else if (rot == 2) {
                sx = w - 1 - ox;
                sy = h - 1 - oy;
            } else {
                sx = w - 1 - oy;
                sy = ox;
            }
            p = NS_UI_PIN_RGBA + ((size_t)sy * (size_t)w + (size_t)sx) * 4u;
            if (p[3] == 0) {
                continue;
            }
            if (tint_rgb) {
                SDL_SetRenderDrawColor(r, tint_rgb[0], tint_rgb[1], tint_rgb[2], 255);
            } else {
                SDL_SetRenderDrawColor(r, p[0], p[1], p[2], 255);
            }
            SDL_RenderDrawPoint(r, tl_x + ox, tl_y + oy);
        }
    }
}

static int pin_tip_reach(void) {
    return NS_UI_PIN_H - 1 - NS_UI_PIN_OY;
}

static void draw_dip_pad_h(SDL_Renderer *r, int px, int body_edge_y, int outward_down, const Uint8 *tint_rgb) {
    int reach = pin_tip_reach();
    if (outward_down) {
        blit_pin_rot(r, px, body_edge_y + reach, 2, tint_rgb);
    } else {
        blit_pin_rot(r, px, body_edge_y - 1 - reach, 0, tint_rgb);
    }
}

static void draw_dip_pad_v(SDL_Renderer *r, int py, int body_edge_x, int outward_left, const Uint8 *tint_rgb) {
    int reach = pin_tip_reach();
    if (outward_left) {
        blit_pin_rot(r, body_edge_x - 1 - reach, py, 3, tint_rgb);
    } else {
        blit_pin_rot(r, body_edge_x + reach, py, 1, tint_rgb);
    }
}

static void blit_rgba(SDL_Renderer *r, int dx, int dy, const uint8_t *rgba, int w, int h) {
    int x;
    int y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const uint8_t *p = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            if (p[3] == 0) {
                continue;
            }
            SDL_SetRenderDrawColor(r, p[0], p[1], p[2], 255);
            SDL_RenderDrawPoint(r, dx + x, dy + y);
        }
    }
}

static unsigned label_seed(const NsEntity *e) {
    unsigned seed = 0;
    const char *s = e->refdes ? e->refdes : (e->part ? e->part : "");
    for (; *s; s++) {
        seed = seed * 131u + (unsigned char)*s;
    }
    return seed;
}

static void draw_selection(SDL_Renderer *r, int x, int y, int w, int h, int selected) {
    if (selected) {
        draw_rect(r, x, y, w, h, R01A_SEL_YELLOW_R, R01A_SEL_YELLOW_G, R01A_SEL_YELLOW_B);
    }
}

static void draw_ic(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int i;
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int horiz = ns_orient_is_horiz(e->orient);
    Uint8 br = selected ? 40 : 28;
    Uint8 bg = selected ? 48 : 32;
    Uint8 bb = selected ? 36 : 28;
    NsOutlineRgb oc = ns_outline_rgb(e->health);

    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int along;
        int side_pin1;
        Uint8 tint[3];
        if (num < 1 || num > dip) {
            continue;
        }
        dip_pin_pos(e, num, &along, &side_pin1);
        pin_level_rgb(e->pins[i].level, e->pins[i].dir, &tint[0], &tint[1], &tint[2]);
        switch (e->orient) {
        case NS_ORIENT_90:
            draw_dip_pad_v(r, y + along, side_pin1 ? x : (x + e->body_w), side_pin1, tint);
            break;
        case NS_ORIENT_180:
            draw_dip_pad_h(r, x + along, side_pin1 ? y : (y + e->body_h), !side_pin1, tint);
            break;
        case NS_ORIENT_270:
            draw_dip_pad_v(r, y + along, side_pin1 ? (x + e->body_w) : x, !side_pin1, tint);
            break;
        case NS_ORIENT_0:
        default:
            draw_dip_pad_h(r, x + along, side_pin1 ? (y + e->body_h) : y, side_pin1, tint);
            break;
        }
    }
    fill_rect(r, x, y, e->body_w, e->body_h, br, bg, bb);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, R01A_SEL_YELLOW_R, R01A_SEL_YELLOW_G, R01A_SEL_YELLOW_B);
    } else {
        draw_rect(r, x, y, e->body_w, e->body_h, oc.r, oc.g, oc.b);
    }
    switch (e->orient) {
    case NS_ORIENT_90:
        fill_rect(r, x + e->body_w / 2 - 2, y - 1, 4, 2, 20, 22, 20);
        break;
    case NS_ORIENT_180:
        fill_rect(r, x + e->body_w - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
        break;
    case NS_ORIENT_270:
        fill_rect(r, x + e->body_w / 2 - 2, y + e->body_h - 1, 4, 2, 20, 22, 20);
        break;
    case NS_ORIENT_0:
    default:
        fill_rect(r, x - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
        break;
    }
    {
        const char *label = e->part ? e->part : e->refdes;
        if (label && label[0]) {
            int pad = 2;
            int view_w = e->body_w - pad * 2;
            int view_h = e->body_h - pad * 2;
            if (view_w < 1) {
                view_w = e->body_w;
                pad = 0;
            }
            if (view_h < 1) {
                view_h = e->body_h;
                pad = 0;
            }
            if (horiz) {
                r01a_draw_label_bounce(r, x + pad, y + pad, view_w, view_h, label, label_seed(e), 255, 255, 255, 128);
            } else {
                r01a_draw_label_bounce_rot90ccw(r, x + pad, y + pad, view_w, view_h, label, label_seed(e), 255, 255,
                                                255, 128);
            }
        }
    }
}

static void draw_pwr(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int ix = x + (e->body_w - NS_UI_BATTERY_W) / 2;
    int iy = y + 2;
    fill_rect(r, x, y, e->body_w, e->body_h, 0, 0, 0);
    blit_rgba(r, ix, iy, NS_UI_BATTERY_RGBA, NS_UI_BATTERY_W, NS_UI_BATTERY_H);
    draw_selection(r, x, y, e->body_w, e->body_h, selected);
}

static void draw_osc(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int ix = x + (e->body_w - NS_UI_OSC_W) / 2;
    int iy = y + (e->body_h - NS_UI_OSC_H) / 2;
    fill_rect(r, x, y, e->body_w, e->body_h, 0, 0, 0);
    blit_rgba(r, ix, iy, NS_UI_OSC_RGBA, NS_UI_OSC_W, NS_UI_OSC_H);
    draw_selection(r, x, y, e->body_w, e->body_h, selected);
}

static void draw_lcd(SDL_Renderer *r, R01aUi *ui, NsVideoSink *sink, int selected) {
    const uint8_t *rgb;
    SDL_Rect dst;
    int x = board_sx(ui, sink->base.board_x);
    int y = board_sy(ui, sink->base.board_y);
    int lcd_w;
    int lcd_h;

    ns_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
    rgb = ns_video_sink_rgb(sink);
    if (!rgb) {
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
    dst.x = x;
    dst.y = y;
    dst.w = lcd_w;
    dst.h = lcd_h;
    SDL_RenderCopy(r, ui->lcd_tex, NULL, &dst);
    draw_selection(r, x, y, sink->base.body_w, sink->base.body_h, selected);
}

static void draw_entity(SDL_Renderer *r, R01aUi *ui, NsEntity *e, int selected) {
    if (!e || chip_hidden(e)) {
        return;
    }
    switch (e->visual) {
    case NS_ENTITY_VIS_PWR:
        draw_pwr(r, ui, e, selected);
        break;
    case NS_ENTITY_VIS_OSC:
        draw_osc(r, ui, e, selected);
        break;
    case NS_ENTITY_VIS_DISPLAY:
        draw_lcd(r, ui, (NsVideoSink *)e, selected);
        break;
    case NS_ENTITY_VIS_IC:
    default:
        draw_ic(r, ui, e, selected);
        break;
    }
}

static void draw_tooltip(SDL_Renderer *r, int lx, int ly, const char *text) {
    int tw;
    int pad = 4;
    int box_x;
    int box_y;
    int box_w;
    int box_h = r01a_font_line_h() + pad * 2;

    if (!text || !text[0]) {
        return;
    }
    tw = r01a_font_text_width(text);
    box_w = tw + pad * 2;
    box_x = lx + 14;
    box_y = ly + 16;
    if (box_x + box_w > NS_LOGIC_W - 4) {
        box_x = lx - box_w - 8;
    }
    if (box_y + box_h > NS_LOGIC_H - 4) {
        box_y = ly - box_h - 8;
    }
    if (box_x < 4) {
        box_x = 4;
    }
    if (box_y < 4) {
        box_y = 4;
    }
    fill_rect(r, box_x, box_y, box_w, box_h, 255, 245, 180);
    draw_rect(r, box_x, box_y, box_w, box_h, 200, 180, 100);
    r01a_font_draw(r, box_x + pad, box_y + pad, text, 0, 0, 0);
}

static void fill_tooltip(const R01aUi *ui, char *out, size_t out_len) {
    int chip_i;
    const NsEntity *e;
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    chip_i = hit_top_chip(ui, ui->mouse_lx, ui->mouse_ly);
    if (chip_i < 0) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e) {
        return;
    }
    snprintf(out, out_len, "%s (%s)", e->refdes ? e->refdes : "?", e->part ? e->part : "?");
}

static void present_frame(R01aUi *ui);

static void draw_frame(R01aUi *ui, R01aBoard *board) {
    char status[128];
    char tip[96];
    int rank;

    SDL_SetRenderTarget(ui->rend, ui->target);
    SDL_SetRenderDrawColor(ui->rend, NS_BOARD_BG_R, NS_BOARD_BG_G, NS_BOARD_BG_B, 255);
    SDL_RenderClear(ui->rend);

    for (rank = 0; rank < ui->chip_count; rank++) {
        int ci = ui->chip_z[rank];
        int selected;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        selected = ui->chip_sel[ci] || ci == ui->selected;
        draw_entity(ui->rend, ui, ui->chips[ci], selected);
    }

    if (ui->box_sel) {
        int x0 = board_sx(ui, ui->box_bx0 < ui->box_bx1 ? ui->box_bx0 : ui->box_bx1);
        int y0 = board_sy(ui, ui->box_by0 < ui->box_by1 ? ui->box_by0 : ui->box_by1);
        int x1 = board_sx(ui, ui->box_bx0 < ui->box_bx1 ? ui->box_bx1 : ui->box_bx0);
        int y1 = board_sy(ui, ui->box_by0 < ui->box_by1 ? ui->box_by1 : ui->box_by0);
        int bw = x1 - x0;
        int bh = y1 - y0;
        if (bw < 1) {
            bw = 1;
        }
        if (bh < 1) {
            bh = 1;
        }
        fill_rect_a(ui->rend, x0, y0, bw, bh, 80, 180, 120, 40);
        draw_rect(ui->rend, x0, y0, bw, bh, 80, 180, 120);
    }

    ns_island_group_fill_status(r01a_board_group(board), status, sizeof(status));
    r01a_font_draw_a(ui->rend, 4, 2, board->running ? "RUN" : "PAUSE", 180, 180, 120, 180);
    r01a_font_draw_a(ui->rend, 4 + r01a_font_text_width("PAUSE") + 10, 2, status, 200, 210, 200, 160);

    if (SDL_GetTicks() >= ui->tip_show_at && !ui->drag_pan && ui->drag_chip < 0 && !ui->box_sel) {
        fill_tooltip(ui, tip, sizeof(tip));
        if (tip[0]) {
            draw_tooltip(ui->rend, ui->mouse_lx, ui->mouse_ly, tip);
        }
    }
    present_frame(ui);
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
    int scale = ui->scale > 0 ? ui->scale : 1;
    int draw_w;
    int draw_h;
    int ox;
    int oy;
    SDL_GetWindowSize(ui->win, &ww, &wh);
    draw_w = NS_LOGIC_W * scale;
    draw_h = NS_LOGIC_H * scale;
    ox = (ww - draw_w) / 2;
    oy = (wh - draw_h) / 2;
    *lx = (wx - ox) / scale;
    *ly = (wy - oy) / scale;
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

static void tip_reset(R01aUi *ui, int mx, int my) {
    ui->tip_stable_mx = mx;
    ui->tip_stable_my = my;
    ui->tip_show_at = SDL_GetTicks() + R01A_UI_TOOLTIP_DELAY_MS;
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
    int board_mx = 0;
    int board_my = 0;

    ui->mouse_lx = lx;
    ui->mouse_ly = ly;
    logic_to_board(ui, lx, ly, &board_mx, &board_my);

    if (e->type == SDL_MOUSEMOTION && (lx != ui->tip_stable_mx || ly != ui->tip_stable_my)) {
        tip_reset(ui, lx, ly);
    }
    if (e->type == SDL_MOUSEWHEEL) {
        ui->pan_x -= e->wheel.x * 32;
        ui->pan_y -= e->wheel.y * 32;
        clamp_pan(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 1;
        ui->drag_last_x = lx;
        ui->drag_last_y = ly;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        ui->drag_pan = 1;
        ui->drag_last_x = lx;
        ui->drag_last_y = ly;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP &&
        (e->button.button == SDL_BUTTON_MIDDLE || e->button.button == SDL_BUTTON_RIGHT)) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        ui->pan_x -= (lx - ui->drag_last_x);
        ui->pan_y -= (ly - ui->drag_last_y);
        ui->drag_last_x = lx;
        ui->drag_last_y = ly;
        clamp_pan(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->box_sel) {
        ui->box_bx1 = board_mx;
        ui->box_by1 = board_my;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        if (sel_count(ui) > 1) {
            move_selection_drag(ui, board_mx, board_my);
        } else {
            move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (ui->box_sel) {
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            int w = ui->box_bx1 - ui->box_bx0;
            int h = ui->box_by1 - ui->box_by0;
            if (w < 0) {
                w = -w;
            }
            if (h < 0) {
                h = -h;
            }
            ui->box_sel = 0;
            if (w >= 4 || h >= 4) {
                sel_from_box(ui, shift);
            } else if (!shift) {
                sel_clear(ui);
            }
            return 1;
        }
        ui->drag_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int chip_i;
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

        if (e->button.clicks == 2) {
            chip_i = hit_top_chip(ui, lx, ly);
            if (chip_i >= 0 && ui->chips[chip_i] && ui->chips[chip_i]->visual == NS_ENTITY_VIS_DISPLAY) {
                NsVideoSink *sink = (NsVideoSink *)ui->chips[chip_i];
                ns_video_sink_set_scale_2x(sink, !ns_video_sink_scale_2x(sink));
                return 1;
            }
        }

        ui->drag_chip = -1;
        chip_i = hit_top_chip(ui, lx, ly);
        if (chip_i >= 0) {
            chip_z_raise(ui, chip_i);
            if (shift) {
                sel_toggle(ui, chip_i);
                return 1;
            }
            if (ui->chip_sel[chip_i] && sel_count(ui) > 1) {
                ui->selected = chip_i;
                ui->drag_chip = chip_i;
                ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                begin_sel_drag(ui, board_mx, board_my);
                return 1;
            }
            sel_set_one(ui, chip_i);
            ui->drag_chip = chip_i;
            ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
            ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
            begin_sel_drag(ui, board_mx, board_my);
            return 1;
        }
        if (!shift) {
            sel_clear(ui);
        }
        ui->box_sel = 1;
        ui->box_bx0 = ui->box_bx1 = board_mx;
        ui->box_by0 = ui->box_by1 = board_my;
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        int step = 48;
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            return 2;
        }
        if (e->key.keysym.sym == SDLK_SPACE) {
            board->running = !board->running;
            if (r01a_board_group(board)) {
                r01a_board_group(board)->running = board->running;
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_PERIOD) {
            r01a_board_step(board);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_r && (e->key.keysym.mod & KMOD_CTRL)) {
            r01a_board_reset(board);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_r) {
            rotate_selected(ui);
            return 1;
        }
        if (e->key.keysym.mod & KMOD_SHIFT) {
            if (e->key.keysym.sym == SDLK_LEFT) {
                ui->pan_x -= step;
                clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHT) {
                ui->pan_x += step;
                clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_UP) {
                ui->pan_y -= step;
                clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_DOWN) {
                ui->pan_y += step;
                clamp_pan(ui);
                return 1;
            }
        }
    }
    return 0;
}

int r01a_ui_run(R01aBoard *board) {
    R01aUi ui;
    int quit = 0;

    memset(&ui, 0, sizeof(ui));
    ui.selected = -1;
    ui.drag_chip = -1;
    bind_chips(&ui, board);
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
    tip_reset(&ui, 0, 0);

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
