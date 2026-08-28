#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        /* Solid dark — no light center (pad is a filled block). */
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

static void ui_chip_pin_rgb(const R01sUi *ui, R01sLevel lvl, R01sPinDir dir, Uint8 *pr, Uint8 *pg,
                            Uint8 *pb) {
    if (ui && ui->pins_quiet) {
        *pr = R01S_UI_PIN_GRAY_R;
        *pg = R01S_UI_PIN_GRAY_G;
        *pb = R01S_UI_PIN_GRAY_B;
        return;
    }
    pin_level_rgb(lvl, dir, pr, pg, pb);
}

/* rot: 0=N (sprite as-is), 1=E, 2=S, 3=W. Opaque pixels tinted to (tr,tg,tb). */
static void blit_pin_rot(SDL_Renderer *r, int dx, int dy, int rot, Uint8 tr, Uint8 tg, Uint8 tb) {
    int w = R01S_UI_PIN_W;
    int h = R01S_UI_PIN_H;
    int sx, sy, ox, oy, ow, oh;

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
    for (oy = 0; oy < oh; oy++) {
        for (ox = 0; ox < ow; ox++) {
            const uint8_t *p;
            if (rot == 0) {
                sx = ox;
                sy = oy;
            } else if (rot == 1) {
                /* 90° CW: dest(ox,oy) <- src(oy, h-1-ox) */
                sx = oy;
                sy = h - 1 - ox;
            } else if (rot == 2) {
                sx = w - 1 - ox;
                sy = h - 1 - oy;
            } else {
                /* 90° CCW: dest(ox,oy) <- src(w-1-oy, ox) */
                sx = w - 1 - oy;
                sy = ox;
            }
            p = R01S_UI_PIN_RGBA + ((size_t)sy * (size_t)w + (size_t)sx) * 4u;
            if (p[3] == 0) {
                continue;
            }
            SDL_SetRenderDrawColor(r, tr, tg, tb, 255);
            SDL_RenderDrawPoint(r, dx + ox, dy + oy);
        }
    }
}

/* DIP pad from pin.png: tip points away from body. */
static void draw_dip_pad_h(SDL_Renderer *r, int px, int body_edge_y, int outward_down, Uint8 pr, Uint8 pg,
                           Uint8 pb) {
    if (outward_down) {
        blit_pin_rot(r, px - R01S_UI_PIN_W / 2, body_edge_y, 2, pr, pg, pb);
    } else {
        blit_pin_rot(r, px - R01S_UI_PIN_W / 2, body_edge_y - R01S_UI_PIN_H, 0, pr, pg, pb);
    }
}

static void draw_dip_pad_v(SDL_Renderer *r, int py, int body_edge_x, int outward_left, Uint8 pr, Uint8 pg,
                           Uint8 pb) {
    if (outward_left) {
        blit_pin_rot(r, body_edge_x - R01S_UI_PIN_H, py - R01S_UI_PIN_W / 2, 3, pr, pg, pb);
    } else {
        blit_pin_rot(r, body_edge_x, py - R01S_UI_PIN_W / 2, 1, pr, pg, pb);
    }
}

static void draw_glyph_pins(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int board_x, int board_y) {
    int x = ui_board_sx(ui, board_x);
    int li = 0;
    int ri = 0;
    int i;
    for (i = 0; i < e->pin_count; i++) {
        int side_left;
        int idx;
        int py;
        Uint8 pr, pg, pb;
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
        ui_chip_pin_rgb(ui, e->pins[i].level, e->pins[i].dir, &pr, &pg, &pb);
        if (side_left) {
            draw_dip_pad_v(r, py, x, 1, pr, pg, pb);
        } else {
            draw_dip_pad_v(r, py, x + e->body_w, 0, pr, pg, pb);
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
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 80, selected ? 220 : 90, selected ? 80 : 70);
    blit_rgba_scaled(r, ix, iy, R01S_UI_BATTERY_RGBA, R01S_UI_BATTERY_W, R01S_UI_BATTERY_H, 1);
}

static void draw_osc_glyph(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int img_w = R01S_UI_OSC_W;
    int img_h = R01S_UI_OSC_H;
    int ix = x + (e->body_w - img_w) / 2;
    int iy = y + (e->body_h - img_h) / 2 - 2;
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 80, selected ? 220 : 100, selected ? 80 : 110);
    blit_rgba_scaled(r, ix, iy, R01S_UI_OSC_RGBA, R01S_UI_OSC_W, R01S_UI_OSC_H, 1);
}

static void draw_lcd_ctrl_btn(SDL_Renderer *r, const R01sUi *ui, const SDL_Rect *rc, int selected,
                              const char *label) {
    int tw;
    int tx;
    int ty;
    int hot;
    Uint8 bg_a;

    if (!r || !rc || !label) {
        return;
    }
    hot = selected;
    if (ui && ui->mouse_lx >= rc->x && ui->mouse_lx < rc->x + rc->w && ui->mouse_ly >= rc->y &&
        ui->mouse_ly < rc->y + rc->h) {
        hot = 1;
    }
    bg_a = hot ? R01S_LCD_CTRL_BG_A_HOT : R01S_LCD_CTRL_BG_A_IDLE;
    fill_rect_a(r, rc->x, rc->y, rc->w, rc->h, selected ? 36 : 22, selected ? 52 : 30, selected ? 40 : 28,
                bg_a);
    tw = font_text_width(label);
    tx = rc->x + (rc->w - tw) / 2;
    ty = rc->y + (rc->h - font_line_h()) / 2;
    font_draw(r, tx, ty, label, 255, 255, 255);
}

void display_ctrl_btn_rect(const R01sUi *ui, const R01sEntity *e, int btn, SDL_Rect *rc) {
    int x0 = ui_board_sx(ui, e->board_x + R01S_LCD_CTRL_PAD_X);
    int y0 = ui_board_sy(ui, e->board_y + R01S_LCD_CTRL_PAD_Y);
    if (!rc || !e || btn < 0 || btn >= R01S_LCD_CTRL_BTN_N) {
        return;
    }
    rc->x = x0 + btn * (R01S_LCD_CTRL_BTN_W + R01S_LCD_CTRL_GAP);
    rc->y = y0;
    rc->w = R01S_LCD_CTRL_BTN_W;
    rc->h = R01S_LCD_CTRL_BTN_H;
}

static void draw_lcd_ctrl_bar(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e,
                              const R01sVideoSink *sink) {
    static const char *const labels[R01S_LCD_CTRL_BTN_N] = {"PRS", "PHO", "1X", "2X"};
    SDL_Rect rc;
    int mode;
    int scale2;
    int i;

    if (!r || !ui || !e || !sink) {
        return;
    }
    mode = r01s_video_sink_render_mode(sink);
    scale2 = r01s_video_sink_scale_2x(sink);
    for (i = 0; i < R01S_LCD_CTRL_BTN_N; i++) {
        int sel = (i < 2) ? (mode == i + 1) : (i == 2 ? !scale2 : scale2);
        display_ctrl_btn_rect(ui, e, i, &rc);
        draw_lcd_ctrl_btn(r, ui, &rc, sel, labels[i]);
    }
}

static void draw_display_glyph(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    const R01sVideoSink *sink = (const R01sVideoSink *)e;
    int lcd_w;
    int lcd_h;

    r01s_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    draw_video_pixels(r, ui, (R01sVideoSink *)(void *)sink, x, y, lcd_w, lcd_h);
    draw_lcd_ctrl_bar(r, ui, e, sink);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, 255, 220, 80);
    }
}

/* Pin along-axis board coord + which side (1 = pin1 side: bottom if H, left if V).
 * Pitch is always JEDEC 0.100″ (R01S_DIP_PIN_PITCH_PX); end margin centers the row. */
static void dip_pin_pos(const R01sEntity *e, int pin_num, int *along, int *side_pin1) {
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int half = dip / 2;
    int idx;
    int span;
    int pitch = R01S_DIP_PIN_PITCH_PX;
    int row_span;
    int margin;

    if (dip <= 0 || pin_num <= 0 || pin_num > dip) {
        *side_pin1 = 1;
        *along = (e->orient == R01S_ORIENT_H) ? (e->body_w / 2) : (e->body_h / 2);
        return;
    }
    *side_pin1 = pin_num <= half;
    idx = *side_pin1 ? (pin_num - 1) : (dip - pin_num);
    span = (e->orient == R01S_ORIENT_H) ? e->body_w : e->body_h;
    row_span = (half > 1) ? (half - 1) * pitch : 0;
    margin = (span - row_span) / 2;
    if (margin < 1) {
        margin = 1;
    }
    *along = margin + idx * pitch;
}

static void draw_chip(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int i;
    int horiz = (e->orient != R01S_ORIENT_V);
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;

    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int along;
        int side_pin1;
        Uint8 pr, pg, pb;
        /* Only package outline pins — logical pins beyond dip_pins used to share one
         * slot (often the center) and crush spacing (e.g. ATF22V10 beam-Y extras). */
        if (num < 1 || num > dip) {
            continue;
        }
        dip_pin_pos(e, num, &along, &side_pin1);
        ui_chip_pin_rgb(ui, e->pins[i].level, e->pins[i].dir, &pr, &pg, &pb);
        if (horiz) {
            draw_dip_pad_h(r, x + along, side_pin1 ? (y + e->body_h) : y, side_pin1, pr, pg, pb);
        } else {
            draw_dip_pad_v(r, y + along, side_pin1 ? x : (x + e->body_w), side_pin1, pr, pg, pb);
        }
    }

    fill_rect(r, x, y, e->body_w, e->body_h, selected ? 40 : 28, selected ? 48 : 32, selected ? 36 : 28);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, 255, 220, 80);
    }
    /* Notch: left when horizontal, top when vertical. */
    if (horiz) {
        fill_rect(r, x - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
    } else {
        fill_rect(r, x + e->body_w / 2 - 2, y - 1, 4, 2, 20, 22, 20);
    }
    /* Part label centered on body, 50% opaque white. Skip if it won't fit fully.
     * Vertical packages get the label rotated 90° CW (reads top→bottom). */
    {
        const char *label = e->part ? e->part : e->refdes;
        if (label && label[0]) {
            int fh = font_line_h();
            int tw = font_text_width(label);
            if (horiz) {
                if (tw <= e->body_w - 4 && fh <= e->body_h - 4) {
                    font_draw_a(r, x + (e->body_w - tw) / 2, y + (e->body_h - fh) / 2, label, 255, 255, 255, 128);
                }
            } else if (tw <= e->body_h - 4 && fh <= e->body_w - 4) {
                /* Rotated AABB is fh wide × tw tall. */
                font_draw_a_rot90ccw(r, x + (e->body_w - fh) / 2, y + (e->body_h - tw) / 2, label, 255, 255, 255,
                                     128);
            }
        }
    }
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

static void draw_pad_bits(SDL_Renderer *r, int x, int y, uint8_t bits) {
    static const char *names[8] = {"R", "L", "D", "U", "X", "Y", "C", "S"};
    int i;
    for (i = 0; i < 8; i++) {
        int on = (bits & (1u << i)) != 0;
        fill_rect(r, x + i * 14, y, 12, 12, on ? 80 : 30, on ? 200 : 30, on ? 100 : 30);
        draw_rect(r, x + i * 14, y, 12, 12, 120, 130, 120);
        font_draw(r, x + i * 14 + 3, y + 2, names[i], 200, 210, 200);
    }
}
