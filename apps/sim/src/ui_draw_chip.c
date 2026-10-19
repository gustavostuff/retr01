#include "discrete_ic/pin_header.h"

#include "ui.h"
#include "retr01_sim/ui_button.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "breadboard.h"
#include "passive.h"
#include "ui_assets.h"
#include "video_sink.h"
#include "retr01_sim/ns_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defined later in this file. Used by manual pin-pivot status. */
int ui_chip_pin_tip_board(const R01sEntity *e, int pin_num, int *tbx, int *tby);

void pin_level_rgb(R01sLevel lvl, R01sPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
    if (dir == R01S_PIN_PWR) {
        *pr = 220;
        *pg = 70;
        *pb = 70;
        return;
    }
    if (dir == R01S_PIN_NC) {
        *pr = 70;
        *pg = 70;
        *pb = 70;
        return;
    }
    switch (lvl) {
    case R01S_LVL_H:
        *pr = 70;
        *pg = 210;
        *pb = 90;
        break;
    case R01S_LVL_L:
        /* Solid dark -- no light center (pad is a filled block). */
        *pr = 28;
        *pg = 32;
        *pb = 30;
        break;
    case R01S_LVL_X:
        *pr = 220;
        *pg = 80;
        *pb = 200;
        break;
    default:
        /* Hi-Z / undriven */
        *pr = 120;
        *pg = 125;
        *pb = 110;
        break;
    }
}

void ui_chip_pin_rgb(const R01sUi *ui, const R01sEntity *e, int pin_index, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
    int rail;
    if (!pr || !pg || !pb) {
        return;
    }
    rail = ui_pin_rail_kind(ui, e, pin_index);
    if (rail == 1) {
        *pr = R01S_UI_PIN_VCC_R;
        *pg = R01S_UI_PIN_VCC_G;
        *pb = R01S_UI_PIN_VCC_B;
        return;
    }
    if (rail == 2) {
        *pr = R01S_UI_PIN_GND_R;
        *pg = R01S_UI_PIN_GND_G;
        *pb = R01S_UI_PIN_GND_B;
        return;
    }
    *pr = R01S_UI_PIN_GRAY_R;
    *pg = R01S_UI_PIN_GRAY_G;
    *pb = R01S_UI_PIN_GRAY_B;
}

/* Manual BB status for pin.png pivot (col OX, row OY). Priority: red > green > yellow. */
enum {
    R01S_UI_PIN_PIVOT_NONE = 0,
    R01S_UI_PIN_PIVOT_YELLOW,
    R01S_UI_PIN_PIVOT_GREEN,
    R01S_UI_PIN_PIVOT_RED
};

static int ui_tip_bb_strip(const R01sUi *ui, int tip_x, int tip_y, int *bb_out, int *strip_out) {
    int bi;
    if (!ui) {
        return 0;
    }
    for (bi = 0; bi < ui->chip_count; bi++) {
        const R01sEntity *be = ui->chips[bi];
        const R01sBreadboard *bb;
        int strip = -1;
        if (!be || be->visual != R01S_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        bb = (const R01sBreadboard *)(const void *)be;
        if (!r01s_breadboard_tip_strip(bb, tip_x, tip_y, &strip)) {
            continue;
        }
        if (bb_out) {
            *bb_out = bi;
        }
        if (strip_out) {
            *strip_out = strip;
        }
        return 1;
    }
    return 0;
}

static int ui_strip_tip_count(const R01sUi *ui, int bb_i, int strip) {
    int ci;
    int n = 0;
    if (!ui || bb_i < 0 || bb_i >= ui->chip_count) {
        return 0;
    }
    for (ci = 0; ci < ui->chip_count; ci++) {
        const R01sEntity *e = ui->chips[ci];
        int dip;
        int pi;
        if (!e || e->visual != R01S_ENTITY_VIS_IC || ui_chip_hidden(ui, e)) {
            continue;
        }
        dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
        for (pi = 0; pi < e->pin_count; pi++) {
            int num = e->pins[pi].number;
            int tx, ty;
            int found_bb = -1;
            int found_strip = -1;
            if (num < 1 || num > dip) {
                continue;
            }
            if (!ui_chip_pin_tip_board(e, num, &tx, &ty)) {
                continue;
            }
            if (!ui_tip_bb_strip(ui, tx, ty, &found_bb, &found_strip)) {
                continue;
            }
            if (found_bb == bb_i && found_strip == strip) {
                n++;
            }
        }
    }
    return n;
}

static int ui_ic_any_tip_on_bb(const R01sUi *ui, const R01sEntity *e) {
    int dip;
    int pi;
    if (!ui || !e) {
        return 0;
    }
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    for (pi = 0; pi < e->pin_count; pi++) {
        int num = e->pins[pi].number;
        int tx, ty;
        if (num < 1 || num > dip) {
            continue;
        }
        if (!ui_chip_pin_tip_board(e, num, &tx, &ty)) {
            continue;
        }
        if (ui_tip_bb_strip(ui, tx, ty, NULL, NULL)) {
            return 1;
        }
    }
    return 0;
}

/* Returns pivot overlay kind for MANUAL mode. LIVE always NONE. */
static int ui_manual_pin_pivot(const R01sUi *ui, const R01sEntity *e, int pin_num) {
    int tx, ty;
    int bb_i = -1;
    int strip = -1;
    if (!ui || ui->wire_mode != R01S_WIRE_MANUAL || !e || e->visual != R01S_ENTITY_VIS_IC) {
        return R01S_UI_PIN_PIVOT_NONE;
    }
    if (!ui_chip_pin_tip_board(e, pin_num, &tx, &ty)) {
        return R01S_UI_PIN_PIVOT_NONE;
    }
    if (ui_tip_bb_strip(ui, tx, ty, &bb_i, &strip)) {
        if (ui_strip_tip_count(ui, bb_i, strip) >= 2) {
            return R01S_UI_PIN_PIVOT_RED;
        }
        return R01S_UI_PIN_PIVOT_GREEN;
    }
    if (ui_ic_any_tip_on_bb(ui, e)) {
        return R01S_UI_PIN_PIVOT_YELLOW;
    }
    return R01S_UI_PIN_PIVOT_NONE;
}

static void ui_pin_pivot_rgb(int kind, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
    switch (kind) {
    case R01S_UI_PIN_PIVOT_RED:
        *pr = 230;
        *pg = 40;
        *pb = 40;
        break;
    case R01S_UI_PIN_PIVOT_GREEN:
        *pr = 40;
        *pg = 210;
        *pb = 70;
        break;
    case R01S_UI_PIN_PIVOT_YELLOW:
        *pr = 230;
        *pg = 210;
        *pb = 40;
        break;
    default:
        *pr = 255;
        *pg = 255;
        *pb = 255;
        break;
    }
}

/* Very light green over the sprite's own color. */
static void hover_green(Uint8 *r, Uint8 *g, Uint8 *b) {
    *r = (Uint8)((*r * 65 + 140 * 35) / 100);
    *g = (Uint8)((*g * 65 + 255 * 35) / 100);
    *b = (Uint8)((*b * 65 + 175 * 35) / 100);
}

static void pin_sprite_box(int rot, int *org_x, int *org_y, int *ow, int *oh) {
    int w = R01S_UI_PIN_W;
    int h = R01S_UI_PIN_H;
    int ox0 = R01S_UI_PIN_OX;
    int oy0 = R01S_UI_PIN_OY;
    if (rot == 1 || rot == 3) {
        *ow = h;
        *oh = w;
    } else {
        *ow = w;
        *oh = h;
    }
    if (rot == 0) {
        *org_x = ox0;
        *org_y = oy0;
    } else if (rot == 1) {
        *org_x = h - 1 - oy0;
        *org_y = ox0;
    } else if (rot == 2) {
        *org_x = w - 1 - ox0;
        *org_y = h - 1 - oy0;
    } else {
        *org_x = oy0;
        *org_y = w - 1 - ox0;
    }
}

static void pin_sprite_src(int rot, int ox, int oy, int *sx, int *sy) {
    int w = R01S_UI_PIN_W;
    int h = R01S_UI_PIN_H;
    if (rot == 0) {
        *sx = ox;
        *sy = oy;
    } else if (rot == 1) {
        *sx = oy;
        *sy = h - 1 - ox;
    } else if (rot == 2) {
        *sx = w - 1 - ox;
        *sy = h - 1 - oy;
    } else {
        *sx = w - 1 - oy;
        *sy = ox;
    }
}

/* rot: 0=N (sprite as-is), 1=E, 2=S, 3=W.
 * tint=NULL draws pin.png RGB as-is. Otherwise opaque pixels use (tr,tg,tb).
 * (dx,dy) is where the sprite origin (OX,OY) lands after rotation.
 * hover mixes a light green into those pixels. */
static void blit_pin_rot(SDL_Renderer *r, int dx, int dy, int rot, const Uint8 *tint_rgb, int hover) {
    int w = R01S_UI_PIN_W;
    int sx, sy, ox, oy, ow, oh;
    int tl_x, tl_y;
    int org_x, org_y;

    if (!r) {
        return;
    }
    pin_sprite_box(rot, &org_x, &org_y, &ow, &oh);
    tl_x = dx - org_x;
    tl_y = dy - org_y;
    for (oy = 0; oy < oh; oy++) {
        for (ox = 0; ox < ow; ox++) {
            const uint8_t *p;
            Uint8 cr;
            Uint8 cg;
            Uint8 cb;
            pin_sprite_src(rot, ox, oy, &sx, &sy);
            p = R01S_UI_PIN_RGBA + ((size_t)sy * (size_t)w + (size_t)sx) * 4u;
            if (p[3] == 0) {
                continue;
            }
            if (tint_rgb) {
                cr = tint_rgb[0];
                cg = tint_rgb[1];
                cb = tint_rgb[2];
            } else {
                cr = p[0];
                cg = p[1];
                cb = p[2];
            }
            if (hover) {
                hover_green(&cr, &cg, &cb);
            }
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderDrawPoint(r, tl_x + ox, tl_y + oy);
        }
    }
}

static int pin_sprite_hit(int tip_sx, int tip_sy, int rot, int lx, int ly) {
    int org_x;
    int org_y;
    int ow;
    int oh;
    int ox;
    int oy;
    pin_sprite_box(rot, &org_x, &org_y, &ow, &oh);
    ox = lx - (tip_sx - org_x);
    oy = ly - (tip_sy - org_y);
    /* Whole pin.png rectangle. Transparent corners still start a drag. */
    return ox >= 0 && oy >= 0 && ox < ow && oy < oh;
}

static void blit_pin_pivot(SDL_Renderer *r, int tip_sx, int tip_sy, int pivot_kind) {
    Uint8 pr, pg, pb;
    if (!r || pivot_kind == R01S_UI_PIN_PIVOT_NONE) {
        return;
    }
    ui_pin_pivot_rgb(pivot_kind, &pr, &pg, &pb);
    SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
    SDL_RenderDrawPoint(r, tip_sx, tip_sy);
}

/* Reach from tip (origin) to the far base row of the sprite. */
static int pin_tip_reach(void) {
    return R01S_UI_PIN_H - 1 - R01S_UI_PIN_OY;
}

/* DIP pad: tip (origin) points away. Base sits on the first pixel OUTSIDE the
 * body so pins meet the package edge-to-edge with no overlap.
 * tint_rgb=NULL keeps pin.png colors. pivot_kind paints tip pixel after blit. */
static void draw_dip_pad_h(SDL_Renderer *r, int px, int body_edge_y, int outward_down, const Uint8 *tint_rgb,
                           int pivot_kind, int hover) {
    int reach = pin_tip_reach();
    int tip_y;
    if (outward_down) {
        /* body_edge_y is exclusive bottom (y + h). Base at that row. */
        tip_y = body_edge_y + reach;
        blit_pin_rot(r, px, tip_y, 2, tint_rgb, hover);
        blit_pin_pivot(r, px, tip_y, pivot_kind);
    } else {
        /* body_edge_y is inclusive top. Base at edge - 1. */
        tip_y = body_edge_y - 1 - reach;
        blit_pin_rot(r, px, tip_y, 0, tint_rgb, hover);
        blit_pin_pivot(r, px, tip_y, pivot_kind);
    }
}

static void draw_dip_pad_v(SDL_Renderer *r, int py, int body_edge_x, int outward_left, const Uint8 *tint_rgb,
                           int pivot_kind, int hover) {
    int reach = pin_tip_reach();
    int tip_x;
    if (outward_left) {
        tip_x = body_edge_x - 1 - reach;
        blit_pin_rot(r, tip_x, py, 3, tint_rgb, hover);
        blit_pin_pivot(r, tip_x, py, pivot_kind);
    } else {
        /* body_edge_x is exclusive right (x + w). Base at that col. */
        tip_x = body_edge_x + reach;
        blit_pin_rot(r, tip_x, py, 1, tint_rgb, hover);
        blit_pin_pivot(r, tip_x, py, pivot_kind);
    }
}

static void draw_glyph_pins(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int board_x, int board_y) {
    int x = ui_board_sx(ui, board_x);
    int li = 0;
    int ri = 0;
    int i;
    int manual = ui && ui->wire_mode == R01S_WIRE_MANUAL;
    for (i = 0; i < e->pin_count; i++) {
        int side_left;
        int idx;
        int py;
        Uint8 tint[3];
        const Uint8 *tint_rgb;
        int pivot = R01S_UI_PIN_PIVOT_NONE;
        if (e->pins[i].dir == R01S_PIN_PWR || e->pins[i].dir == R01S_PIN_NC) {
            continue;
        }
        side_left = (e->pins[i].dir == R01S_PIN_IN || e->pins[i].dir == R01S_PIN_IO) ? 1 : 0;
        if (side_left) {
            idx = li++;
        } else {
            idx = ri++;
        }
        py = board_y + 5 + idx * 5;
        if (py > board_y + e->body_h - 3) {
            py = board_y + e->body_h - 3;
        }
        py = ui_board_sy(ui, py);
        if (manual) {
            tint_rgb = NULL;
        } else {
            ui_chip_pin_rgb(ui, e, i, &tint[0], &tint[1], &tint[2]);
            tint_rgb = tint;
        }
        if (side_left) {
            draw_dip_pad_v(r, py, x, 1, tint_rgb, pivot, 0);
        } else {
            draw_dip_pad_v(r, py, x + e->body_w, 0, tint_rgb, pivot, 0);
        }
    }
}

static void blit_rgba_scaled(SDL_Renderer *r, int dx, int dy, const uint8_t *rgba, int w, int h, int scale) {
    int x, y, sx, sy;
    if (!rgba || w <= 0 || h <= 0 || scale < 1) {
        return;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const uint8_t *p = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            if (p[3] == 0) {
                continue;
            }
            SDL_SetRenderDrawColor(r, p[0], p[1], p[2], 255);
            for (sy = 0; sy < scale; sy++) {
                for (sx = 0; sx < scale; sx++) {
                    SDL_RenderDrawPoint(r, dx + x * scale + sx, dy + y * scale + sy);
                }
            }
        }
    }
}

void draw_segment_btn(SDL_Renderer *r, const SDL_Rect *rc, int selected, const char *label) {
    int tw;
    int tx;
    int ty;
    if (!r || !rc || !label) {
        return;
    }
    tw = font_text_width(label);
    tx = rc->x + (rc->w - tw) / 2;
    ty = rc->y + (rc->h - font_line_h()) / 2;
    fill_rect(r, rc->x, rc->y, rc->w, rc->h, selected ? 36 : 22, selected ? 52 : 30, selected ? 40 : 28);
    draw_rect(r, rc->x, rc->y, rc->w, rc->h, selected ? 140 : 80, selected ? 170 : 100, selected ? 120 : 85);
    font_draw(r, tx, ty, label, selected ? 220 : 170, selected ? 230 : 180, selected ? 200 : 160);
}

static void draw_pwr_glyph(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int img_w = R01S_UI_BATTERY_W;
    int ix = x + (e->body_w - img_w) / 2;
    int iy = y + 2;
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    fill_rect(r, x, y, e->body_w, e->body_h, 0, 0, 0);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, 255, 220, 80);
    }
    blit_rgba_scaled(r, ix, iy, R01S_UI_BATTERY_RGBA, R01S_UI_BATTERY_W, R01S_UI_BATTERY_H, 1);
}

static void draw_osc_glyph(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int px;
    int py;
    int i;
    if (!e) {
        return;
    }
    if (!ns_osc4legs_chip_tip(e, 14, &px, &py)) {
        px = e->board_x;
        py = e->board_y;
    }
    ns_passive_draw_kind(r, NS_PASSIVE_OSC4LEGS, e->orient, ui_board_sx(ui, px), ui_board_sy(ui, py), selected);
    for (i = 0; i < e->pin_count; i++) {
        int tbx;
        int tby;
        Uint8 tint[3];
        int num = e->pins[i].number;
        int rot;
        int pin_hover;
        if (!ns_osc4legs_chip_tip(e, num, &tbx, &tby)) {
            continue;
        }
        ui_chip_pin_rgb(ui, e, i, &tint[0], &tint[1], &tint[2]);
        rot = ((num == 14 || num == 8) ? 0 : 2);
        rot = (rot + (int)e->orient) & 3;
        pin_hover = ui && ui->hover_pin == i && ui->hover_chip >= 0 && ui->hover_chip < ui->chip_count &&
                    ui->chips[ui->hover_chip] == e;
        blit_pin_rot(r, ui_board_sx(ui, tbx), ui_board_sy(ui, tby), rot, tint, pin_hover);
    }
}

static void draw_button_glyph(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected) {
    const R01sUiButton *btn = (const R01sUiButton *)e;
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int pressed = btn && btn->pressed > 0;
    Uint8 br = pressed ? 90 : 55;
    Uint8 bg = pressed ? 120 : 80;
    Uint8 bb = pressed ? 150 : 110;
    int tw;
    int fh = font_line_h();

    fill_rect(r, x, y, e->body_w, e->body_h, br, bg, bb);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 180, selected ? 220 : 200, selected ? 80 : 160);
    if (btn && btn->label[0]) {
        tw = font_text_width(btn->label);
        if (tw <= e->body_w - 4) {
            font_draw(r, x + (e->body_w - tw) / 2, y + (e->body_h - fh) / 2, btn->label, 230, 235, 240);
        }
    }
}

static void draw_panel_glyph(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    const char *label;
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);

    (void)ui;
    fill_rect(r, x, y, e->body_w, e->body_h, 38, 42, 48);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 120, selected ? 220 : 100, selected ? 80 : 85);
    label = (e->refdes && e->refdes[0]) ? e->refdes : e->part;
    if (label && label[0] && e->body_w > 4 && e->body_h > 4) {
        unsigned seed = 0;
        const char *s;
        for (s = e->refdes ? e->refdes : label; *s; s++) {
            seed = seed * 131u + (unsigned char)*s;
        }
        ui_draw_label_bounce(r, x + 2, y + 2, e->body_w - 4, e->body_h - 4, label, seed, 180, 185, 195, 255);
    }
}

static void draw_display_glyph(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    const R01sVideoSink *sink;
    int lcd_w;
    int lcd_h;

    if (!e->part || strcmp(e->part, "SCREEN_SINK") != 0) {
        draw_panel_glyph(r, ui, e, selected);
        return;
    }
    sink = (const R01sVideoSink *)(e->impl ? e->impl : (void *)e);
    if (!sink) {
        draw_panel_glyph(r, ui, e, selected);
        return;
    }
    r01s_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    draw_video_pixels(r, ui, (R01sVideoSink *)(void *)sink, x, y, lcd_w, lcd_h);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, 255, 220, 80);
    }
}

/* Pin along-axis offset from body origin + whether pin is on the pin-1 side.
 * Pitch is always JEDEC 0.100" (R01S_DIP_PIN_PITCH_PX); end margin centers the row. */
void ui_chip_dip_pin_pos(const R01sEntity *e, int pin_num, int *along, int *side_pin1) {
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int half = dip / 2;
    int idx;
    int span;
    int pitch = e->pkg_pitch_px > 0 ? e->pkg_pitch_px : R01S_DIP_PIN_PITCH_PX;
    int row_span;
    int margin;
    int reverse;

    if (dip <= 0 || pin_num <= 0 || pin_num > dip) {
        *side_pin1 = 1;
        *along = r01s_orient_is_horiz(e->orient) ? (e->body_w / 2) : (e->body_h / 2);
        return;
    }
    *side_pin1 = pin_num <= half;
    idx = *side_pin1 ? (pin_num - 1) : (dip - pin_num);
    span = r01s_orient_is_horiz(e->orient) ? e->body_w : e->body_h;
    row_span = (half > 1) ? (half - 1) * pitch : 0;
    margin = (span - row_span) / 2;
    if (margin < 1) {
        margin = 1;
    }
    reverse = (e->orient == R01S_ORIENT_180 || e->orient == R01S_ORIENT_270);
    if (reverse) {
        *along = margin + (half > 0 ? (half - 1 - idx) : 0) * pitch;
    } else {
        *along = margin + idx * pitch;
    }
}

/* Tip origin (pin.png col 1, row 0) in board canvas coords. */
int ui_chip_pin_tip_board(const R01sEntity *e, int pin_num, int *tbx, int *tby) {
    int along;
    int side_pin1;
    int reach;
    int dip;

    if (!e || !tbx || !tby) {
        return 0;
    }
    if (e->visual == R01S_ENTITY_VIS_PASSIVE) {
        return r01s_passive_tip_board((const R01sPassive *)(const void *)e, pin_num, tbx, tby);
    }
    if (e->visual == R01S_ENTITY_VIS_OSC) {
        return ns_osc4legs_chip_tip(e, pin_num, tbx, tby);
    }
    if (e->visual == R01S_ENTITY_VIS_PIN_HDR) {
        return ns_pin_header_pin_tip_board(e, pin_num, tbx, tby);
    }
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    if (pin_num < 1 || pin_num > dip) {
        return 0;
    }
    reach = R01S_UI_PIN_H - 1 - R01S_UI_PIN_OY;
    ui_chip_dip_pin_pos(e, pin_num, &along, &side_pin1);
    switch (e->orient) {
    case R01S_ORIENT_90:
        *tby = e->board_y + along;
        *tbx = side_pin1 ? (e->board_x - 1 - reach) : (e->board_x + e->body_w + reach);
        break;
    case R01S_ORIENT_180:
        *tbx = e->board_x + along;
        *tby = side_pin1 ? (e->board_y - 1 - reach) : (e->board_y + e->body_h + reach);
        break;
    case R01S_ORIENT_270:
        *tby = e->board_y + along;
        *tbx = side_pin1 ? (e->board_x + e->body_w + reach) : (e->board_x - 1 - reach);
        break;
    case R01S_ORIENT_0:
    default:
        *tbx = e->board_x + along;
        *tby = side_pin1 ? (e->board_y + e->body_h + reach) : (e->board_y - 1 - reach);
        break;
    }
    return 1;
}

static int dip_pin_rot(const R01sEntity *e, int side_pin1);

/* Package pin whose pin.png covers zoom-divided screen (zx, zy), or -1. */
static int ic_pin_index_at(const R01sUi *ui, const R01sEntity *e, int zx, int zy) {
    int dip;
    int i;
    if (!ui || !e || e->visual != R01S_ENTITY_VIS_IC) {
        return -1;
    }
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int along;
        int side_pin1;
        int tbx;
        int tby;
        if (num < 1 || num > dip) {
            continue;
        }
        if (!ui_chip_pin_tip_board(e, num, &tbx, &tby)) {
            continue;
        }
        ui_chip_dip_pin_pos(e, num, &along, &side_pin1);
        (void)along;
        if (pin_sprite_hit(ui_board_sx(ui, tbx), ui_board_sy(ui, tby), dip_pin_rot(e, side_pin1), zx, zy)) {
            return i;
        }
    }
    return -1;
}

int ui_ic_hover_pin(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    if (!ui || !e) {
        return -1;
    }
    return ic_pin_index_at(ui, e, ui_div_floor(lx, ui_zoom(ui)), ui_div_floor(ly, ui_zoom(ui)));
}

static int dip_pin_rot(const R01sEntity *e, int side_pin1) {
    switch (e->orient) {
    case R01S_ORIENT_90:
        return side_pin1 ? 3 : 1;
    case R01S_ORIENT_180:
        return side_pin1 ? 0 : 2;
    case R01S_ORIENT_270:
        return side_pin1 ? 1 : 3;
    case R01S_ORIENT_0:
    default:
        return side_pin1 ? 2 : 0;
    }
}

int ui_chip_pin_screen_center(const R01sUi *ui, const R01sEntity *e, int pin_index, int *sx, int *sy) {
    int num;
    int tbx;
    int tby;

    if (!ui || !e || !sx || !sy || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    num = e->pins[pin_index].number;
    if (!ui_chip_pin_tip_board(e, num, &tbx, &tby)) {
        return 0;
    }
    *sx = ui_board_sx(ui, tbx);
    *sy = ui_board_sy(ui, tby);
    return 1;
}

static void draw_chip(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int i;
    int horiz = r01s_orient_is_horiz(e->orient);
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int manual = ui && ui->wire_mode == R01S_WIRE_MANUAL;
    Uint8 br, bg, bb;

    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int along;
        int side_pin1;
        Uint8 tint[3];
        const Uint8 *tint_rgb;
        int pivot;
        if (num < 1 || num > dip) {
            continue;
        }
        ui_chip_dip_pin_pos(e, num, &along, &side_pin1);
        pivot = ui_manual_pin_pivot(ui, e, num);
        if (manual) {
            tint_rgb = NULL;
        } else {
            ui_chip_pin_rgb(ui, e, i, &tint[0], &tint[1], &tint[2]);
            tint_rgb = tint;
            pivot = R01S_UI_PIN_PIVOT_NONE;
        }
        {
            int pin_hover = ui && ui->hover_pin == i && ui->hover_chip >= 0 && ui->hover_chip < ui->chip_count &&
                            ui->chips[ui->hover_chip] == e;
            switch (e->orient) {
            case R01S_ORIENT_90:
                draw_dip_pad_v(r, y + along, side_pin1 ? x : (x + e->body_w), side_pin1, tint_rgb, pivot, pin_hover);
                break;
            case R01S_ORIENT_180:
                draw_dip_pad_h(r, x + along, side_pin1 ? y : (y + e->body_h), !side_pin1, tint_rgb, pivot, pin_hover);
                break;
            case R01S_ORIENT_270:
                draw_dip_pad_v(r, y + along, side_pin1 ? (x + e->body_w) : x, !side_pin1, tint_rgb, pivot, pin_hover);
                break;
            case R01S_ORIENT_0:
            default:
                draw_dip_pad_h(r, x + along, side_pin1 ? (y + e->body_h) : y, side_pin1, tint_rgb, pivot, pin_hover);
                break;
            }
        }
    }

    ui_chip_body_rgb(e, selected, &br, &bg, &bb);
    if (ui && ui->hover_pin < 0 && ui->hover_chip >= 0 && ui->hover_chip < ui->chip_count &&
        ui->chips[ui->hover_chip] == e) {
        hover_green(&br, &bg, &bb);
    }
    fill_rect(r, x, y, e->body_w, e->body_h, br, bg, bb);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, 255, 220, 80);
    } else {
        NsOutlineRgb oc = ns_outline_rgb(e->health);
        draw_rect(r, x, y, e->body_w, e->body_h, oc.r, oc.g, oc.b);
    }
    /* Notch sits by pin 1. */
    switch (e->orient) {
    case R01S_ORIENT_90:
        fill_rect(r, x + e->body_w / 2 - 2, y - 1, 4, 2, 20, 22, 20);
        break;
    case R01S_ORIENT_180:
        fill_rect(r, x + e->body_w - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
        break;
    case R01S_ORIENT_270:
        fill_rect(r, x + e->body_w / 2 - 2, y + e->body_h - 1, 4, 2, 20, 22, 20);
        break;
    case R01S_ORIENT_0:
    default:
        fill_rect(r, x - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
        break;
    }
    /* Part label centered on body, 50% opaque white. Bounce-scroll when too wide
     * (same timing as island titles). Vertical packages: rot90 CCW, scroll along height. */
    {
        const char *label = e->part ? e->part : e->refdes;
        if (label && label[0]) {
            unsigned seed = 0;
            const char *s;
            int pad = 2;
            int view_w = e->body_w - pad * 2;
            int view_h = e->body_h - pad * 2;
            for (s = e->refdes ? e->refdes : label; *s; s++) {
                seed = seed * 131u + (unsigned char)*s;
            }
            if (view_w < 1) {
                view_w = e->body_w;
                pad = 0;
            }
            if (view_h < 1) {
                view_h = e->body_h;
                pad = 0;
            }
            if (horiz) {
                ui_draw_label_bounce(r, x + pad, y + pad, view_w, view_h, label, seed, 255, 255, 255, 128);
            } else {
                ui_draw_label_bounce_rot90ccw(r, x + pad, y + pad, view_w, view_h, label, seed, 255, 255, 255,
                                             128);
            }
        }
    }
}

static int filled_at(int lx, int ly, int x, int y, int w, int h) {
    return w > 0 && h > 0 && lx >= x && ly >= y && lx < x + w && ly < y + h;
}

/* Same placement as draw_glyph_pins. Whole pin.png rectangle. */
static int glyph_pins_hit(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    int x = ui_board_sx(ui, e->board_x);
    int li = 0;
    int ri = 0;
    int i;
    int reach = pin_tip_reach();
    for (i = 0; i < e->pin_count; i++) {
        int side_left;
        int idx;
        int py;
        int tip_x;
        int rot;
        if (e->pins[i].dir == R01S_PIN_PWR || e->pins[i].dir == R01S_PIN_NC) {
            continue;
        }
        side_left = (e->pins[i].dir == R01S_PIN_IN || e->pins[i].dir == R01S_PIN_IO) ? 1 : 0;
        if (side_left) {
            idx = li++;
        } else {
            idx = ri++;
        }
        py = e->board_y + 5 + idx * 5;
        if (py > e->board_y + e->body_h - 3) {
            py = e->board_y + e->body_h - 3;
        }
        py = ui_board_sy(ui, py);
        if (side_left) {
            tip_x = x - 1 - reach;
            rot = 3;
        } else {
            tip_x = x + e->body_w + reach;
            rot = 1;
        }
        if (pin_sprite_hit(tip_x, py, rot, lx, ly)) {
            return 1;
        }
    }
    return 0;
}

static int board_rect_hit(int bx, int by, int x, int y, int w, int h) {
    return w > 0 && h > 0 && bx >= x && by >= y && bx < x + w && by < y + h;
}

/* DIP body plus the pin stubs drawn on the long sides. */
static int ic_enclosing_hit(const R01sEntity *e, int bx, int by) {
    int x = e->board_x;
    int y = e->board_y;
    int w = e->body_w;
    int h = e->body_h;
    if (r01s_orient_is_horiz(e->orient)) {
        x -= R01S_CHIP_PIN_OUT;
        w += 2 * R01S_CHIP_PIN_OUT;
    } else {
        y -= R01S_CHIP_PIN_OUT;
        h += 2 * R01S_CHIP_PIN_OUT;
    }
    return board_rect_hit(bx, by, x, y, w, h);
}

/* lx, ly are zoom-divided screen coords, matching the draw (pan already in ui_board_s*). */
int ui_part_image_hit(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    int bx;
    int by;
    if (!ui || !e) {
        return 0;
    }
    ui_zoom_to_board(ui, lx, ly, &bx, &by);
    if (e->visual == R01S_ENTITY_VIS_PASSIVE) {
        return r01s_passive_hit((const R01sPassive *)(const void *)e, bx, by);
    }
    if (e->visual == R01S_ENTITY_VIS_OSC) {
        return ns_osc4legs_hit(e, bx, by);
    }
    if (e->visual == R01S_ENTITY_VIS_PIN_HDR) {
        return ns_pin_header_hit(e, bx, by);
    }
    if (e->visual == R01S_ENTITY_VIS_IC) {
        return ic_enclosing_hit(e, bx, by);
    }
    if (e->visual == R01S_ENTITY_VIS_PWR) {
        int x = ui_board_sx(ui, e->board_x);
        int y = ui_board_sy(ui, e->board_y);
        int ix = x + (e->body_w - R01S_UI_BATTERY_W) / 2;
        int iy = y + 2;
        if (filled_at(lx, ly, x, y, e->body_w, e->body_h) ||
            filled_at(lx, ly, ix, iy, R01S_UI_BATTERY_W, R01S_UI_BATTERY_H)) {
            return 1;
        }
        return glyph_pins_hit(ui, e, lx, ly);
    }
    if (e->visual == R01S_ENTITY_VIS_DISPLAY && e->part && strcmp(e->part, "SCREEN_SINK") == 0) {
        const R01sVideoSink *sink = (const R01sVideoSink *)(e->impl ? e->impl : (void *)e);
        int lcd_w = 0;
        int lcd_h = 0;
        if (sink) {
            r01s_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
        }
        if (board_rect_hit(bx, by, e->board_x, e->board_y, lcd_w, lcd_h)) {
            return 1;
        }
        return glyph_pins_hit(ui, e, lx, ly);
    }
    return board_rect_hit(bx, by, e->board_x, e->board_y, e->body_w, e->body_h);
}

void draw_board_item(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected) {
    if (!e) {
        return;
    }
    switch (e->visual) {
    case R01S_ENTITY_VIS_PWR:
        draw_pwr_glyph(r, ui, e, selected);
        break;
    case R01S_ENTITY_VIS_OSC:
        draw_osc_glyph(r, ui, e, selected);
        break;
    case R01S_ENTITY_VIS_DISPLAY:
        draw_display_glyph(r, ui, e, selected);
        break;
    case R01S_ENTITY_VIS_BUTTON:
        draw_button_glyph(r, ui, e, selected);
        break;
    case R01S_ENTITY_VIS_PANEL:
        draw_panel_glyph(r, ui, e, selected);
        break;
    case R01S_ENTITY_VIS_PIN_HDR:
        ns_pin_header_draw(r, e, ui_board_sx(ui, e->board_x), ui_board_sy(ui, e->board_y), selected);
        if (e->refdes && e->refdes[0]) {
            font_draw(r, ui_board_sx(ui, e->board_x), ui_board_sy(ui, e->board_y) - font_line_h() - 2, e->refdes, 180, 185, 170);
        }
        break;
    case R01S_ENTITY_VIS_BREADBOARD:
        r01s_breadboard_draw(r, (const R01sBreadboard *)(const void *)e, ui_board_sx(ui, e->board_x),
                             ui_board_sy(ui, e->board_y), selected);
        break;
    case R01S_ENTITY_VIS_PASSIVE:
        r01s_passive_draw(r, (const R01sPassive *)(const void *)e,
                          ui_board_sx(ui, ((const R01sPassive *)(const void *)e)->pivot_x),
                          ui_board_sy(ui, ((const R01sPassive *)(const void *)e)->pivot_y), selected);
        break;
    case R01S_ENTITY_VIS_IC:
    default:
        draw_chip(r, ui, e, selected);
        break;
    }
}

void draw_led(SDL_Renderer *r, int x, int y, int on, Uint8 R, Uint8 G, Uint8 B, const char *label) {
    fill_rect(r, x, y, 10, 10, on ? R : 30, on ? G : 30, on ? B : 30);
    draw_rect(r, x, y, 10, 10, 200, 200, 200);
    font_draw(r, x + 14, y + 2, label, 180, 180, 170);
}
