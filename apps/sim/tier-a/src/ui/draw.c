#include "r01a_ui.h"

#include "ui_font.h"

#include "discrete_ic/types.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderFillRect(r, &rc);
}

void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawRect(r, &rc);
}

void plot_a(SDL_Renderer *r, int x, int y, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca) {
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

void draw_soft_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb,
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

void draw_hard_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDrawLine(r, x0, y0, x1, y1);
}

void draw_ants_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint32 now) {
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

void draw_ic(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected, int hover_pin) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int bw = e->body_w;
    int bh = e->body_h;
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

void draw_passive_glyph(SDL_Renderer *r, const R01aUi *ui, NsEntity *e, int selected, int hover_pin) {
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

void mode_btn_rect(const R01aUi *ui, SDL_Rect *rc) {
    int tw = r01a_font_text_width(mode_label(ui));
    rc->x = 4;
    rc->y = 2;
    rc->w = tw + 12;
    rc->h = r01a_font_line_h() + 4;
}

void draw_mode_btn(SDL_Renderer *r, const R01aUi *ui) {
    SDL_Rect rc;
    mode_btn_rect(ui, &rc);
    fill_rect(r, rc.x, rc.y, rc.w, rc.h, 18, 18, 22);
    draw_rect(r, rc.x, rc.y, rc.w, rc.h, ui->show_nets ? 220 : 160, ui->show_nets ? 180 : 160, 80);
    r01a_font_draw(r, rc.x + 6, rc.y + 2, mode_label(ui), 230, 230, 200);
}

void draw_legend(SDL_Renderer *r) {
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

void draw_tooltip(SDL_Renderer *r, int lx, int ly, const char *text) {
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

static const char *part_label(const NsEntity *e) {
    if (!e) {
        return "?";
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        const NsPassive *p = (const NsPassive *)e;
        if (p->value[0]) {
            return p->value;
        }
    }
    if (e->visual == NS_ENTITY_VIS_DISPLAY) {
        return "Screen";
    }
    if (e->visual == NS_ENTITY_VIS_PIN_HDR) {
        return "Header";
    }
    return e->part ? e->part : "?";
}

void fill_tooltip(const R01aUi *ui, char *out, size_t out_len) {
    const NsEntity *e;
    const char *ref;
    const char *part;
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (ui->hover_chip < 0 || ui->hover_chip >= ui->chip_count) {
        return;
    }
    e = ui->chips[ui->hover_chip];
    if (!e) {
        return;
    }
    ref = e->refdes ? e->refdes : "?";
    part = part_label(e);
    if (ui->hover_pin >= 0 && ui->hover_pin < e->pin_count && e->pins[ui->hover_pin].name) {
        snprintf(out, out_len, "%s  %s  %s", ref, part, e->pins[ui->hover_pin].name);
        return;
    }
    snprintf(out, out_len, "%s  %s", ref, part);
}

static void lcd_view_rect(const R01aUi *ui, const NsVideoSink *sink, int lcd_w, int lcd_h, SDL_Rect *dst) {
    int z = canvas_zoom(ui);
    dst->x = board_sx(ui, sink->base.board_x) * z;
    dst->y = board_sy(ui, sink->base.board_y) * z;
    dst->w = lcd_w * z;
    dst->h = lcd_h * z;
}

static void draw_lcd_bezel(SDL_Renderer *r, const R01aUi *ui, NsVideoSink *sink, int selected, int lcd_w,
                           int lcd_h) {
    SDL_Rect dst;
    float sx;
    float sy;
    SDL_RenderGetScale(r, &sx, &sy);
    SDL_RenderSetScale(r, 1.0f, 1.0f);
    lcd_view_rect(ui, sink, lcd_w, lcd_h, &dst);
    fill_rect(r, dst.x, dst.y, dst.w, dst.h, 0, 0, 0);
    draw_rect(r, dst.x - 1, dst.y - 1, dst.w + 2, dst.h + 2, selected ? R01A_SEL_R : 180,
              selected ? R01A_SEL_G : 180, selected ? R01A_SEL_B : 180);
    SDL_RenderSetScale(r, sx, sy);
}

void draw_lcd(SDL_Renderer *r, R01aUi *ui, NsVideoSink *sink, int selected) {
    const uint8_t *rgb;
    SDL_Rect dst;
    int lcd_w;
    int lcd_h;
    float sx;
    float sy;
    ns_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
    rgb = ns_video_sink_rgb(sink);
    if (!rgb || !screen_picture_live(ui)) {
        draw_lcd_bezel(r, ui, sink, selected, lcd_w, lcd_h);
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
        draw_lcd_bezel(r, ui, sink, selected, lcd_w, lcd_h);
        return;
    }
    SDL_UpdateTexture(ui->lcd_tex, NULL, rgb, NS_VIDEO_W * 3);
    SDL_RenderGetScale(r, &sx, &sy);
    SDL_RenderSetScale(r, 1.0f, 1.0f);
    lcd_view_rect(ui, sink, lcd_w, lcd_h, &dst);
    SDL_RenderCopy(r, ui->lcd_tex, NULL, &dst);
    draw_rect(r, dst.x - 1, dst.y - 1, dst.w + 2, dst.h + 2, selected ? R01A_SEL_R : 180,
              selected ? R01A_SEL_G : 180, selected ? R01A_SEL_B : 180);
    SDL_RenderSetScale(r, sx, sy);
}

void update_scale(R01aUi *ui) {
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

void logic_from_window(const R01aUi *ui, int wx, int wy, int *lx, int *ly) {
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

void present_frame(R01aUi *ui) {
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

void draw_frame(R01aUi *ui, R01aBoard *board) {
    int rank;
    (void)board;
    copper_rebuild(ui);
    SDL_SetRenderTarget(ui->rend, ui->target);
    SDL_SetRenderDrawColor(ui->rend, 0, 0, 0, 255);
    SDL_RenderClear(ui->rend);
    SDL_RenderSetScale(ui->rend, (float)canvas_zoom(ui), (float)canvas_zoom(ui));

    /* Bottom → top: LCD, breadboards, parts, then wires/jumpers. */
    for (rank = 0; rank < ui->chip_count; rank++) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (e && e->visual == NS_ENTITY_VIS_DISPLAY) {
            draw_lcd(ui->rend, ui, (NsVideoSink *)e, ci == ui->selected);
        }
    }

    draw_breadboards(ui->rend, ui, board);

    for (rank = 0; rank < ui->chip_count; rank++) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        int sel;
        int hp;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        sel = ci == ui->selected;
        hp = (ci == ui->hover_chip) ? ui->hover_pin : -1;
        if (is_passive_glyph(e)) {
            draw_passive_glyph(ui->rend, ui, e, sel, hp);
        } else {
            draw_ic(ui->rend, ui, e, sel, hp);
        }
    }

    draw_jumpers(ui->rend, ui, board);
    if (ui->show_nets) {
        draw_air_wires(ui->rend, ui);
#if R01A_COPPER_TRACES
        {
            int mx;
            int my;
            draw_traces(ui->rend, ui);
            logic_to_board(ui, ui->mouse_lx, ui->mouse_ly, &mx, &my);
            draw_arm(ui->rend, ui, mx, my);
        }
#endif
    }

    SDL_RenderSetScale(ui->rend, 1.0f, 1.0f);
    draw_mode_btn(ui->rend, ui);
    draw_legend(ui->rend);
    {
        char tip[96];
        fill_tooltip(ui, tip, sizeof(tip));
        if (tip[0]) {
            draw_tooltip(ui->rend, ui->mouse_lx, ui->mouse_ly, tip);
        }
    }
    present_frame(ui);
}
