#include "ui.h"
#include "ui_font.h"

#include "r01a_board.h"
#include "r01a_layout.h"

#include "netlist_sim/board_layout.h"
#include "netlist_sim/breadboard.h"
#include "netlist_sim/entity.h"
#include "netlist_sim/island.h"
#include "netlist_sim/island_group.h"
#include "netlist_sim/outline.h"
#include "netlist_sim/passive.h"
#include "netlist_sim/types.h"
#include "netlist_sim/ui_assets.h"
#include "netlist_sim/video_sink.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#ifndef R01A_LAYOUT_FILE
#define R01A_LAYOUT_FILE "ui_layout.json"
#endif

#define R01A_UI_SCALE 2
#define R01A_SIM_BUDGET_MS 8
#define R01A_SIM_MAX_STEPS_PER_FRAME 24
#define R01A_DOTS_PER_STEP 32
#define R01A_BOARD_MAX_CHIPS 64
#define R01A_PAN_OVERSCROLL (NS_LOGIC_W / 2)
#define R01A_UI_TOOLTIP_DELAY_MS 400
#define R01A_SEL_YELLOW_R 255
#define R01A_SEL_YELLOW_G 220
#define R01A_SEL_YELLOW_B 80
#define R01A_PIN_HIT_PX 5
#define R01A_PIN_NET_SLOTS 512
#define R01A_HOVER_JOG 8
#define R01A_JUMPER_HIT_PX 5
#define R01A_JUMPER_END_PX 6
#define R01A_JUMPER_COLORS 10

static const Uint8 k_jumper_rgb[R01A_JUMPER_COLORS][3] = {
    {220, 50, 50},   {230, 140, 40},  {230, 200, 50}, {50, 180, 70},  {50, 120, 220},
    {160, 70, 200},  {50, 50, 50},    {230, 230, 230}, {150, 95, 55}, {150, 150, 155},
};

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
    int jumper_arm;
    NsPbHole jumper_from;
    NsBreadboard *jumper_bb;
    int jumper_mode;
    int jumper_color_i;
    uint8_t jumper_sel[R01A_JUMPER_MAX];
    int hover_jumper;
    int drag_jumper;
    int drag_jumper_end;
    int drag_jumper_preview_ok;
    NsPbHole drag_jumper_preview;
    int hover_chip;
    int hover_pin;
    int tip_stable_mx;
    int tip_stable_my;
    Uint32 tip_show_at;
    int right_armed;
    int right_pan;
    int right_lx;
    int right_ly;
    int ctx_open;
    int ctx_lx;
    int ctx_ly;
    int ctx_bx;
    int ctx_by;
    int fps;
    int fps_n;
    Uint32 fps_t0;
    int pin_gray;
} R01aUi;

static NsBreadboard *ui_bb_named(const R01aUi *ui, const char *ref);
static NsBreadboard *hit_breadboard(const R01aUi *ui, int mx, int my, NsPbHole *hole);
static NsBreadboard *ui_breadboard(const R01aUi *ui);

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
    move_entity(e, bx, by);
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
    {
        uint8_t ordered[R01A_BOARD_MAX_CHIPS];
        int n = 0;
        int k;
        for (k = 0; k < ui->chip_count; k++) {
            if (ui->chips[k] && ui->chips[k]->visual == NS_ENTITY_VIS_BREADBOARD) {
                ordered[n++] = (uint8_t)k;
            }
        }
        for (k = 0; k < ui->chip_count; k++) {
            if (!ui->chips[k] || ui->chips[k]->visual != NS_ENTITY_VIS_BREADBOARD) {
                ordered[n++] = (uint8_t)k;
            }
        }
        for (k = 0; k < n; k++) {
            ui->chip_z[k] = ordered[k];
        }
    }
}

static void chip_z_raise(R01aUi *ui, int chip_i) {
    int r;
    int dst = 0;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    if (ui->chips[chip_i] && ui->chips[chip_i]->visual == NS_ENTITY_VIS_BREADBOARD) {
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
    memset(ui->jumper_sel, 0, sizeof(ui->jumper_sel));
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

static void sel_from_box(R01aUi *ui, R01aBoard *board, int additive) {
    int i;
    int first = -1;
    int x0 = ui->box_bx0;
    int y0 = ui->box_by0;
    int x1 = ui->box_bx1;
    int y1 = ui->box_by1;
    int tmp;
    if (!additive) {
        sel_clear(ui);
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
    for (i = 0; i < ui->chip_count; i++) {
        if (chip_hidden(ui->chips[i]) || !chip_in_box(ui->chips[i], ui->box_bx0, ui->box_by0, ui->box_bx1, ui->box_by1)) {
            continue;
        }
        ui->chip_sel[i] = 1;
        if (first < 0) {
            first = i;
        }
    }
    if (board) {
        for (i = 0; i < board->jumper_count; i++) {
            const NsBreadboard *bb = ui_bb_named(ui, board->jumpers[i].bb_ref);
            int ax;
            int ay;
            int bx;
            int by;
            if (!bb) {
                continue;
            }
            ns_breadboard_hole_world(bb, board->jumpers[i].a, &ax, &ay);
            ns_breadboard_hole_world(bb, board->jumpers[i].b, &bx, &by);
            if ((ax >= x0 && ax <= x1 && ay >= y0 && ay <= y1) || (bx >= x0 && bx <= x1 && by >= y0 && by <= y1)) {
                ui->jumper_sel[i] = 1;
            }
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
    move_entity(e, board_mx - ui->drag_grab_bx, board_my - ui->drag_grab_by);
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
        move_entity(ui->chips[i], ui->sel_start_x[i] + dx, ui->sel_start_y[i] + dy);
        clamp_chip(ui->chips[i]);
    }
}

static int entity_tip_board(const NsEntity *e, int pin_num, int *tx, int *ty) {
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return ns_passive_tip_board((const NsPassive *)e, pin_num, tx, ty);
    }
    return ns_entity_pin_tip_board(e, pin_num, tx, ty);
}

static int entity_pin_hi(const NsEntity *e) {
    int i;
    int hi = 0;
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_IC) {
        return e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return e->pin_count;
    }
    if (e->visual != NS_ENTITY_VIS_OSC) {
        return 0;
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].number > hi) {
            hi = e->pins[i].number;
        }
    }
    return hi;
}

static NsBreadboard *ui_breadboard(const R01aUi *ui) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] && ui->chips[i]->visual == NS_ENTITY_VIS_BREADBOARD) {
            return (NsBreadboard *)ui->chips[i];
        }
    }
    return NULL;
}

static int count_pins_on_holes(const R01aUi *ui, const NsEntity *e, int dx, int dy) {
    int n;
    int count = 0;
    int pin_hi = entity_pin_hi(e);
    int bi;

    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        if (!entity_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        tx += dx;
        ty += dy;
        for (bi = 0; bi < ui->chip_count; bi++) {
            const NsEntity *be = ui->chips[bi];
            NsPbHole h;
            int hx;
            int hy;
            if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD || be == e) {
                continue;
            }
            if (!ns_breadboard_hit_hole((const NsBreadboard *)(const void *)be, tx, ty, &h)) {
                continue;
            }
            ns_breadboard_hole_world((const NsBreadboard *)(const void *)be, h, &hx, &hy);
            if (hx == tx && hy == ty) {
                count++;
                break;
            }
        }
    }
    return count;
}

static int snap_chip_to_breadboard(R01aUi *ui, int chip_i) {
    NsEntity *e;
    int n;
    int bi;
    int best_count = 0;
    int best_dx = 0;
    int best_dy = 0;
    int pin_hi;

    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return 0;
    }
    e = ui->chips[chip_i];
    pin_hi = entity_pin_hi(e);
    if (!e || pin_hi < 1) {
        return 0;
    }
    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        if (!entity_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        for (bi = 0; bi < ui->chip_count; bi++) {
            NsEntity *be = ui->chips[bi];
            NsPbHole h;
            int hx;
            int hy;
            int dx;
            int dy;
            int count;
            if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD) {
                continue;
            }
            if (!ns_breadboard_hit_hole((const NsBreadboard *)(const void *)be, tx, ty, &h)) {
                continue;
            }
            ns_breadboard_hole_world((const NsBreadboard *)(const void *)be, h, &hx, &hy);
            dx = hx - tx;
            dy = hy - ty;
            count = count_pins_on_holes(ui, e, dx, dy);
            if (count > best_count) {
                best_count = count;
                best_dx = dx;
                best_dy = dy;
            }
        }
    }
    if (best_count < 1) {
        return 0;
    }
    move_entity(e, e->board_x + best_dx, e->board_y + best_dy);
    clamp_chip(e);
    return best_count;
}

static void snap_selection_to_breadboard(R01aUi *ui) {
    int i;
    if (sel_count(ui) > 0) {
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chip_sel[i]) {
                snap_chip_to_breadboard(ui, i);
            }
        }
        return;
    }
    if (ui->drag_chip >= 0) {
        snap_chip_to_breadboard(ui, ui->drag_chip);
    }
}

static int hit_chip_body(const R01aUi *ui, const NsEntity *e, int lx, int ly) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int pad = (e->visual == NS_ENTITY_VIS_BREADBOARD) ? 0 : NS_CHIP_PIN_OUT;
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
        if (!e) {
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_IC) {
            ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
            clamp_chip(e);
            snap_chip_to_breadboard(ui, i);
            n++;
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_BREADBOARD) {
            ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
            ns_breadboard_sync_body((NsBreadboard *)e);
            clamp_chip(e);
            n++;
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_OSC) {
            ns_osc4legs_set_orient(e, ns_orient_next_cw(e->orient));
            clamp_chip(e);
            snap_chip_to_breadboard(ui, i);
            n++;
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_PASSIVE) {
            ns_passive_set_orient((NsPassive *)e, ns_orient_next_cw(e->orient));
            clamp_chip(e);
            snap_chip_to_breadboard(ui, i);
            n++;
        }
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
        if (ui->pin_gray) {
            tint[0] = 120;
            tint[1] = 125;
            tint[2] = 110;
        } else {
            pin_level_rgb(e->pins[i].level, e->pins[i].dir, &tint[0], &tint[1], &tint[2]);
        }
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

static void draw_osc(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected) {
    int p1x;
    int p1y;
    if (!ns_osc4legs_chip_tip(e, 14, &p1x, &p1y)) {
        return;
    }
    ns_passive_draw_kind(r, NS_PASSIVE_OSC4LEGS, e->orient, board_sx(ui, p1x), board_sy(ui, p1y),
                        selected);
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

typedef struct R01aPinNetSlot {
    NsEntity *entity;
    int pin_index;
} R01aPinNetSlot;

static R01aPinNetSlot g_pin_slots[R01A_PIN_NET_SLOTS];
static int g_pin_parent[R01A_PIN_NET_SLOTS];
static int g_pin_slot_count;

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

static NsEntity *ui_ent(const R01aUi *ui, const char *ref) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] && ui->chips[i]->refdes && strcmp(ui->chips[i]->refdes, ref) == 0) {
            return ui->chips[i];
        }
    }
    return NULL;
}

static void pin_net_build(R01aUi *ui) {
    NsEntity *y2 = ui_ent(ui, "Y2");
    NsEntity *y3 = ui_ent(ui, "Y3");
    NsEntity *bx = ui_ent(ui, "UPLDX");
    NsEntity *by = ui_ent(ui, "UPLDY");
    NsEntity *u24 = ui_ent(ui, "U24");
    NsEntity *enc = ui_ent(ui, "UENC");
    int i;
    char iname[8];
    char aname[4];

    g_pin_slot_count = 0;
    pin_net_link(y2, "VDD", y2, "OE#");
    pin_net_link(y2, "VDD", y3, "VDD");
    pin_net_link(y2, "VDD", y3, "OE#");
    pin_net_link(y2, "VDD", bx, "VCC");
    pin_net_link(y2, "VDD", by, "VCC");
    pin_net_link(y2, "VDD", u24, "VCC");
    pin_net_link(y2, "VDD", u24, "VPP");
    pin_net_link(y2, "VDD", enc, "APOS");
    pin_net_link(y2, "VDD", enc, "DPOS");
    pin_net_link(y2, "VDD", bx, "RES#");
    pin_net_link(y2, "VDD", by, "RES#");
    pin_net_link(y2, "VDD", u24, "PGM#");
    pin_net_link(y2, "VDD", enc, "ENCD");
    pin_net_link(y2, "VDD", enc, "STND");
    pin_net_link(y2, "VDD", enc, "VSYNC");
    pin_net_link(y2, "GND", y3, "GND");
    pin_net_link(y2, "GND", bx, "GND");
    pin_net_link(y2, "GND", by, "GND");
    pin_net_link(y2, "GND", u24, "GND");
    pin_net_link(y2, "GND", enc, "AGND");
    pin_net_link(y2, "GND", enc, "DGND");
    pin_net_link(y2, "GND", enc, "SELECT");
    pin_net_link(y2, "GND", u24, "CE#");
    pin_net_link(y2, "GND", u24, "OE#");
    pin_net_link(y2, "DOT", ui_ent(ui, "R12"), "1");
    pin_net_link(ui_ent(ui, "R12"), "2", bx, "CLK");
    pin_net_link(y3, "FSC", ui_ent(ui, "R13"), "1");
    pin_net_link(ui_ent(ui, "R13"), "2", enc, "FIN");
    pin_net_link(bx, "HWRAP", by, "CLK");
    pin_net_link(bx, "CSYNC", enc, "HSYNC");
    for (i = 0; i < 6; i++) {
        snprintf(iname, sizeof(iname), "INDEX%d", i);
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(bx, iname, u24, aname);
    }
    for (i = 6; i <= 13; i++) {
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(y2, "GND", u24, aname);
    }
    pin_net_link(u24, "O7", ui_ent(ui, "R1"), "1");
    pin_net_link(ui_ent(ui, "R1"), "2", enc, "RIN");
    pin_net_link(u24, "O6", ui_ent(ui, "R2"), "1");
    pin_net_link(ui_ent(ui, "R2"), "2", enc, "RIN");
    pin_net_link(u24, "O5", ui_ent(ui, "R3"), "1");
    pin_net_link(ui_ent(ui, "R3"), "2", enc, "RIN");
    pin_net_link(u24, "O4", ui_ent(ui, "R4"), "1");
    pin_net_link(ui_ent(ui, "R4"), "2", enc, "GIN");
    pin_net_link(u24, "O3", ui_ent(ui, "R5"), "1");
    pin_net_link(ui_ent(ui, "R5"), "2", enc, "GIN");
    pin_net_link(u24, "O2", ui_ent(ui, "R6"), "1");
    pin_net_link(ui_ent(ui, "R6"), "2", enc, "GIN");
    pin_net_link(u24, "O1", ui_ent(ui, "R7"), "1");
    pin_net_link(ui_ent(ui, "R7"), "2", enc, "BIN");
    pin_net_link(u24, "O0", ui_ent(ui, "R8"), "1");
    pin_net_link(ui_ent(ui, "R8"), "2", enc, "BIN");
    pin_net_link(enc, "RIN", ui_ent(ui, "R9"), "1");
    pin_net_link(ui_ent(ui, "R9"), "2", y2, "GND");
    pin_net_link(enc, "GIN", ui_ent(ui, "R10"), "1");
    pin_net_link(ui_ent(ui, "R10"), "2", y2, "GND");
    pin_net_link(enc, "BIN", ui_ent(ui, "R11"), "1");
    pin_net_link(ui_ent(ui, "R11"), "2", y2, "GND");
}

static int hit_pin_at(const R01aUi *ui, int board_mx, int board_my, int *chip_out, int *pin_out) {
    int rank;
    int best_d = R01A_PIN_HIT_PX * R01A_PIN_HIT_PX + 1;
    int best_c = -1;
    int best_p = -1;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        int i;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (i = 0; i < e->pin_count; i++) {
            int tx;
            int ty;
            int dx;
            int dy;
            int d;
            if (!entity_tip_board(e, e->pins[i].number, &tx, &ty)) {
                continue;
            }
            dx = board_mx - tx;
            dy = board_my - ty;
            d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                best_c = ci;
                best_p = i;
            }
        }
        if (best_c == ci) {
            break;
        }
    }
    if (best_c < 0) {
        return 0;
    }
    if (chip_out) {
        *chip_out = best_c;
    }
    if (pin_out) {
        *pin_out = best_p;
    }
    return 1;
}

static int pin_on_hover_net(const R01aUi *ui, const NsEntity *e, int pin_index) {
    int src;
    int slot;
    int root;
    if (ui->hover_chip < 0 || ui->hover_pin < 0 || !e) {
        return 0;
    }
    src = pin_net_find(ui->chips[ui->hover_chip], ui->hover_pin);
    slot = pin_net_find((NsEntity *)e, pin_index);
    if (src < 0 || slot < 0) {
        return src < 0 ? 0 : (e == ui->chips[ui->hover_chip] && pin_index == ui->hover_pin);
    }
    root = pin_net_root(src);
    return pin_net_root(slot) == root;
}

static int pin_name_is_gnd(const char *name) {
    return name && (strcmp(name, "GND") == 0 || strcmp(name, "AGND") == 0 || strcmp(name, "DGND") == 0);
}

static int pin_name_is_vdd(const char *name) {
    return name && (strcmp(name, "VDD") == 0 || strcmp(name, "VCC") == 0 || strcmp(name, "APOS") == 0 ||
                    strcmp(name, "DPOS") == 0);
}

static int pin_on_hub_net(const R01aUi *ui, const NsEntity *e, int pin_index, const char *rail) {
    NsEntity *hub;
    const NsPin *rp;
    int rslot;
    int slot;
    if (!e || pin_index < 0 || pin_index >= e->pin_count || !rail) {
        return 0;
    }
    hub = ui_ent(ui, "Y2");
    if (!hub) {
        return 0;
    }
    rp = ns_entity_pin_named_const(hub, rail);
    if (!rp) {
        return 0;
    }
    rslot = pin_net_find(hub, (int)(rp - hub->pins));
    slot = pin_net_find((NsEntity *)e, pin_index);
    if (rslot < 0 || slot < 0) {
        return 0;
    }
    return pin_net_root(rslot) == pin_net_root(slot);
}

static int pin_on_gnd_net(const R01aUi *ui, const NsEntity *e, int pin_index) {
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (pin_name_is_gnd(e->pins[pin_index].name)) {
        return 1;
    }
    return pin_on_hub_net(ui, e, pin_index, "GND");
}

static int pin_on_vdd_net(const R01aUi *ui, const NsEntity *e, int pin_index) {
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (pin_name_is_vdd(e->pins[pin_index].name)) {
        return 1;
    }
    return pin_on_hub_net(ui, e, pin_index, "VDD");
}

static int hole_is_neg_rail(NsPbHole h) {
    return h.lane == NS_PB_LANE_TOP_NEG;
}

static int hole_is_pos_rail(NsPbHole h) {
    return h.lane == NS_PB_LANE_TOP_POS;
}

static int bb_is_powered(const NsBreadboard *bb) {
    return bb && bb->base.refdes && strcmp(bb->base.refdes, "BB1") == 0;
}

static const NsBreadboard *ui_powered_bb(const R01aUi *ui) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (bb_is_powered((const NsBreadboard *)e)) {
            return (const NsBreadboard *)e;
        }
    }
    return NULL;
}

static int ui_hover_rail(const R01aUi *ui, int pos, int *wx, int *wy, const NsBreadboard **bb_out,
                         NsPbHole *h_out) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        const NsBreadboard *bb;
        NsEntity *e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        bb = (const NsBreadboard *)e;
        if (!bb->hover_valid || !bb_is_powered(bb)) {
            continue;
        }
        if (pos) {
            if (!hole_is_pos_rail(bb->hover)) {
                continue;
            }
        } else if (!hole_is_neg_rail(bb->hover)) {
            continue;
        }
        if (wx && wy) {
            ns_breadboard_hole_world(bb, bb->hover, wx, wy);
        }
        if (bb_out) {
            *bb_out = bb;
        }
        if (h_out) {
            *h_out = bb->hover;
        }
        return 1;
    }
    return 0;
}

static int ui_hover_rail_xy(const R01aUi *ui, int pos, int *wx, int *wy) {
    return ui_hover_rail(ui, pos, wx, wy, NULL, NULL);
}

static int rail_right_col(int lane) {
    int col;
    for (col = NS_PB_COLS - 1; col >= 0; col--) {
        NsPbHole h = {col, lane};
        if (ns_breadboard_hole_exists(h)) {
            return col;
        }
    }
    return 0;
}

static int ui_ground_rail_tip(const R01aUi *ui, int ax, int ay, int *gx, int *gy) {
    const NsBreadboard *bb = ui_powered_bb(ui);
    int best_d = -1;
    int found = 0;
    int edge;
    if (!bb) {
        return 0;
    }
    for (edge = 0; edge < 2; edge++) {
        NsPbHole h;
        int hx;
        int hy;
        int dx;
        int dy;
        int d;
        h.lane = NS_PB_LANE_TOP_NEG;
        h.col = edge ? rail_right_col(h.lane) : 0;
        if (!ns_breadboard_hole_exists(h)) {
            continue;
        }
        ns_breadboard_hole_world(bb, h, &hx, &hy);
        dx = hx - ax;
        dy = hy - ay;
        d = dx * dx + dy * dy;
        if (!found || d < best_d) {
            best_d = d;
            found = 1;
            if (gx) {
                *gx = hx;
            }
            if (gy) {
                *gy = hy;
            }
        }
    }
    return found;
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
        jog = R01A_HOVER_JOG;
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

static int manhattan_h_first(int ax, int ay, int bx, int by) {
    int dx = bx - ax;
    int dy = by - ay;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    return dx >= dy;
}

static void draw_2elbow(SDL_Renderer *r, int ax, int ay, int bx, int by, int h_first, int jog) {
    int x[4];
    int y[4];
    int i;
    elbow_pts(ax, ay, bx, by, h_first, jog, x, y);
    for (i = 0; i < 3; i++) {
        if (x[i] != x[i + 1] || y[i] != y[i + 1]) {
            SDL_RenderDrawLine(r, x[i], y[i], x[i + 1], y[i + 1]);
        }
    }
}

static int phys_strip_find(int *parent, int s) {
    if (!parent || s < 0 || s >= NS_PB_STRIPS) {
        return s;
    }
    while (parent[s] != s) {
        parent[s] = parent[parent[s]];
        s = parent[s];
    }
    return s;
}

static void phys_strip_union(int *parent, int a, int b) {
    int ra;
    int rb;
    if (!parent) {
        return;
    }
    ra = phys_strip_find(parent, a);
    rb = phys_strip_find(parent, b);
    if (ra != rb && ra >= 0 && ra < NS_PB_STRIPS) {
        parent[rb] = ra;
    }
}

static int jumper_matches_bb(const R01aJumper *j, const NsBreadboard *bb) {
    const char *ref;
    if (!j || !bb || !bb->base.refdes) {
        return 0;
    }
    ref = j->bb_ref[0] ? j->bb_ref : "BB1";
    return strcmp(ref, bb->base.refdes) == 0;
}

static void phys_nets_on_bb(int *parent, const R01aBoard *board, const NsBreadboard *bb) {
    int s;
    int i;
    for (s = 0; s < NS_PB_STRIPS; s++) {
        parent[s] = s;
    }
    if (!board || !bb) {
        return;
    }
    for (i = 0; i < board->jumper_count; i++) {
        if (!jumper_matches_bb(&board->jumpers[i], bb)) {
            continue;
        }
        phys_strip_union(parent, ns_breadboard_strip_id(board->jumpers[i].a),
                         ns_breadboard_strip_id(board->jumpers[i].b));
    }
}

static int pin_bb_strip(const R01aUi *ui, const NsEntity *e, int pin_index, const NsBreadboard **bb_out,
                        int *strip_out) {
    int tx;
    int ty;
    int i;
    if (!ui || !e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (!entity_tip_board(e, e->pins[pin_index].number, &tx, &ty)) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *be = ui->chips[i];
        int strip;
        if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (!ns_breadboard_tip_strip((const NsBreadboard *)be, tx, ty, &strip)) {
            continue;
        }
        if (bb_out) {
            *bb_out = (const NsBreadboard *)be;
        }
        if (strip_out) {
            *strip_out = strip;
        }
        return 1;
    }
    return 0;
}

static int pins_phys_connected(const R01aUi *ui, const R01aBoard *board, const NsEntity *a, int ap,
                              const NsEntity *b, int bp) {
    const NsBreadboard *bba;
    const NsBreadboard *bbb;
    int sa;
    int sb;
    int parent[NS_PB_STRIPS];
    if (!pin_bb_strip(ui, a, ap, &bba, &sa) || !pin_bb_strip(ui, b, bp, &bbb, &sb)) {
        return 0;
    }
    if (bba != bbb) {
        return 0;
    }
    phys_nets_on_bb(parent, board, bba);
    return phys_strip_find(parent, sa) == phys_strip_find(parent, sb);
}

static int pin_phys_on_hole_net(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index,
                               const NsBreadboard *bb, NsPbHole h) {
    const NsBreadboard *pbb;
    int ps;
    int parent[NS_PB_STRIPS];
    if (!bb || !pin_bb_strip(ui, e, pin_index, &pbb, &ps) || pbb != bb) {
        return 0;
    }
    phys_nets_on_bb(parent, board, bb);
    return phys_strip_find(parent, ps) == phys_strip_find(parent, ns_breadboard_strip_id(h));
}

static int pin_phys_on_powered_rail(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index,
                                   int pos) {
    const NsBreadboard *pbb;
    int ps;
    int parent[NS_PB_STRIPS];
    int k;
    int ids[4];
    int base = NS_PB_COLS * 2;
    if (!pin_bb_strip(ui, e, pin_index, &pbb, &ps) || !bb_is_powered(pbb)) {
        return 0;
    }
    phys_nets_on_bb(parent, board, pbb);
    if (pos) {
        ids[0] = base + 0;
        ids[1] = base + 2;
        ids[2] = base + 4;
        ids[3] = base + 6;
    } else {
        ids[0] = base + 1;
        ids[1] = base + 3;
        ids[2] = base + 5;
        ids[3] = base + 7;
    }
    ps = phys_strip_find(parent, ps);
    for (k = 0; k < 4; k++) {
        if (ps == phys_strip_find(parent, ids[k])) {
            return 1;
        }
    }
    return 0;
}

static int hover_skip_air(const R01aBoard *board) {
    return board && r01a_board_wire_mode(board) == R01A_WIRE_MANUAL;
}

static int hover_skip_pin_pair(const R01aUi *ui, const R01aBoard *board, const NsEntity *a, int ap,
                               const NsEntity *b, int bp) {
    return hover_skip_air(board) && pins_phys_connected(ui, board, a, ap, b, bp);
}

static int hover_skip_rail_pin(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index,
                              int pos, const NsBreadboard *hover_bb, NsPbHole hover_h) {
    if (!hover_skip_air(board)) {
        return 0;
    }
    if (pin_phys_on_hole_net(ui, board, e, pin_index, hover_bb, hover_h)) {
        return 1;
    }
    return pin_phys_on_powered_rail(ui, board, e, pin_index, pos);
}

static void draw_air_line(SDL_Renderer *r, const R01aUi *ui, int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) {
        return;
    }
    SDL_RenderDrawLine(r, board_sx(ui, ax), board_sy(ui, ay), board_sx(ui, bx), board_sy(ui, by));
}

static void draw_rail_net_from(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, int ax, int ay,
                              int vdd, const NsBreadboard *hover_bb, NsPbHole hover_h) {
    int i;
    int pi;
    SDL_SetRenderDrawColor(r, 255, 230, 80, 255);
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int bx;
            int by;
            if (vdd) {
                if (!pin_on_vdd_net(ui, e, pi)) {
                    continue;
                }
            } else if (!pin_on_gnd_net(ui, e, pi)) {
                continue;
            }
            if (hover_skip_rail_pin(ui, board, e, pi, vdd, hover_bb, hover_h)) {
                continue;
            }
            if (!entity_tip_board(e, e->pins[pi].number, &bx, &by)) {
                continue;
            }
            draw_air_line(r, ui, ax, ay, bx, by);
        }
    }
}

static void draw_hover_ground(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, const NsEntity *src_e,
                             int src_pin, int ax, int ay) {
    int gx = 0;
    int gy = 0;
    if (hover_skip_air(board) && pin_phys_on_powered_rail(ui, board, src_e, src_pin, 0)) {
        return;
    }
    if (!ui_ground_rail_tip(ui, ax, ay, &gx, &gy)) {
        return;
    }
    SDL_SetRenderDrawColor(r, 255, 230, 80, 255);
    draw_air_line(r, ui, ax, ay, gx, gy);
}

static void draw_hover_wires(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board) {
    const NsEntity *src_e;
    int ax;
    int ay;
    int i;
    int pi;
    int saw_gnd = 0;
    int src_gnd;

    if (ui->hover_chip < 0 || ui->hover_pin < 0) {
        int rax;
        int ray;
        const NsBreadboard *hbb = NULL;
        NsPbHole hh = {0, 0};
        if (ui_hover_rail(ui, 0, &rax, &ray, &hbb, &hh)) {
            draw_rail_net_from(r, ui, board, rax, ray, 0, hbb, hh);
        } else if (ui_hover_rail(ui, 1, &rax, &ray, &hbb, &hh)) {
            draw_rail_net_from(r, ui, board, rax, ray, 1, hbb, hh);
        }
        return;
    }
    src_e = ui->chips[ui->hover_chip];
    if (!src_e || ui->hover_pin >= src_e->pin_count) {
        return;
    }
    if (!entity_tip_board(src_e, src_e->pins[ui->hover_pin].number, &ax, &ay)) {
        return;
    }
    src_gnd = pin_on_gnd_net(ui, src_e, ui->hover_pin);
    SDL_SetRenderDrawColor(r, 255, 230, 80, 255);
    if (src_gnd) {
        draw_hover_ground(r, ui, board, src_e, ui->hover_pin, ax, ay);
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e == src_e) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int bx;
            int by;
            if (!pin_on_hover_net(ui, e, pi)) {
                continue;
            }
            if (pin_name_is_gnd(e->pins[pi].name) || pin_on_gnd_net(ui, e, pi)) {
                saw_gnd = 1;
                continue;
            }
            if (hover_skip_pin_pair(ui, board, src_e, ui->hover_pin, e, pi)) {
                continue;
            }
            if (!entity_tip_board(e, e->pins[pi].number, &bx, &by)) {
                continue;
            }
            draw_air_line(r, ui, ax, ay, bx, by);
        }
    }
    if (saw_gnd) {
        draw_hover_ground(r, ui, board, src_e, ui->hover_pin, ax, ay);
    }
}

static void draw_entity(SDL_Renderer *r, R01aUi *ui, NsEntity *e, int selected) {
    if (!e || chip_hidden(e)) {
        return;
    }
    switch (e->visual) {
    case NS_ENTITY_VIS_BREADBOARD:
        ns_breadboard_draw(r, (const NsBreadboard *)e, board_sx(ui, e->board_x), board_sy(ui, e->board_y),
                           selected);
        break;
    case NS_ENTITY_VIS_OSC:
        draw_osc(r, ui, e, selected);
        break;
    case NS_ENTITY_VIS_DISPLAY:
        draw_lcd(r, ui, (NsVideoSink *)e, selected);
        break;
    case NS_ENTITY_VIS_PASSIVE: {
        const NsPassive *p = (const NsPassive *)e;
        ns_passive_draw(r, p, board_sx(ui, p->pivot_x), board_sy(ui, p->pivot_y), selected);
        break;
    }
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
    if (ui->hover_jumper >= 0) {
        snprintf(out, out_len, "jumper");
        return;
    }
    if (ui_hover_rail_xy(ui, 0, NULL, NULL)) {
        snprintf(out, out_len, "GND rail");
        return;
    }
    if (ui_hover_rail_xy(ui, 1, NULL, NULL)) {
        snprintf(out, out_len, "VDD rail");
        return;
    }
    chip_i = hit_top_chip(ui, ui->mouse_lx, ui->mouse_ly);
    if (chip_i < 0) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e) {
        return;
    }
    if (ui->hover_pin >= 0 && ui->hover_chip == chip_i && ui->hover_pin < e->pin_count &&
        e->pins[ui->hover_pin].name) {
        snprintf(out, out_len, "%s %s", e->refdes ? e->refdes : "?", e->pins[ui->hover_pin].name);
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        const NsPassive *p = (const NsPassive *)e;
        snprintf(out, out_len, "%s %s", e->refdes ? e->refdes : "?", p->value[0] ? p->value : e->part);
        return;
    }
    snprintf(out, out_len, "%s (%s)", e->refdes ? e->refdes : "?", e->part ? e->part : "?");
}

static const char *wire_mode_label(int mode) {
    return (mode == R01A_WIRE_MANUAL) ? "Manual" : "Auto";
}

static void wire_mode_btn_rect(int mode, SDL_Rect *rc) {
    int tw = r01a_font_text_width(wire_mode_label(mode));
    rc->x = 4;
    rc->y = 2;
    rc->w = tw + 12;
    rc->h = r01a_font_line_h() + 4;
}

static int point_in_rect(int x, int y, const SDL_Rect *rc) {
    return x >= rc->x && y >= rc->y && x < rc->x + rc->w && y < rc->y + rc->h;
}

enum {
    R01A_CTX_R = 0,
    R01A_CTX_CCAP,
    R01A_CTX_ECAP,
    R01A_CTX_OSC,
    R01A_CTX_D,
    R01A_CTX_BB,
    R01A_CTX_COUNT
};

static const char *k_ctx_label[R01A_CTX_COUNT] = {
    "Add resistor", "Add ceramic cap", "Add electrolytic", "Add oscillator", "Add diode", "Add breadboard"};

static const char *k_ctx_value[R01A_CTX_COUNT] = {"33", "100nF", "220uF", "8.000MHz", "", ""};

static int ctx_item_h(void) {
    return r01a_font_line_h() + 4;
}

static int ctx_menu_w(void) {
    int i;
    int w = 0;
    for (i = 0; i < R01A_CTX_COUNT; i++) {
        int tw = r01a_font_text_width(k_ctx_label[i]);
        if (tw > w) {
            w = tw;
        }
    }
    return w + 16;
}

static void ctx_menu_geom(const R01aUi *ui, int *x, int *y, int *w, int *h) {
    int mw = ctx_menu_w();
    int mh = ctx_item_h() * R01A_CTX_COUNT;
    int mx = ui->ctx_lx;
    int my = ui->ctx_ly;
    if (mx + mw > NS_LOGIC_W - 4) {
        mx = NS_LOGIC_W - 4 - mw;
    }
    if (my + mh > NS_LOGIC_H - 4) {
        my = NS_LOGIC_H - 4 - mh;
    }
    if (mx < 4) {
        mx = 4;
    }
    if (my < 4) {
        my = 4;
    }
    if (x) {
        *x = mx;
    }
    if (y) {
        *y = my;
    }
    if (w) {
        *w = mw;
    }
    if (h) {
        *h = mh;
    }
}

static int ctx_hit_item(const R01aUi *ui, int lx, int ly) {
    int x;
    int y;
    int w;
    int h;
    int item;
    if (!ui->ctx_open) {
        return -1;
    }
    ctx_menu_geom(ui, &x, &y, &w, &h);
    if (lx < x || ly < y || lx >= x + w || ly >= y + h) {
        return -1;
    }
    item = (ly - y) / ctx_item_h();
    if (item < 0 || item >= R01A_CTX_COUNT) {
        return -1;
    }
    return item;
}

static void draw_ctx_menu(SDL_Renderer *r, const R01aUi *ui) {
    int x;
    int y;
    int w;
    int h;
    int i;
    int ih;
    if (!ui->ctx_open) {
        return;
    }
    ctx_menu_geom(ui, &x, &y, &w, &h);
    ih = ctx_item_h();
    fill_rect(r, x, y, w, h, 28, 32, 28);
    draw_rect(r, x, y, w, h, 180, 180, 120);
    for (i = 0; i < R01A_CTX_COUNT; i++) {
        int iy = y + i * ih;
        if (ui->mouse_lx >= x && ui->mouse_lx < x + w && ui->mouse_ly >= iy && ui->mouse_ly < iy + ih) {
            fill_rect(r, x + 1, iy, w - 2, ih, 50, 70, 50);
        }
        r01a_font_draw(r, x + 8, iy + 2, k_ctx_label[i], 230, 230, 200);
    }
}

static NsBreadboard *ui_bb_named(const R01aUi *ui, const char *ref) {
    int i;
    if (!ref || !ref[0]) {
        ref = "BB1";
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (e && e->visual == NS_ENTITY_VIS_BREADBOARD && e->refdes && strcmp(e->refdes, ref) == 0) {
            return (NsBreadboard *)e;
        }
    }
    return ui_breadboard(ui);
}

static NsBreadboard *hit_breadboard(const R01aUi *ui, int mx, int my, NsPbHole *hole) {
    int rank;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        NsPbHole h;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (ns_breadboard_hit_hole((const NsBreadboard *)e, mx, my, &h)) {
            if (hole) {
                *hole = h;
            }
            return (NsBreadboard *)e;
        }
    }
    return NULL;
}

static void ctx_apply(R01aUi *ui, R01aBoard *board, int item) {
    sel_clear(ui);
    if (item == R01A_CTX_BB) {
        if (r01a_board_add_breadboard(board, ui->ctx_bx, ui->ctx_by)) {
            bind_chips(ui, board);
        }
        return;
    }
    {
        NsPassiveKind kind = NS_PASSIVE_R;
        if (item == R01A_CTX_CCAP) {
            kind = NS_PASSIVE_CCAP;
        } else if (item == R01A_CTX_ECAP) {
            kind = NS_PASSIVE_ECAP;
        } else if (item == R01A_CTX_OSC) {
            kind = NS_PASSIVE_OSC4LEGS;
        } else if (item == R01A_CTX_D) {
            kind = NS_PASSIVE_D;
        }
        if (r01a_board_add_passive(board, kind, k_ctx_value[item], ui->ctx_bx, ui->ctx_by)) {
            bind_chips(ui, board);
        }
    }
}

static int dist2(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    return dx * dx + dy * dy;
}

static int dist2_seg(int px, int py, int x0, int y0, int x1, int y1) {
    int vx = x1 - x0;
    int vy = y1 - y0;
    int wx = px - x0;
    int wy = py - y0;
    int c1 = vx * wx + vy * wy;
    int c2 = vx * vx + vy * vy;
    if (c2 <= 0) {
        return wx * wx + wy * wy;
    }
    if (c1 <= 0) {
        return wx * wx + wy * wy;
    }
    if (c1 >= c2) {
        return dist2(px, py, x1, y1);
    }
    {
        int t_num = c1;
        int cx = x0 + (int)(((long)vx * t_num) / c2);
        int cy = y0 + (int)(((long)vy * t_num) / c2);
        return dist2(px, py, cx, cy);
    }
}

static int jumper_world_ends(const R01aUi *ui, const R01aJumper *j, int *x0, int *y0, int *x1, int *y1) {
    const NsBreadboard *bb;
    if (!j) {
        return 0;
    }
    bb = ui_bb_named(ui, j->bb_ref);
    if (!bb) {
        return 0;
    }
    ns_breadboard_hole_world(bb, j->a, x0, y0);
    ns_breadboard_hole_world(bb, j->b, x1, y1);
    return 1;
}

static void jumper_rgb_draw(const R01aJumper *j, int bright, Uint8 *r, Uint8 *g, Uint8 *b) {
    Uint8 cr = j ? j->r : 220;
    Uint8 cg = j ? j->g : 160;
    Uint8 cb = j ? j->bcol : 40;
    if (bright) {
        cr = (Uint8)(cr + (255 - cr) / 2);
        cg = (Uint8)(cg + (255 - cg) / 2);
        cb = (Uint8)(cb + (255 - cb) / 2);
    }
    if (r) {
        *r = cr;
    }
    if (g) {
        *g = cg;
    }
    if (b) {
        *b = cb;
    }
}

static int dist2_jumper_path(int px, int py, int ax, int ay, int bx, int by) {
    int x[4];
    int y[4];
    int i;
    int best;
    elbow_pts(ax, ay, bx, by, manhattan_h_first(ax, ay, bx, by), R01A_HOVER_JOG, x, y);
    best = dist2_seg(px, py, x[0], y[0], x[1], y[1]);
    for (i = 1; i < 3; i++) {
        int d = dist2_seg(px, py, x[i], y[i], x[i + 1], y[i + 1]);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

static void draw_end_handle(SDL_Renderer *r, int x, int y, Uint8 R, Uint8 G, Uint8 B) {
    fill_rect(r, x - 2, y - 2, 5, 5, R, G, B);
}

static int hit_jumper_end(const R01aUi *ui, const R01aBoard *board, int bx, int by, int *j_out, int *end_out) {
    int i;
    int best = R01A_JUMPER_END_PX * R01A_JUMPER_END_PX;
    int found = 0;
    for (i = 0; i < board->jumper_count; i++) {
        int x0;
        int y0;
        int x1;
        int y1;
        int d;
        if (!jumper_world_ends(ui, &board->jumpers[i], &x0, &y0, &x1, &y1)) {
            continue;
        }
        d = dist2(bx, by, x0, y0);
        if (d <= best) {
            best = d;
            found = 1;
            if (j_out) {
                *j_out = i;
            }
            if (end_out) {
                *end_out = 0;
            }
        }
        d = dist2(bx, by, x1, y1);
        if (d <= best) {
            best = d;
            found = 1;
            if (j_out) {
                *j_out = i;
            }
            if (end_out) {
                *end_out = 1;
            }
        }
    }
    return found;
}

static int hit_jumper_body(const R01aUi *ui, const R01aBoard *board, int bx, int by) {
    int i;
    int best = R01A_JUMPER_HIT_PX * R01A_JUMPER_HIT_PX;
    int found = -1;
    for (i = 0; i < board->jumper_count; i++) {
        int x0;
        int y0;
        int x1;
        int y1;
        int d;
        if (!jumper_world_ends(ui, &board->jumpers[i], &x0, &y0, &x1, &y1)) {
            continue;
        }
        d = dist2_jumper_path(bx, by, x0, y0, x1, y1);
        if (d <= best) {
            best = d;
            found = i;
        }
    }
    return found;
}

static void jumper_sel_set_one(R01aUi *ui, int ji) {
    sel_clear(ui);
    if (ji < 0 || ji >= R01A_JUMPER_MAX) {
        return;
    }
    ui->jumper_sel[ji] = 1;
}

static int jumper_sel_any(const R01aUi *ui) {
    int i;
    for (i = 0; i < R01A_JUMPER_MAX; i++) {
        if (ui->jumper_sel[i]) {
            return 1;
        }
    }
    return 0;
}

static int jumper_palette_i(const R01aJumper *j) {
    int i;
    if (!j) {
        return 0;
    }
    for (i = 0; i < R01A_JUMPER_COLORS; i++) {
        if (j->r == k_jumper_rgb[i][0] && j->g == k_jumper_rgb[i][1] && j->bcol == k_jumper_rgb[i][2]) {
            return i;
        }
    }
    return 0;
}

static void jumper_cycle_selected(R01aUi *ui, R01aBoard *board, int dir) {
    int i;
    int last = ui->jumper_color_i;
    if (!dir) {
        return;
    }
    for (i = 0; i < board->jumper_count; i++) {
        int ci;
        const Uint8 *rgb;
        if (!ui->jumper_sel[i]) {
            continue;
        }
        ci = jumper_palette_i(&board->jumpers[i]);
        if (dir > 0) {
            ci = (ci + 1) % R01A_JUMPER_COLORS;
        } else {
            ci = (ci + R01A_JUMPER_COLORS - 1) % R01A_JUMPER_COLORS;
        }
        rgb = k_jumper_rgb[ci];
        board->jumpers[i].r = rgb[0];
        board->jumpers[i].g = rgb[1];
        board->jumpers[i].bcol = rgb[2];
        last = ci;
    }
    ui->jumper_color_i = last;
}

static void jumper_delete_selected(R01aUi *ui, R01aBoard *board) {
    int i;
    for (i = board->jumper_count - 1; i >= 0; i--) {
        if (!ui->jumper_sel[i]) {
            continue;
        }
        r01a_board_jumper_remove(board, i);
        {
            int k;
            for (k = i; k < board->jumper_count; k++) {
                ui->jumper_sel[k] = ui->jumper_sel[k + 1];
            }
            ui->jumper_sel[board->jumper_count] = 0;
        }
    }
    ui->hover_jumper = -1;
    ui->drag_jumper = -1;
    ui->drag_jumper_preview_ok = 0;
}

static void breadboard_delete_selected(R01aUi *ui, R01aBoard *board) {
    char refs[R01A_BB_EXTRA_MAX + 1][R01A_BB_REF_LEN];
    int nref = 0;
    int i;
    int removed = 0;
    for (i = 0; i < ui->chip_count; i++) {
        const NsEntity *e;
        if (!ui->chip_sel[i]) {
            continue;
        }
        e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD || !e->refdes) {
            continue;
        }
        if (nref >= R01A_BB_EXTRA_MAX + 1) {
            break;
        }
        snprintf(refs[nref], sizeof(refs[nref]), "%s", e->refdes);
        nref++;
    }
    for (i = 0; i < nref; i++) {
        NsEntity *e = r01a_board_entity_by_refdes(board, refs[i]);
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (ui->jumper_bb == (NsBreadboard *)e) {
            ui->jumper_arm = 0;
            ui->jumper_bb = NULL;
        }
        if (r01a_board_remove_breadboard(board, (NsBreadboard *)e)) {
            removed = 1;
        }
    }
    if (removed) {
        sel_clear(ui);
        bind_chips(ui, board);
        pin_net_build(ui);
    }
}

static void draw_jumpers(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board) {
    int i;
    for (i = 0; i < board->jumper_count; i++) {
        int x0;
        int y0;
        int x1;
        int y1;
        int sx0;
        int sy0;
        int sx1;
        int sy1;
        Uint8 cr;
        Uint8 cg;
        Uint8 cb;
        int sel = ui->jumper_sel[i];
        int hover = (ui->hover_jumper == i);
        int bright = sel || hover;
        if (!jumper_world_ends(ui, &board->jumpers[i], &x0, &y0, &x1, &y1)) {
            continue;
        }
        if (ui->drag_jumper == i && ui->drag_jumper_preview_ok) {
            int hx;
            int hy;
            const NsBreadboard *bb = ui_bb_named(ui, board->jumpers[i].bb_ref);
            if (bb) {
                ns_breadboard_hole_world(bb, ui->drag_jumper_preview, &hx, &hy);
                if (ui->drag_jumper_end) {
                    x1 = hx;
                    y1 = hy;
                } else {
                    x0 = hx;
                    y0 = hy;
                }
            }
        }
        jumper_rgb_draw(&board->jumpers[i], bright, &cr, &cg, &cb);
        sx0 = board_sx(ui, x0);
        sy0 = board_sy(ui, y0);
        sx1 = board_sx(ui, x1);
        sy1 = board_sy(ui, y1);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        draw_2elbow(r, sx0, sy0, sx1, sy1, manhattan_h_first(x0, y0, x1, y1), R01A_HOVER_JOG);
        if (sel || hover) {
            draw_end_handle(r, sx0, sy0, sel ? 255 : cr, sel ? 230 : cg, sel ? 80 : cb);
            draw_end_handle(r, sx1, sy1, sel ? 255 : cr, sel ? 230 : cg, sel ? 80 : cb);
        }
    }
    if (ui->jumper_arm && ui->jumper_bb) {
        int x0;
        int y0;
        const Uint8 *rgb = k_jumper_rgb[ui->jumper_color_i % R01A_JUMPER_COLORS];
        ns_breadboard_hole_world(ui->jumper_bb, ui->jumper_from, &x0, &y0);
        SDL_SetRenderDrawColor(r, rgb[0], rgb[1], rgb[2], 255);
        draw_2elbow(r, board_sx(ui, x0), board_sy(ui, y0), ui->mouse_lx, ui->mouse_ly,
                    manhattan_h_first(board_sx(ui, x0), board_sy(ui, y0), ui->mouse_lx, ui->mouse_ly),
                    R01A_HOVER_JOG);
        draw_end_handle(r, board_sx(ui, x0), board_sy(ui, y0), rgb[0], rgb[1], rgb[2]);
    }
}

static void present_frame(R01aUi *ui);

static void draw_frame(R01aUi *ui, R01aBoard *board) {
    char status[128];
    char tip[96];
    int rank;
    SDL_Rect btn;
    int hud_x;

    SDL_SetRenderTarget(ui->rend, ui->target);
    SDL_SetRenderDrawColor(ui->rend, NS_BOARD_BG_R, NS_BOARD_BG_G, NS_BOARD_BG_B, 255);
    SDL_RenderClear(ui->rend);

    ui->pin_gray = r01a_board_wire_mode(board) == R01A_WIRE_MANUAL;

    for (rank = 0; rank < ui->chip_count; rank++) {
        int ci = ui->chip_z[rank];
        int selected;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        selected = ui->chip_sel[ci] || ci == ui->selected;
        draw_entity(ui->rend, ui, ui->chips[ci], selected);
    }
    draw_hover_wires(ui->rend, ui, board);
    draw_jumpers(ui->rend, ui, board);

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

    wire_mode_btn_rect(r01a_board_wire_mode(board), &btn);
    fill_rect(ui->rend, btn.x, btn.y, btn.w, btn.h, 18, 28, 22);
    draw_rect(ui->rend, btn.x, btn.y, btn.w, btn.h,
              (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? 220 : 180,
              (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? 180 : 180, 80);
    r01a_font_draw(ui->rend, btn.x + 6, btn.y + 2, wire_mode_label(r01a_board_wire_mode(board)), 230, 230,
                   200);

    ns_island_group_fill_status(r01a_board_group(board), status, sizeof(status));
    hud_x = btn.x + btn.w + 10;
    if (ui->jumper_mode) {
        r01a_font_draw(ui->rend, hud_x, 2, "JUMPER", 255, 210, 80);
        hud_x += r01a_font_text_width("JUMPER") + 8;
    }
    if (ui->jumper_arm || jumper_sel_any(ui)) {
        const Uint8 *rgb = k_jumper_rgb[ui->jumper_color_i % R01A_JUMPER_COLORS];
        if (!ui->jumper_arm) {
            int i;
            for (i = 0; i < board->jumper_count; i++) {
                if (ui->jumper_sel[i]) {
                    rgb = k_jumper_rgb[jumper_palette_i(&board->jumpers[i])];
                    break;
                }
            }
        }
        fill_rect(ui->rend, hud_x, 3, 10, 8, rgb[0], rgb[1], rgb[2]);
        hud_x += 16;
    }
    if (ui->jumper_mode || ui->jumper_arm || jumper_sel_any(ui)) {
        hud_x += 4;
    }
    r01a_font_draw_a(ui->rend, hud_x, 2, board->running ? "RUN" : "PAUSE", 180, 180, 120, 180);
    hud_x += r01a_font_text_width("PAUSE") + 10;
    r01a_font_draw_a(ui->rend, hud_x, 2, status, 200, 210, 200, 160);

    {
        char fps[16];
        int tw;
        Uint32 now = SDL_GetTicks();
        ui->fps_n++;
        if (ui->fps_t0 == 0) {
            ui->fps_t0 = now;
        }
        if ((now - ui->fps_t0) >= 1000u) {
            ui->fps = ui->fps_n;
            ui->fps_n = 0;
            ui->fps_t0 = now;
        }
        snprintf(fps, sizeof(fps), "%d", ui->fps);
        tw = r01a_font_text_width(fps);
        r01a_font_draw_a(ui->rend, NS_LOGIC_W - 4 - tw, 2, fps, 160, 170, 150, 180);
    }

    if (ui->ctx_open) {
        draw_ctx_menu(ui->rend, ui);
    } else if (SDL_GetTicks() >= ui->tip_show_at && !ui->drag_pan && ui->drag_chip < 0 && ui->drag_jumper < 0 &&
               !ui->box_sel && !ui->jumper_arm) {
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
    if (!hit_pin_at(ui, board_mx, board_my, &ui->hover_chip, &ui->hover_pin)) {
        ui->hover_chip = -1;
        ui->hover_pin = -1;
    }

    if (e->type == SDL_MOUSEMOTION && (lx != ui->tip_stable_mx || ly != ui->tip_stable_my)) {
        tip_reset(ui, lx, ly);
    }
    {
        int i;
        NsBreadboard *bb;
        NsPbHole hole;
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chips[i] && ui->chips[i]->visual == NS_ENTITY_VIS_BREADBOARD) {
                ns_breadboard_clear_hover((NsBreadboard *)ui->chips[i]);
            }
        }
        bb = hit_breadboard(ui, board_mx, board_my, &hole);
        if (bb) {
            ns_breadboard_set_hover(bb, 1, hole);
        }
    }
    if (ui->drag_jumper >= 0) {
        ui->hover_jumper = ui->drag_jumper;
    } else if (ui->jumper_arm) {
        ui->hover_jumper = -1;
    } else {
        int je;
        int end_dummy;
        if (hit_jumper_end(ui, board, board_mx, board_my, &je, &end_dummy)) {
            ui->hover_jumper = je;
        } else {
            ui->hover_jumper = hit_jumper_body(ui, board, board_mx, board_my);
        }
    }
    if (e->type == SDL_MOUSEWHEEL) {
        if (ui->jumper_arm) {
            if (e->wheel.y > 0) {
                ui->jumper_color_i = (ui->jumper_color_i + 1) % R01A_JUMPER_COLORS;
            } else if (e->wheel.y < 0) {
                ui->jumper_color_i = (ui->jumper_color_i + R01A_JUMPER_COLORS - 1) % R01A_JUMPER_COLORS;
            }
            return 1;
        }
        if (jumper_sel_any(ui)) {
            if (e->wheel.y > 0) {
                jumper_cycle_selected(ui, board, 1);
            } else if (e->wheel.y < 0) {
                jumper_cycle_selected(ui, board, -1);
            }
            return 1;
        }
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
        ui->right_armed = 1;
        ui->right_pan = 0;
        ui->right_lx = lx;
        ui->right_ly = ly;
        ui->drag_last_x = lx;
        ui->drag_last_y = ly;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_RIGHT) {
        int chip_i;
        ui->right_armed = 0;
        if (ui->right_pan || ui->drag_pan) {
            ui->right_pan = 0;
            ui->drag_pan = 0;
            return 1;
        }
        chip_i = hit_top_chip(ui, lx, ly);
        if (chip_i < 0) {
            ui->ctx_open = 1;
            ui->ctx_lx = lx;
            ui->ctx_ly = ly;
            ui->ctx_bx = board_mx;
            ui->ctx_by = board_my;
        } else {
            ui->ctx_open = 0;
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->right_armed && !ui->right_pan) {
        int dx = lx - ui->right_lx;
        int dy = ly - ui->right_ly;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dx > 5 || dy > 5) {
            ui->right_pan = 1;
            ui->drag_pan = 1;
            ui->ctx_open = 0;
        }
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
    if (e->type == SDL_MOUSEMOTION && ui->drag_jumper >= 0) {
        const NsBreadboard *bb = ui_bb_named(ui, board->jumpers[ui->drag_jumper].bb_ref);
        NsPbHole hole;
        ui->drag_jumper_preview_ok = 0;
        if (bb && ns_breadboard_hit_hole(bb, board_mx, board_my, &hole)) {
            ui->drag_jumper_preview = hole;
            ui->drag_jumper_preview_ok = 1;
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        if (sel_count(ui) > 1) {
            move_selection_drag(ui, board_mx, board_my);
            snap_selection_to_breadboard(ui);
        } else {
            move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
            snap_chip_to_breadboard(ui, ui->drag_chip);
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
                sel_from_box(ui, board, shift);
            } else if (!shift) {
                sel_clear(ui);
            }
            return 1;
        }
        if (ui->drag_jumper >= 0) {
            if (ui->drag_jumper_preview_ok) {
                (void)r01a_board_jumper_set_end(board, ui->drag_jumper, ui->drag_jumper_end,
                                                ui->drag_jumper_preview);
            }
            ui->drag_jumper = -1;
            ui->drag_jumper_preview_ok = 0;
            return 1;
        }
        if (ui->drag_chip >= 0) {
            snap_selection_to_breadboard(ui);
        }
        ui->drag_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int chip_i;
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        SDL_Rect btn;
        NsBreadboard *bb;
        NsPbHole hole;

        if (ui->ctx_open) {
            int item = ctx_hit_item(ui, lx, ly);
            ui->ctx_open = 0;
            if (item >= 0) {
                ctx_apply(ui, board, item);
            }
            return 1;
        }

        wire_mode_btn_rect(r01a_board_wire_mode(board), &btn);
        if (point_in_rect(lx, ly, &btn)) {
            int next = (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? R01A_WIRE_AUTO : R01A_WIRE_MANUAL;
            r01a_board_set_wire_mode(board, next);
            ui->jumper_arm = 0;
            return 1;
        }

        if (e->button.clicks == 2) {
            chip_i = hit_top_chip(ui, lx, ly);
            if (chip_i >= 0 && ui->chips[chip_i] && ui->chips[chip_i]->visual == NS_ENTITY_VIS_DISPLAY) {
                NsVideoSink *sink = (NsVideoSink *)ui->chips[chip_i];
                ns_video_sink_set_scale_2x(sink, !ns_video_sink_scale_2x(sink));
                return 1;
            }
        }

        ui->drag_chip = -1;
        ui->drag_jumper = -1;
        chip_i = hit_top_chip(ui, lx, ly);
        bb = hit_breadboard(ui, board_mx, board_my, &hole);
        if (ui->jumper_mode && ui->jumper_arm && bb) {
            if (ui->jumper_bb == bb && ui->jumper_from.col == hole.col &&
                ui->jumper_from.lane == hole.lane) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
            } else if (ui->jumper_bb == bb) {
                const Uint8 *rgb = k_jumper_rgb[ui->jumper_color_i % R01A_JUMPER_COLORS];
                r01a_board_jumper_add_on(board, bb, ui->jumper_from, hole, rgb[0], rgb[1], rgb[2]);
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
            } else {
                ui->jumper_arm = 1;
                ui->jumper_from = hole;
                ui->jumper_bb = bb;
            }
            return 1;
        }
        {
            int ji;
            int end_i;
            if (hit_jumper_end(ui, board, board_mx, board_my, &ji, &end_i)) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                if (shift) {
                    ui->jumper_sel[ji] = ui->jumper_sel[ji] ? 0 : 1;
                } else {
                    jumper_sel_set_one(ui, ji);
                }
                ui->drag_jumper = ji;
                ui->drag_jumper_end = end_i;
                ui->drag_jumper_preview_ok = 0;
                return 1;
            }
            ji = hit_jumper_body(ui, board, board_mx, board_my);
            if (ji >= 0) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                if (shift) {
                    ui->jumper_sel[ji] = ui->jumper_sel[ji] ? 0 : 1;
                } else {
                    jumper_sel_set_one(ui, ji);
                }
                return 1;
            }
        }
        if (ui->jumper_mode && bb) {
            ui->jumper_arm = 1;
            ui->jumper_from = hole;
            ui->jumper_bb = bb;
            return 1;
        }
        if (ui->jumper_arm) {
            ui->jumper_arm = 0;
            ui->jumper_bb = NULL;
        }
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
            if (ui->ctx_open) {
                ui->ctx_open = 0;
                return 1;
            }
            if (ui->jumper_arm) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                return 1;
            }
            if (ui->jumper_mode) {
                ui->jumper_mode = 0;
                return 1;
            }
            return 2;
        }
        if (e->key.keysym.sym == SDLK_j) {
            ui->jumper_mode = !ui->jumper_mode;
            if (!ui->jumper_mode) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_DELETE || e->key.keysym.sym == SDLK_BACKSPACE) {
            jumper_delete_selected(ui, board);
            breadboard_delete_selected(ui, board);
            return 1;
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
        if (e->key.keysym.sym == SDLK_a) {
            int next = (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? R01A_WIRE_AUTO : R01A_WIRE_MANUAL;
            r01a_board_set_wire_mode(board, next);
            ui->jumper_arm = 0;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_x) {
            r01a_board_jumper_clear(board);
            ui->jumper_arm = 0;
            ui->jumper_bb = NULL;
            memset(ui->jumper_sel, 0, sizeof(ui->jumper_sel));
            ui->hover_jumper = -1;
            ui->drag_jumper = -1;
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
    ui.jumper_arm = 0;
    ui.jumper_mode = 0;
    ui.jumper_color_i = 0;
    ui.hover_jumper = -1;
    ui.drag_jumper = -1;
    ui.hover_chip = -1;
    ui.hover_pin = -1;
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
    (void)r01a_layout_load(R01A_LAYOUT_FILE, board, &ui.pan_x, &ui.pan_y);
    bind_chips(&ui, board);
    pin_net_build(&ui);
    clamp_pan(&ui);

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

    (void)r01a_layout_save(R01A_LAYOUT_FILE, board, ui.pan_x, ui.pan_y);

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
