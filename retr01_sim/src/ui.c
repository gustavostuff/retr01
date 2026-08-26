#include "ui.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clamp_chip_in_island(R01sUi *ui, R01sEntity *e, int island_index);
static void clamp_chip(R01sUi *ui, R01sEntity *e, int island_index);
static void ui_save_island_layout(R01sUi *ui);
static void ui_save_compact_layout(R01sUi *ui);
static void ui_restore_island_layout(R01sUi *ui);
static int ui_island_snapshot_valid(const R01sUi *ui);
static void ui_arrange_islands_default(R01sUi *ui);
static void ui_tighten_island_to_chips(R01sUi *ui, int island_index);
static void ui_pack_island_chips(R01sUi *ui, int island_index);
static void ui_row_place_islands(R01sUi *ui);
static int island_saved_chip_layout_sane(const R01sUi *ui, int island_index);
static void island_expand_for_saved_chips(R01sUi *ui, int island_index);
static int island_chips_overlap(const R01sUi *ui, int island_index);
static void ui_rebuild_islands_from_saved_chips(R01sUi *ui);
static void ui_chip_rel_from_abs(const R01sUi *ui, int chip_i, int abs_x, int abs_y, int *rx, int *ry);
static void ui_chip_place_rel(R01sUi *ui, int chip_i, int rx, int ry);
static void island_content_min_size(const R01sUi *ui, int island_index, int *min_w, int *min_h);
static void draw_video_pixels(SDL_Renderer *r, R01sUi *ui, R01sVideoSink *sink, int px, int py);
static void ui_toggle_compact(R01sUi *ui);
static void compact_btn_rect(const R01sUi *ui, SDL_Rect *rc);
static void save_btn_rect(const R01sUi *ui, SDL_Rect *rc);
static void ui_save_layout_now(R01sUi *ui);
static void ui_toggle_lcd_scale(R01sUi *ui);
static void ui_set_lcd_scale(R01sUi *ui, int scale_2x);
static void ui_set_screen_render_mode(R01sUi *ui, int mode);
static void ui_set_sim_fast(R01sUi *ui, int enable);
static int ui_lcd_scale_2x(const R01sUi *ui);
static int ui_screen_render_mode(const R01sUi *ui);

static int ui_board_sx(const R01sUi *ui, int board_x) {
    return R01S_UI_VIEW_X + board_x - ui->pan_x;
}

static int ui_board_sy(const R01sUi *ui, int board_y) {
    return R01S_UI_VIEW_Y + board_y - ui->pan_y;
}

static int ui_logic_in_view(int lx, int ly) {
    return lx >= R01S_UI_VIEW_X && lx < R01S_UI_VIEW_X + R01S_UI_VIEW_W && ly >= R01S_UI_VIEW_Y &&
           ly < R01S_UI_VIEW_Y + R01S_UI_VIEW_H;
}

static void ui_logic_to_board(const R01sUi *ui, int lx, int ly, int *bx, int *by) {
    *bx = lx - R01S_UI_VIEW_X + ui->pan_x;
    *by = ly - R01S_UI_VIEW_Y + ui->pan_y;
}

/* Minimal 5x7 digits/letters for labels (subset). */
static const uint8_t FONT[36][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
};

static int glyph_ix(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return 10 + (c - 'a');
    }
    return -1;
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

static void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    int cx = x;
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    for (; text && *text; text++) {
        int gi = glyph_ix(*text);
        int row, col;
        if (*text == '.') {
            SDL_RenderDrawPoint(r, cx + 2, y + 6);
            cx += 6;
            continue;
        }
        if (*text == ' ' || *text == '/' || *text == '+' || *text == '-' || *text == '#' || *text == ':') {
            cx += 6;
            continue;
        }
        if (gi < 0) {
            cx += 6;
            continue;
        }
        for (row = 0; row < 7; row++) {
            uint8_t bits = FONT[gi][row];
            for (col = 0; col < 5; col++) {
                if (bits & (0x10 >> col)) {
                    SDL_RenderDrawPoint(r, cx + col, y + row);
                }
            }
        }
        cx += 6;
    }
}

static int font_text_width(const char *text) {
    int n = 0;
    for (; text && *text; text++) {
        n += 6;
    }
    return n;
}

/* Draw text clipped to max_w; append "..." when truncated. Returns 1 if truncated. */
static int font_draw_ellipsize(SDL_Renderer *r, int x, int y, const char *text, int max_w, Uint8 R, Uint8 G,
                               Uint8 B) {
    char buf[96];
    int full_w;
    int max_chars;
    int i;

    if (!text || max_w < 6) {
        return 0;
    }
    full_w = font_text_width(text);
    if (full_w <= max_w) {
        font_draw(r, x, y, text, R, G, B);
        return 0;
    }
    max_chars = (max_w - 18) / 6; /* room for "..." */
    if (max_chars < 1) {
        max_chars = 1;
    }
    if (max_chars > (int)sizeof(buf) - 4) {
        max_chars = (int)sizeof(buf) - 4;
    }
    for (i = 0; i < max_chars && text[i]; i++) {
        buf[i] = text[i];
    }
    buf[i++] = '.';
    buf[i++] = '.';
    buf[i++] = '.';
    buf[i] = '\0';
    font_draw(r, x, y, buf, R, G, B);
    return 1;
}

int r01s_ui_init(R01sUi *ui) {
    if (!ui) {
        return -1;
    }
    memset(ui, 0, sizeof(*ui));
    ui->selected = -1;
    ui->drag_chip = -1;
    ui->drag_island = -1;
    ui->resize_island = -1;
    ui->drag_stick = -1;
    ui->drag_btn = -1;
    ui->ctx_chip = -1;
    ui->box_sel = 0;
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    snprintf(ui->status, sizeof(ui->status),
             "SPACE pause. S save layout. R rotate. Sidebar: SCREEN / SCALE / SIM. COMPACT/ISLANDS");
    return 0;
}

void r01s_ui_shutdown(R01sUi *ui) {
    if (ui) {
        if (ui->lcd_tex) {
            SDL_DestroyTexture(ui->lcd_tex);
            ui->lcd_tex = NULL;
        }
        memset(ui, 0, sizeof(*ui));
    }
}

int r01s_ui_rotate_selected(R01sUi *ui) {
    int i;
    int n = 0;
    const char *last_ref = NULL;
    R01sPkgOrient last_orient = R01S_ORIENT_H;

    if (!ui) {
        return 0;
    }

    /* Compact multi-select: rotate every selected IC. */
    if (ui->layout_compact) {
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *te;
            if (!ui->chip_sel[i]) {
                continue;
            }
            te = ui->chips[i];
            if (!te || te->visual != R01S_ENTITY_VIS_IC) {
                continue;
            }
            r01s_entity_set_orient(te, te->orient == R01S_ORIENT_V ? R01S_ORIENT_H : R01S_ORIENT_V);
            clamp_chip(ui, te, ui->chip_island[i]);
            last_ref = te->refdes;
            last_orient = te->orient;
            n++;
        }
        if (n > 0) {
            ui->layout_dirty = 1;
            if (n == 1) {
                snprintf(ui->status, sizeof(ui->status), "%s → %s", last_ref ? last_ref : "?",
                         last_orient == R01S_ORIENT_V ? "VERTICAL" : "HORIZONTAL");
            } else {
                snprintf(ui->status, sizeof(ui->status), "rotated %d chips", n);
            }
            return 1;
        }
    }

    {
        R01sEntity *te;
        int idx = ui->selected;
        if (idx < 0 || idx >= ui->chip_count) {
            idx = ui->ctx_chip;
        }
        if (idx < 0 || idx >= ui->chip_count) {
            return 0;
        }
        te = ui->chips[idx];
        if (!te || te->visual != R01S_ENTITY_VIS_IC) {
            return 0;
        }
        r01s_entity_set_orient(te, te->orient == R01S_ORIENT_V ? R01S_ORIENT_H : R01S_ORIENT_V);
        clamp_chip(ui, te, ui->chip_island[idx]);
        ui->layout_dirty = 1;
        snprintf(ui->status, sizeof(ui->status), "%s → %s", te->refdes ? te->refdes : "?",
                 te->orient == R01S_ORIENT_V ? "VERTICAL" : "HORIZONTAL");
        return 1;
    }
}

void r01s_ui_bind_group(R01sUi *ui, R01sIslandGroup *group) {
    if (ui) {
        ui->group = group;
    }
}

int r01s_ui_add_chip(R01sUi *ui, R01sEntity *chip, int island_index) {
    if (!ui || !chip || ui->chip_count >= R01S_BOARD_MAX_CHIPS) {
        return -1;
    }
    if (!ui->group || island_index < 0 || island_index >= r01s_island_group_count(ui->group)) {
        return -1;
    }
    ui->chips[ui->chip_count] = chip;
    ui->chip_island[ui->chip_count] = (uint8_t)island_index;
    clamp_chip(ui, chip, island_index);
    ui->chip_count++;
    return 0;
}

void r01s_ui_clamp_pan(R01sUi *ui) {
    int max_x = R01S_BOARD_W - R01S_UI_VIEW_W;
    int max_y = R01S_BOARD_H - R01S_UI_VIEW_H;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (ui->pan_x < 0) {
        ui->pan_x = 0;
    }
    if (ui->pan_y < 0) {
        ui->pan_y = 0;
    }
    if (ui->pan_x > max_x) {
        ui->pan_x = max_x;
    }
    if (ui->pan_y > max_y) {
        ui->pan_y = max_y;
    }
}

static void pin_level_rgb(R01sLevel lvl, R01sPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
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

/* DIP pad: colored stub only (no terminal square). */
static void draw_dip_pad_h(SDL_Renderer *r, int px, int body_edge_y, int outward_down, Uint8 pr, Uint8 pg,
                           Uint8 pb) {
    int stub0;
    int stub1;
    if (outward_down) {
        stub0 = body_edge_y;
        stub1 = body_edge_y + 9;
    } else {
        stub0 = body_edge_y - 9;
        stub1 = body_edge_y;
    }
    SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
    SDL_RenderDrawLine(r, px, stub0, px, stub1);
}

static void draw_dip_pad_v(SDL_Renderer *r, int py, int body_edge_x, int outward_left, Uint8 pr, Uint8 pg,
                           Uint8 pb) {
    int stub0;
    int stub1;
    if (outward_left) {
        stub0 = body_edge_x - 9;
        stub1 = body_edge_x;
    } else {
        stub0 = body_edge_x;
        stub1 = body_edge_x + 9;
    }
    SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
    SDL_RenderDrawLine(r, stub0, py, stub1, py);
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
        int px0;
        int px1;
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
        py = board_y + 10 + idx * 10;
        if (py > board_y + e->body_h - 6) {
            py = board_y + e->body_h - 6;
        }
        py = ui_board_sy(ui, py);
        pin_level_rgb(e->pins[i].level, e->pins[i].dir, &pr, &pg, &pb);
        if (side_left) {
            px0 = x - 10;
            px1 = x;
        } else {
            px0 = x + e->body_w;
            px1 = x + e->body_w + 10;
        }
        SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
        SDL_RenderDrawLine(r, px0, py, px1, py);
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

static void draw_pwr_glyph(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int img_w = R01S_UI_BATTERY_W * 2;
    int ix = x + (e->body_w - img_w) / 2;
    int iy = y + 4;
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 80, selected ? 220 : 90, selected ? 80 : 70);
    blit_rgba_scaled(r, ix, iy, R01S_UI_BATTERY_RGBA, R01S_UI_BATTERY_W, R01S_UI_BATTERY_H, 2);
    if (e->refdes) {
        font_draw_ellipsize(r, x + 4, y + e->body_h - 12, e->refdes, e->body_w - 8, 180, 190, 160);
    }
}

static void draw_osc_glyph(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int img_w = R01S_UI_OSC_W * 2;
    int img_h = R01S_UI_OSC_H * 2;
    int ix = x + (e->body_w - img_w) / 2;
    int iy = y + (e->body_h - img_h) / 2 - 4;
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 80, selected ? 220 : 100, selected ? 80 : 110);
    blit_rgba_scaled(r, ix, iy, R01S_UI_OSC_RGBA, R01S_UI_OSC_W, R01S_UI_OSC_H, 2);
    if (e->refdes) {
        font_draw_ellipsize(r, x + 4, y + 4, e->refdes, e->body_w - 8, 180, 190, 170);
    }
    if (e->part) {
        font_draw_ellipsize(r, x + 4, y + e->body_h - 12, e->part, e->body_w - 8, 160, 180, 200);
    }
}

static void draw_display_glyph(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int inner_x = x + 4;
    int inner_y = y + 8;
    int inner_w = e->body_w - 8;
    int inner_h = e->body_h - 14;
    int hdr_h = 14;
    int px = inner_x + (inner_w - R01S_VIDEO_W) / 2;
    int py = inner_y + hdr_h;
    const R01sVideoSink *sink = (const R01sVideoSink *)e;
    char scale_lbl[16];
    int scale_w;
    draw_glyph_pins(r, ui, e, e->board_x, e->board_y);
    fill_rect(r, inner_x, inner_y, inner_w, inner_h, 16, 22, 18);
    draw_rect(r, inner_x, inner_y, inner_w, inner_h, selected ? 255 : 140, selected ? 220 : 160,
              selected ? 80 : 180);
    /* Header band — centered title, SCALE right-aligned (clear of left-side pins). */
    font_draw(r, inner_x + (inner_w - font_text_width("SCREEN 256X240")) / 2, inner_y + 3, "SCREEN 256X240",
              180, 190, 160);
    snprintf(scale_lbl, sizeof(scale_lbl), "SCALE %s",
             r01s_video_sink_scale_2x(sink) ? "2X" : "1X");
    scale_w = font_text_width(scale_lbl);
    font_draw(r, inner_x + inner_w - scale_w - 4, inner_y + 3, scale_lbl, 140, 200, 160);
    fill_rect(r, inner_x + 2, inner_y + hdr_h - 1, inner_w - 4, 1, 40, 55, 45);
    fill_rect(r, px, py, R01S_VIDEO_W, R01S_VIDEO_H, 8, 12, 16);
    draw_video_pixels(r, ui, (R01sVideoSink *)(void *)sink, px, py);
    draw_rect(r, px - 1, py - 1, R01S_VIDEO_W + 2, R01S_VIDEO_H + 2, 80, 100, 120);
}

static void font_draw_v(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B) {
    /* 90° CW: each 5x7 glyph reads top→bottom along +Y. */
    int cy = y;
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    for (; text && *text; text++) {
        int gi = glyph_ix(*text);
        int row, col;
        if (*text == ' ' || *text == '/' || *text == '+' || *text == '-' || *text == '#' || *text == ':' ||
            *text == '.') {
            cy += 6;
            continue;
        }
        if (gi < 0) {
            cy += 6;
            continue;
        }
        for (row = 0; row < 7; row++) {
            uint8_t bits = FONT[gi][row];
            for (col = 0; col < 5; col++) {
                if (bits & (0x10 >> col)) {
                    /* (col,row) → (row, 4-col) */
                    SDL_RenderDrawPoint(r, x + row, cy + (4 - col));
                }
            }
        }
        cy += 6;
    }
}

static int font_text_height(const char *text) {
    return font_text_width(text); /* same advance when rotated */
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
    int label_max;
    SDL_Rect body_clip;
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
        pin_level_rgb(e->pins[i].level, e->pins[i].dir, &pr, &pg, &pb);
        if (horiz) {
            draw_dip_pad_h(r, x + along, side_pin1 ? (y + e->body_h) : y, side_pin1, pr, pg, pb);
        } else {
            draw_dip_pad_v(r, y + along, side_pin1 ? x : (x + e->body_w), side_pin1, pr, pg, pb);
        }
    }

    fill_rect(r, x, y, e->body_w, e->body_h, 28, 32, 28);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 140, selected ? 220 : 140,
              selected ? 80 : 120);
    /* Notch: left when horizontal, top when vertical. */
    if (horiz) {
        fill_rect(r, x - 2, y + e->body_h / 2 - 4, 4, 8, 20, 22, 20);
    } else {
        fill_rect(r, x + e->body_w / 2 - 4, y - 2, 8, 4, 20, 22, 20);
    }

    body_clip.x = x + 2;
    body_clip.y = y + 2;
    body_clip.w = e->body_w - 4;
    body_clip.h = e->body_h - 4;
    SDL_RenderSetClipRect(r, &body_clip);
    if (horiz) {
        label_max = e->body_w - 12;
        if (e->refdes) {
            font_draw_ellipsize(r, x + 6, y + e->body_h / 2 - 10, e->refdes, label_max, 220, 220, 200);
        }
        if (e->part) {
            font_draw_ellipsize(r, x + 6, y + e->body_h / 2 + 2, e->part, label_max, 160, 180, 140);
        }
    } else {
        label_max = e->body_h - 12;
        if (e->refdes) {
            font_draw_v(r, x + e->body_w / 2 - 10, y + 6, e->refdes, 220, 220, 200);
        }
        if (e->part) {
            font_draw_v(r, x + e->body_w / 2 + 2, y + 6, e->part, 160, 180, 140);
        }
        (void)label_max;
        (void)font_text_height;
    }
    {
        SDL_Rect view_clip = {R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H};
        SDL_RenderSetClipRect(r, &view_clip);
    }
}

static void draw_board_item(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected) {
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

static void draw_led(SDL_Renderer *r, int x, int y, int on, Uint8 R, Uint8 G, Uint8 B, const char *label) {
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

static void clamp_chip_to_board(R01sEntity *e) {
    int min_x = R01S_CHIP_PIN_OUT;
    int min_y = R01S_CHIP_PIN_OUT;
    int max_x;
    int max_y;
    int bx, by;

    if (!e) {
        return;
    }
    max_x = R01S_BOARD_W - R01S_CHIP_PIN_OUT - e->body_w;
    max_y = R01S_BOARD_H - R01S_CHIP_PIN_OUT - e->body_h;
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
    bx = r01s_grid_snap(bx);
    by = r01s_grid_snap(by);
    if (bx < min_x) {
        bx = r01s_grid_snap_up(min_x);
    }
    if (by < min_y) {
        by = r01s_grid_snap_up(min_y);
    }
    if (bx > max_x) {
        bx = r01s_grid_snap(max_x);
    }
    if (by > max_y) {
        by = r01s_grid_snap(max_y);
    }
    r01s_entity_place(e, bx, by);
}

static void clamp_chip_in_island(R01sUi *ui, R01sEntity *e, int island_index) {
    R01sIsland *island;
    int min_x, min_y, max_x, max_y;
    int bx, by;

    if (!ui || !e) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    min_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
    min_y = island->board_y + R01S_ISLAND_PAD_TOP;
    max_x = island->board_x + island->board_w - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT - e->body_w;
    max_y = island->board_y + island->board_h - R01S_ISLAND_PAD_BOTTOM - e->body_h;
    /* Grow the frame instead of collapsing every chip onto the same center. */
    if (max_x < min_x) {
        int need = r01s_grid_snap_up(e->body_w + 2 * R01S_CHIP_PIN_OUT + 2 * R01S_ISLAND_PAD_X);
        if (island->board_w < need) {
            island->board_w = need;
        }
        min_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
        max_x = island->board_x + island->board_w - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT - e->body_w;
        if (max_x < min_x) {
            min_x = max_x = island->board_x + (island->board_w - e->body_w) / 2;
        }
    }
    if (max_y < min_y) {
        int need = r01s_grid_snap_up(e->body_h + R01S_ISLAND_PAD_TOP + R01S_ISLAND_PAD_BOTTOM);
        if (island->board_h < need) {
            island->board_h = need;
        }
        min_y = island->board_y + R01S_ISLAND_PAD_TOP;
        max_y = island->board_y + island->board_h - R01S_ISLAND_PAD_BOTTOM - e->body_h;
        if (max_y < min_y) {
            min_y = max_y = island->board_y + (island->board_h - e->body_h) / 2;
        }
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
    bx = r01s_grid_snap(bx);
    by = r01s_grid_snap(by);
    if (bx < min_x) {
        bx = r01s_grid_snap_up(min_x);
    }
    if (by < min_y) {
        by = r01s_grid_snap_up(min_y);
    }
    if (bx > max_x) {
        bx = r01s_grid_snap(max_x);
    }
    if (by > max_y) {
        by = r01s_grid_snap(max_y);
    }
    r01s_entity_place(e, bx, by);
}

static void clamp_chip(R01sUi *ui, R01sEntity *e, int island_index) {
    if (!ui) {
        return;
    }
    if (ui->layout_compact) {
        clamp_chip_to_board(e);
    } else {
        clamp_chip_in_island(ui, e, island_index);
    }
}

static void move_chip_drag(R01sUi *ui, int chip_i, int board_mx, int board_my) {
    R01sEntity *e = ui->chips[chip_i];
    r01s_entity_place(e, board_mx - ui->drag_grab_bx, board_my - ui->drag_grab_by);
    clamp_chip(ui, e, ui->chip_island[chip_i]);
}

static void ui_sel_clear(R01sUi *ui) {
    if (!ui) {
        return;
    }
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    ui->selected = -1;
}

static int ui_sel_count(const R01sUi *ui) {
    int i;
    int n = 0;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_sel[i]) {
            n++;
        }
    }
    return n;
}

static void ui_sel_set_one(R01sUi *ui, int chip_i) {
    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    ui->chip_sel[chip_i] = 1;
    ui->selected = chip_i;
}

static void ui_sel_toggle(R01sUi *ui, int chip_i) {
    int i;
    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
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

static int chip_board_intersects_box(const R01sEntity *e, int x0, int y0, int x1, int y1) {
    int l, t, r, b;
    int el, et, er, eb;
    if (!e || e->visual == R01S_ENTITY_VIS_NONE) {
        return 0;
    }
    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    el = e->board_x;
    et = e->board_y;
    er = e->board_x + e->body_w;
    eb = e->board_y + e->body_h;
    l = x0;
    t = y0;
    r = x1;
    b = y1;
    return el < r && er > l && et < b && eb > t;
}

static void ui_sel_from_box(R01sUi *ui, int additive) {
    int i;
    int first = -1;
    if (!ui) {
        return;
    }
    if (!additive) {
        memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
        ui->selected = -1;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!chip_board_intersects_box(e, ui->box_bx0, ui->box_by0, ui->box_bx1, ui->box_by1)) {
            continue;
        }
        ui->chip_sel[i] = 1;
        if (first < 0) {
            first = i;
        }
    }
    if (first >= 0) {
        ui->selected = first;
    } else if (!additive) {
        ui->selected = -1;
    }
}

static void ui_begin_sel_drag(R01sUi *ui, int board_mx, int board_my) {
    int i;
    if (!ui) {
        return;
    }
    ui->sel_drag_ox = board_mx;
    ui->sel_drag_oy = board_my;
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        ui->sel_start_x[i] = e ? e->board_x : 0;
        ui->sel_start_y[i] = e ? e->board_y : 0;
    }
}

static void move_selection_drag(R01sUi *ui, int board_mx, int board_my) {
    int i;
    int dx;
    int dy;
    int dx_lo = -0x3fffffff;
    int dx_hi = 0x3fffffff;
    int dy_lo = -0x3fffffff;
    int dy_hi = 0x3fffffff;
    int any = 0;

    if (!ui) {
        return;
    }
    dx = r01s_grid_snap(board_mx - ui->sel_drag_ox);
    dy = r01s_grid_snap(board_my - ui->sel_drag_oy);

    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        int min_x, min_y, max_x, max_y;
        if (!ui->chip_sel[i]) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        any = 1;
        min_x = R01S_CHIP_PIN_OUT;
        min_y = R01S_CHIP_PIN_OUT;
        max_x = R01S_BOARD_W - R01S_CHIP_PIN_OUT - e->body_w;
        max_y = R01S_BOARD_H - R01S_CHIP_PIN_OUT - e->body_h;
        if (max_x < min_x) {
            max_x = min_x;
        }
        if (max_y < min_y) {
            max_y = min_y;
        }
        if (min_x - ui->sel_start_x[i] > dx_lo) {
            dx_lo = min_x - ui->sel_start_x[i];
        }
        if (max_x - ui->sel_start_x[i] < dx_hi) {
            dx_hi = max_x - ui->sel_start_x[i];
        }
        if (min_y - ui->sel_start_y[i] > dy_lo) {
            dy_lo = min_y - ui->sel_start_y[i];
        }
        if (max_y - ui->sel_start_y[i] < dy_hi) {
            dy_hi = max_y - ui->sel_start_y[i];
        }
    }
    if (!any) {
        return;
    }
    if (dx < dx_lo) {
        dx = dx_lo;
    }
    if (dx > dx_hi) {
        dx = dx_hi;
    }
    if (dy < dy_lo) {
        dy = dy_lo;
    }
    if (dy > dy_hi) {
        dy = dy_hi;
    }
    dx = r01s_grid_snap(dx);
    dy = r01s_grid_snap(dy);
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        if (!ui->chip_sel[i]) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        r01s_entity_place(e, ui->sel_start_x[i] + dx, ui->sel_start_y[i] + dy);
    }
}

typedef struct {
    int idx;
    int pw;
    int ph;
} R01sPackItem;

static void chip_pack_footprint(const R01sEntity *e, int *pw, int *ph) {
    if (!e || !pw || !ph) {
        return;
    }
    /* Pin stubs stick out of the long sides of a DIP (H: L/R, V: T/B). */
    if (e->visual == R01S_ENTITY_VIS_IC && e->orient == R01S_ORIENT_V) {
        *pw = e->body_w;
        *ph = e->body_h + 2 * R01S_CHIP_PIN_OUT;
    } else {
        *pw = e->body_w + 2 * R01S_CHIP_PIN_OUT;
        *ph = e->body_h;
    }
    if (*pw < 1) {
        *pw = 1;
    }
    if (*ph < 1) {
        *ph = 1;
    }
}

static int pack_item_taller(const void *a, const void *b) {
    const R01sPackItem *pa = a;
    const R01sPackItem *pb = b;
    if (pb->ph != pa->ph) {
        return pb->ph - pa->ph;
    }
    return pb->pw - pa->pw;
}

static int ui_isqrt(int n) {
    int x;
    int y;
    if (n <= 0) {
        return 0;
    }
    x = n;
    y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/* Shelf-pack items into rows capped at max_row_w; write board positions into out_x/out_y. */
static void pack_shelves(const R01sPackItem *items, int n, int max_row_w, int gap, int origin_x, int origin_y,
                         int *out_x, int *out_y, int *bb_w, int *bb_h) {
    int i;
    int x = origin_x;
    int y = origin_y;
    int row_h = 0;
    int max_x = origin_x;
    int max_y = origin_y;

    for (i = 0; i < n; i++) {
        int pw = items[i].pw;
        int ph = items[i].ph;
        if (i > 0 && x > origin_x && x + pw > origin_x + max_row_w) {
            x = origin_x;
            y += row_h + gap;
            row_h = 0;
        }
        out_x[i] = x;
        out_y[i] = y;
        if (x + pw > max_x) {
            max_x = x + pw;
        }
        if (y + ph > max_y) {
            max_y = y + ph;
        }
        if (ph > row_h) {
            row_h = ph;
        }
        x += pw + gap;
    }
    *bb_w = max_x - origin_x;
    *bb_h = max_y - origin_y;
}

static void ui_save_island_layout(R01sUi *ui) {
    r01s_ui_snapshot_island_layout(ui);
}

static void ui_chip_rel_from_abs(const R01sUi *ui, int chip_i, int abs_x, int abs_y, int *rx, int *ry) {
    const R01sIsland *island;
    if (!ui || !ui->group || chip_i < 0 || chip_i >= ui->chip_count || !rx || !ry) {
        if (rx) {
            *rx = abs_x;
        }
        if (ry) {
            *ry = abs_y;
        }
        return;
    }
    island = r01s_island_group_at(ui->group, ui->chip_island[chip_i]);
    if (!island) {
        *rx = abs_x;
        *ry = abs_y;
        return;
    }
    *rx = abs_x - island->board_x;
    *ry = abs_y - island->board_y;
}

static void ui_chip_place_rel(R01sUi *ui, int chip_i, int rx, int ry) {
    R01sEntity *e;
    const R01sIsland *island;
    if (!ui || !ui->group || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    island = r01s_island_group_at(ui->group, ui->chip_island[chip_i]);
    if (!e || !island) {
        return;
    }
    r01s_entity_place(e, island->board_x + rx, island->board_y + ry);
    clamp_chip_in_island(ui, e, ui->chip_island[chip_i]);
}

/* Place at exact island-relative coords — used for faithful load (no clamp/grow). */
static void ui_chip_place_rel_exact(R01sUi *ui, int chip_i, int rx, int ry) {
    R01sEntity *e;
    const R01sIsland *island;
    if (!ui || !ui->group || chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    island = r01s_island_group_at(ui->group, ui->chip_island[chip_i]);
    if (!e || !island) {
        return;
    }
    r01s_entity_place(e, island->board_x + rx, island->board_y + ry);
}

void r01s_ui_snapshot_island_layout(R01sUi *ui) {
    int i;

    if (!ui || !ui->group) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!e) {
            ui->save_chip_x[i] = 0;
            ui->save_chip_y[i] = 0;
            ui->save_chip_orient[i] = (uint8_t)R01S_ORIENT_H;
            continue;
        }
        ui_chip_rel_from_abs(ui, i, e->board_x, e->board_y, &ui->save_chip_x[i], &ui->save_chip_y[i]);
        ui->save_chip_orient[i] = (uint8_t)e->orient;
    }
    r01s_ui_snapshot_island_frames(ui);
    ui->layout_saved = 1;
}

void r01s_ui_snapshot_island_frames(R01sUi *ui) {
    int i;
    int n_islands;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        const R01sIsland *island = r01s_island_group_at(ui->group, i);
        if (!island) {
            continue;
        }
        if (island->board_w > 0 && island->board_h > 0) {
            ui->save_island_x[i] = island->board_x;
            ui->save_island_y[i] = island->board_y;
            ui->save_island_w[i] = island->board_w;
            ui->save_island_h[i] = island->board_h;
        }
    }
}

static int ui_island_snapshot_valid(const R01sUi *ui) {
    int i;
    int n_islands;
    if (!ui || !ui->group) {
        return 0;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        if (ui->save_island_w[i] > 0 && ui->save_island_h[i] > 0) {
            return 1;
        }
    }
    return 0;
}

static void ui_arrange_islands_default(R01sUi *ui) {
    int n_islands;
    int i;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    /*
     * Pack chips into each island first. Never size frames from whatever absolute
     * chip positions happen to be live (e.g. compact-mode coords) — that creates
     * huge overlapping islands.
     */
    for (i = 0; i < n_islands; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        if (!island) {
            continue;
        }
        island->board_x = 0;
        island->board_y = 0;
        island->board_w = R01S_ISLAND_MIN_W;
        island->board_h = R01S_ISLAND_MIN_H;
        ui_pack_island_chips(ui, i);
    }
    ui_row_place_islands(ui);
}

/* Place already-sized islands in wrapping rows; chips move with their frame. */
static void ui_row_place_islands(R01sUi *ui) {
    int n_islands;
    int start_x = 40;
    int start_y = 40;
    int x = start_x;
    int y = start_y;
    int row_h = 0;
    int limit = start_x + R01S_ISLAND_ROW_MAX_W;
    int i;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        int dx;
        int dy;
        int j;

        if (!island) {
            continue;
        }
        if (island->board_w < R01S_ISLAND_MIN_W) {
            island->board_w = R01S_ISLAND_MIN_W;
        }
        if (island->board_h < R01S_ISLAND_MIN_H) {
            island->board_h = R01S_ISLAND_MIN_H;
        }
        island->board_w = r01s_grid_snap_up(island->board_w);
        island->board_h = r01s_grid_snap_up(island->board_h);
        if (i > 0 && x > start_x && x + island->board_w > limit) {
            x = start_x;
            y += row_h + R01S_ISLAND_GAP;
            row_h = 0;
        }
        x = r01s_grid_snap(x);
        y = r01s_grid_snap(y);
        dx = x - island->board_x;
        dy = y - island->board_y;
        island->board_x = x;
        island->board_y = y;
        for (j = 0; j < ui->chip_count; j++) {
            R01sEntity *e = ui->chips[j];
            if (!e || ui->chip_island[j] != (uint8_t)i) {
                continue;
            }
            r01s_entity_place(e, e->board_x + dx, e->board_y + dy);
        }
        if (island->board_h > row_h) {
            row_h = island->board_h;
        }
        x += island->board_w + R01S_ISLAND_GAP;
    }
}

/*
 * Recover island frames when islands[] was missing/empty but island_chips look
 * like valid island-relative placements (common corrupt compact save).
 */
static void ui_rebuild_islands_from_saved_chips(R01sUi *ui) {
    int n_islands;
    int i;

    if (!ui || !ui->group) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        int j;
        if (!island) {
            continue;
        }
        if (!island_saved_chip_layout_sane(ui, i)) {
            island->board_x = 0;
            island->board_y = 0;
            island->board_w = R01S_ISLAND_MIN_W;
            island->board_h = R01S_ISLAND_MIN_H;
            ui_pack_island_chips(ui, i);
            continue;
        }
        island->board_x = 0;
        island->board_y = 0;
        island->board_w = R01S_ISLAND_MIN_W;
        island->board_h = R01S_ISLAND_MIN_H;
        for (j = 0; j < ui->chip_count; j++) {
            R01sEntity *e = ui->chips[j];
            if (!e || ui->chip_island[j] != (uint8_t)i) {
                continue;
            }
            if (e->visual == R01S_ENTITY_VIS_IC) {
                r01s_entity_set_orient(e, (R01sPkgOrient)ui->save_chip_orient[j]);
            }
        }
        island_expand_for_saved_chips(ui, i);
        for (j = 0; j < ui->chip_count; j++) {
            if (ui->chip_island[j] != (uint8_t)i) {
                continue;
            }
            ui_chip_place_rel(ui, j, ui->save_chip_x[j], ui->save_chip_y[j]);
        }
        ui_tighten_island_to_chips(ui, i);
        if (island_chips_overlap(ui, i)) {
            ui_pack_island_chips(ui, i);
        }
    }
    ui_row_place_islands(ui);
}

/* Fit frame exactly to chip content (shrinks wasted empty space). */
static void ui_tighten_island_to_chips(R01sUi *ui, int island_index) {
    R01sIsland *island;
    int min_w;
    int min_h;

    if (!ui || !ui->group) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    island_content_min_size(ui, island_index, &min_w, &min_h);
    if (min_w < R01S_ISLAND_MIN_W) {
        min_w = R01S_ISLAND_MIN_W;
    }
    if (min_h < R01S_ISLAND_MIN_H) {
        min_h = R01S_ISLAND_MIN_H;
    }
    island->board_w = r01s_grid_snap_up(min_w);
    island->board_h = r01s_grid_snap_up(min_h);
}

/* Shelf-pack chips inside one island so they never share the same cell. */
static void ui_pack_island_chips(R01sUi *ui, int island_index) {
    R01sIsland *island;
    R01sPackItem items[R01S_BOARD_MAX_CHIPS];
    int place_x[R01S_BOARD_MAX_CHIPS];
    int place_y[R01S_BOARD_MAX_CHIPS];
    int n = 0;
    int i;
    int origin_x;
    int origin_y;
    int max_row;
    int bb_w = 0;
    int bb_h = 0;

    if (!ui || !ui->group) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!e || ui->chip_island[i] != (uint8_t)island_index || e->visual == R01S_ENTITY_VIS_NONE ||
            e->body_w <= 0 || e->body_h <= 0) {
            continue;
        }
        items[n].idx = i;
        chip_pack_footprint(e, &items[n].pw, &items[n].ph);
        n++;
    }
    if (n == 0) {
        return;
    }
    qsort(items, (size_t)n, sizeof(items[0]), pack_item_taller);
    origin_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
    origin_y = island->board_y + R01S_ISLAND_PAD_TOP;
    max_row = island->board_w - 2 * R01S_ISLAND_PAD_X - 2 * R01S_CHIP_PIN_OUT;
    if (max_row < items[0].pw) {
        max_row = items[0].pw;
    }
    pack_shelves(items, n, max_row, R01S_CHIP_GAP, origin_x, origin_y, place_x, place_y, &bb_w, &bb_h);
    for (i = 0; i < n; i++) {
        R01sEntity *e = ui->chips[items[i].idx];
        int bx = place_x[i];
        int by = place_y[i];
        int pw, ph;
        if (!e) {
            continue;
        }
        chip_pack_footprint(e, &pw, &ph);
        bx += (pw - e->body_w) / 2;
        by += (ph - e->body_h) / 2;
        r01s_entity_place(e, r01s_grid_snap(bx), r01s_grid_snap(by));
    }
    ui_tighten_island_to_chips(ui, island_index);
}

static int entity_bodies_overlap(const R01sEntity *a, const R01sEntity *b) {
    if (!a || !b) {
        return 0;
    }
    return a->board_x < b->board_x + b->body_w && a->board_x + a->body_w > b->board_x &&
           a->board_y < b->board_y + b->body_h && a->board_y + a->body_h > b->board_y;
}

static int island_chips_overlap(const R01sUi *ui, int island_index) {
    int i, j;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *a;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        a = ui->chips[i];
        if (!a || a->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        for (j = i + 1; j < ui->chip_count; j++) {
            const R01sEntity *b;
            if (ui->chip_island[j] != (uint8_t)island_index) {
                continue;
            }
            b = ui->chips[j];
            if (!b || b->visual == R01S_ENTITY_VIS_NONE) {
                continue;
            }
            if (entity_bodies_overlap(a, b)) {
                return 1;
            }
        }
    }
    return 0;
}

static int island_saved_positions_degenerate(const R01sUi *ui, int island_index) {
    int i;
    int n = 0;
    int fx = 0;
    int fy = 0;
    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (!ui->chips[i] || ui->chips[i]->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        if (n == 0) {
            fx = ui->save_chip_x[i];
            fy = ui->save_chip_y[i];
        } else if (ui->save_chip_x[i] != fx || ui->save_chip_y[i] != fy) {
            return 0;
        }
        n++;
    }
    return n > 1;
}

/*
 * Island-relative chip saves sometimes get corrupted into absolute board coords
 * (ry in the thousands). Reject those so we re-pack instead of inflating frames.
 */
static int island_saved_chip_layout_sane(const R01sUi *ui, int island_index) {
    int i;
    int n = 0;
    /* One island's content should stay well under the board row wrap width. */
    const int max_rel = R01S_ISLAND_ROW_MAX_W;

    if (!ui) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (!ui->chips[i] || ui->chips[i]->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        n++;
        if (ui->save_chip_x[i] < 0 || ui->save_chip_y[i] < 0) {
            return 0;
        }
        if (ui->save_chip_x[i] > max_rel || ui->save_chip_y[i] > max_rel) {
            return 0;
        }
    }
    if (n == 0) {
        return 1;
    }
    return !island_saved_positions_degenerate(ui, island_index);
}

static void island_expand_for_saved_chips(R01sUi *ui, int island_index) {
    R01sIsland *island;
    int i;
    int need_w = R01S_ISLAND_MIN_W;
    int need_h = R01S_ISLAND_MIN_H;

    if (!ui || !ui->group) {
        return;
    }
    island = r01s_island_group_at_mut(ui->group, island_index);
    if (!island) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        int right;
        int bottom;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        right = ui->save_chip_x[i] + e->body_w + R01S_CHIP_PIN_OUT + R01S_ISLAND_PAD_X;
        bottom = ui->save_chip_y[i] + e->body_h + R01S_ISLAND_PAD_BOTTOM;
        if (right > need_w) {
            need_w = right;
        }
        if (bottom > need_h) {
            need_h = bottom;
        }
    }
    if (island->board_w < need_w) {
        island->board_w = need_w;
    }
    if (island->board_h < need_h) {
        island->board_h = need_h;
    }
}

static void ui_save_compact_layout(R01sUi *ui) {
    int i;
    if (!ui) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        ui->compact_chip_x[i] = e ? e->board_x : 0;
        ui->compact_chip_y[i] = e ? e->board_y : 0;
        ui->compact_chip_orient[i] = e ? (uint8_t)e->orient : (uint8_t)R01S_ORIENT_H;
    }
    ui->compact_saved = 1;
}

void r01s_ui_apply_saved_island_layout(R01sUi *ui) {
    int i;
    int n_islands;

    if (!ui || !ui->group || !ui->layout_saved) {
        return;
    }
    n_islands = r01s_island_group_count(ui->group);

    /* Exact frames from file — no snap/expand/pack. */
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
        if (!island || ui->save_island_w[i] <= 0 || ui->save_island_h[i] <= 0) {
            continue;
        }
        island->board_x = ui->save_island_x[i];
        island->board_y = ui->save_island_y[i];
        island->board_w = ui->save_island_w[i];
        island->board_h = ui->save_island_h[i];
    }

    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        if (e->visual == R01S_ENTITY_VIS_IC) {
            r01s_entity_set_orient(e, (R01sPkgOrient)ui->save_chip_orient[i]);
        }
        ui_chip_place_rel_exact(ui, i, ui->save_chip_x[i], ui->save_chip_y[i]);
    }
}

void r01s_ui_layout_migrate_v1_chips(R01sUi *ui) {
    int i;
    if (!ui || !ui->group) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sIsland *island = r01s_island_group_at(ui->group, ui->chip_island[i]);
        if (!island) {
            continue;
        }
        ui->save_chip_x[i] -= island->board_x;
        ui->save_chip_y[i] -= island->board_y;
    }
}

void r01s_ui_load_island_layout(R01sUi *ui, int file_version) {
    int i;
    int n_islands;

    if (!ui || !ui->group || !ui->layout_saved) {
        return;
    }

    /* v1 without island frames stored absolute board coordinates. */
    if (file_version < 2 && !ui_island_snapshot_valid(ui)) {
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *e = ui->chips[i];
            if (!e) {
                continue;
            }
            if (e->visual == R01S_ENTITY_VIS_IC) {
                r01s_entity_set_orient(e, (R01sPkgOrient)ui->save_chip_orient[i]);
            }
            r01s_entity_place(e, ui->save_chip_x[i], ui->save_chip_y[i]);
        }
        ui_arrange_islands_default(ui);
        r01s_ui_snapshot_island_layout(ui);
        return;
    }

    n_islands = r01s_island_group_count(ui->group);
    if (file_version < 2) {
        for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
            R01sIsland *island = r01s_island_group_at_mut(ui->group, i);
            if (!island || ui->save_island_w[i] <= 0 || ui->save_island_h[i] <= 0) {
                continue;
            }
            island->board_x = ui->save_island_x[i];
            island->board_y = ui->save_island_y[i];
            island->board_w = ui->save_island_w[i];
            island->board_h = ui->save_island_h[i];
        }
        r01s_ui_layout_migrate_v1_chips(ui);
    }

    /*
     * Missing/empty islands[] with leftover island_chips is a common corrupt
     * save. Rebuild frames from relative chip placements when those look sane;
     * otherwise pack from scratch. Valid saves apply verbatim.
     */
    if (!ui_island_snapshot_valid(ui)) {
        int any_sane = 0;
        for (i = 0; i < n_islands; i++) {
            if (island_saved_chip_layout_sane(ui, i)) {
                any_sane = 1;
                break;
            }
        }
        if (any_sane) {
            ui_rebuild_islands_from_saved_chips(ui);
        } else {
            ui_arrange_islands_default(ui);
        }
        r01s_ui_snapshot_island_layout(ui);
        return;
    }

    r01s_ui_apply_saved_island_layout(ui);
}

static void ui_restore_island_layout(R01sUi *ui) {
    int i;
    int n_islands;

    if (!ui || !ui->group) {
        return;
    }
    if (ui->layout_saved && ui_island_snapshot_valid(ui)) {
        r01s_ui_apply_saved_island_layout(ui);
        return;
    }
    /* Frames missing: recover from island_chips if possible. */
    n_islands = r01s_island_group_count(ui->group);
    if (ui->layout_saved) {
        int any_sane = 0;
        for (i = 0; i < n_islands; i++) {
            if (island_saved_chip_layout_sane(ui, i)) {
                any_sane = 1;
                break;
            }
        }
        if (any_sane) {
            ui_rebuild_islands_from_saved_chips(ui);
            r01s_ui_snapshot_island_layout(ui);
            return;
        }
    }
    ui_arrange_islands_default(ui);
    r01s_ui_snapshot_island_layout(ui);
}

static void ui_restore_compact_layout(R01sUi *ui) {
    int i;
    if (!ui || !ui->compact_saved) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e = ui->chips[i];
        if (!e) {
            continue;
        }
        if (e->visual == R01S_ENTITY_VIS_IC) {
            r01s_entity_set_orient(e, (R01sPkgOrient)ui->compact_chip_orient[i]);
        }
        r01s_entity_place(e, ui->compact_chip_x[i], ui->compact_chip_y[i]);
    }
}

/*
 * Pack all drawable chips into a near-square rectangle (shelf packing over
 * several candidate row widths). Chip body is inset by pin stub margin.
 */
static void ui_apply_compact_layout(R01sUi *ui) {
    R01sPackItem items[R01S_BOARD_MAX_CHIPS];
    int place_x[R01S_BOARD_MAX_CHIPS];
    int place_y[R01S_BOARD_MAX_CHIPS];
    int best_x[R01S_BOARD_MAX_CHIPS];
    int best_y[R01S_BOARD_MAX_CHIPS];
    int n = 0;
    int i;
    int area = 0;
    int side;
    int best_score = 0x7fffffff;
    int best_w = 0;
    int best_h = 0;
    int t;

    if (!ui) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (!e || e->visual == R01S_ENTITY_VIS_NONE || e->body_w <= 0 || e->body_h <= 0) {
            continue;
        }
        items[n].idx = i;
        chip_pack_footprint(e, &items[n].pw, &items[n].ph);
        area += items[n].pw * items[n].ph;
        n++;
    }
    if (n == 0) {
        return;
    }
    qsort(items, (size_t)n, sizeof(items[0]), pack_item_taller);
    side = ui_isqrt(area);
    if (side < items[0].pw) {
        side = items[0].pw;
    }

    for (t = 0; t < 24; t++) {
        int max_row = side + (t - 8) * (side / 8 + 8);
        int bb_w = 0;
        int bb_h = 0;
        int score;
        int diff;
        if (max_row < items[0].pw) {
            max_row = items[0].pw;
        }
        pack_shelves(items, n, max_row, R01S_COMPACT_GAP, R01S_COMPACT_ORIGIN_X, R01S_COMPACT_ORIGIN_Y,
                     place_x, place_y, &bb_w, &bb_h);
        diff = bb_w > bb_h ? bb_w - bb_h : bb_h - bb_w;
        /* Prefer near-square, then smaller bounding box. */
        score = diff * 4 + bb_w + bb_h;
        if (score < best_score) {
            best_score = score;
            best_w = bb_w;
            best_h = bb_h;
            memcpy(best_x, place_x, (size_t)n * sizeof(int));
            memcpy(best_y, place_y, (size_t)n * sizeof(int));
        }
    }

    for (i = 0; i < n; i++) {
        R01sEntity *e = ui->chips[items[i].idx];
        int bx = best_x[i];
        int by = best_y[i];
        int pw, ph;
        if (!e) {
            continue;
        }
        chip_pack_footprint(e, &pw, &ph);
        /* Center body inside footprint so pin stubs stay inside the cell. */
        bx += (pw - e->body_w) / 2;
        by += (ph - e->body_h) / 2;
        r01s_entity_place(e, r01s_grid_snap(bx), r01s_grid_snap(by));
        clamp_chip_to_board(e);
    }

    (void)best_w;
    (void)best_h;
    ui->pan_x = 0;
    ui->pan_y = 0;
    r01s_ui_clamp_pan(ui);
}

static void ui_toggle_compact(R01sUi *ui) {
    if (!ui) {
        return;
    }
    ui->drag_chip = -1;
    ui->drag_island = -1;
    ui->resize_island = -1;
    ui->selected = -1;
    ui->ctx_chip = -1;
    ui->box_sel = 0;
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));

    if (!ui->layout_compact) {
        ui_save_island_layout(ui);
        if (ui->compact_saved) {
            ui_restore_compact_layout(ui);
        } else {
            ui_apply_compact_layout(ui);
            ui_save_compact_layout(ui);
        }
        ui->layout_compact = 1;
        ui->layout_dirty = 1;
        snprintf(ui->status, sizeof(ui->status), "compact PCB layout — click ISLANDS to restore frames");
    } else {
        ui_save_compact_layout(ui);
        ui_restore_island_layout(ui);
        ui->layout_compact = 0;
        ui->layout_dirty = 1;
        snprintf(ui->status, sizeof(ui->status), "island layout restored");
        r01s_ui_clamp_pan(ui);
    }
}

static void compact_btn_rect(const R01sUi *ui, SDL_Rect *rc) {
    const char *label = (ui && ui->layout_compact) ? "ISLANDS" : "COMPACT";
    int tw = font_text_width(label) + 16;
    rc->x = R01S_LOGIC_W - tw - 8;
    rc->y = 3;
    rc->w = tw;
    rc->h = 16;
}

static void save_btn_rect(const R01sUi *ui, SDL_Rect *rc) {
    SDL_Rect cbtn;
    const char *label = "SAVE";
    int tw = font_text_width(label) + 16;
    compact_btn_rect(ui, &cbtn);
    rc->w = tw;
    rc->h = 16;
    rc->y = 3;
    rc->x = cbtn.x - tw - 6;
}

static void ui_save_layout_now(R01sUi *ui) {
    if (!ui || !ui->group) {
        return;
    }
    if (r01s_ui_layout_save(ui) == 0) {
        snprintf(ui->status, sizeof(ui->status), "layout saved");
    } else {
        snprintf(ui->status, sizeof(ui->status), "layout save failed");
    }
}

static void island_content_min_size(const R01sUi *ui, int island_index, int *min_w, int *min_h) {
    const R01sIsland *island = r01s_island_group_at(ui->group, island_index);
    int i;
    int need_w = R01S_ISLAND_MIN_W;
    int need_h = R01S_ISLAND_MIN_H;

    if (!island) {
        *min_w = R01S_ISLAND_MIN_W;
        *min_h = R01S_ISLAND_MIN_H;
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        int right, bottom;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        right = (e->board_x - island->board_x) + e->body_w + R01S_CHIP_PIN_OUT + R01S_ISLAND_PAD_X;
        bottom = (e->board_y - island->board_y) + e->body_h + R01S_ISLAND_PAD_BOTTOM;
        if (right > need_w) {
            need_w = right;
        }
        if (bottom > need_h) {
            need_h = bottom;
        }
    }
    *min_w = need_w;
    *min_h = need_h;
}

static void move_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my) {
    R01sIsland *island = r01s_island_group_at_mut(ui->group, island_index);
    int nx, ny, dx, dy, i;

    if (!island) {
        return;
    }
    nx = r01s_grid_snap(board_mx - ui->drag_grab_bx);
    ny = r01s_grid_snap(board_my - ui->drag_grab_by);
    if (nx < 0) {
        nx = 0;
    }
    if (ny < 0) {
        ny = 0;
    }
    if (nx + island->board_w > R01S_BOARD_W) {
        nx = r01s_grid_snap(R01S_BOARD_W - island->board_w);
    }
    if (ny + island->board_h > R01S_BOARD_H) {
        ny = r01s_grid_snap(R01S_BOARD_H - island->board_h);
    }
    if (nx < 0) {
        nx = 0;
    }
    if (ny < 0) {
        ny = 0;
    }
    dx = nx - island->board_x;
    dy = ny - island->board_y;
    if (dx == 0 && dy == 0) {
        return;
    }
    island->board_x = nx;
    island->board_y = ny;
    for (i = 0; i < ui->chip_count; i++) {
        R01sEntity *e;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        r01s_entity_place(e, e->board_x + dx, e->board_y + dy);
    }
}

static void island_chip_content_bounds(const R01sUi *ui, int island_index, int *out_l, int *out_t, int *out_r,
                                       int *out_b) {
    int i;
    int have = 0;
    int l = 0, t = 0, r = 0, b = 0;

    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e;
        int cl, ct, cr, cb;
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        e = ui->chips[i];
        if (!e || e->visual == R01S_ENTITY_VIS_NONE) {
            continue;
        }
        cl = e->board_x - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT;
        ct = e->board_y - R01S_ISLAND_PAD_TOP;
        cr = e->board_x + e->body_w + R01S_CHIP_PIN_OUT + R01S_ISLAND_PAD_X;
        cb = e->board_y + e->body_h + R01S_ISLAND_PAD_BOTTOM;
        if (!have) {
            l = cl;
            t = ct;
            r = cr;
            b = cb;
            have = 1;
        } else {
            if (cl < l) {
                l = cl;
            }
            if (ct < t) {
                t = ct;
            }
            if (cr > r) {
                r = cr;
            }
            if (cb > b) {
                b = cb;
            }
        }
    }
    if (!have) {
        l = 0;
        t = 0;
        r = R01S_ISLAND_MIN_W;
        b = R01S_ISLAND_MIN_H;
    }
    if (out_l) {
        *out_l = l;
    }
    if (out_t) {
        *out_t = t;
    }
    if (out_r) {
        *out_r = r;
    }
    if (out_b) {
        *out_b = b;
    }
}

static void resize_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my) {
    R01sIsland *island = r01s_island_group_at_mut(ui->group, island_index);
    int fixed_l, fixed_t, fixed_r, fixed_b;
    int nx, ny, nw, nh;
    int need_l, need_t, need_r, need_b;
    int i;
    int corner;

    if (!island) {
        return;
    }
    corner = ui->resize_corner;
    fixed_l = island->board_x;
    fixed_t = island->board_y;
    fixed_r = island->board_x + island->board_w;
    fixed_b = island->board_y + island->board_h;
    island_chip_content_bounds(ui, island_index, &need_l, &need_t, &need_r, &need_b);

    switch (corner) {
    case R01S_ISLAND_CORNER_BL:
        nx = board_mx;
        ny = fixed_t;
        nw = fixed_r - board_mx;
        nh = board_my - fixed_t;
        break;
    case R01S_ISLAND_CORNER_TR:
        nx = fixed_l;
        ny = board_my;
        nw = board_mx - fixed_l;
        nh = fixed_b - board_my;
        break;
    case R01S_ISLAND_CORNER_TL:
        nx = board_mx;
        ny = board_my;
        nw = fixed_r - board_mx;
        nh = fixed_b - board_my;
        break;
    case R01S_ISLAND_CORNER_BR:
    default:
        nx = fixed_l;
        ny = fixed_t;
        nw = board_mx - fixed_l;
        nh = board_my - fixed_t;
        break;
    }

    /* Keep chips inside: clamp edges that are being dragged. */
    if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
        if (nx > need_l) {
            nx = need_l;
        }
        nw = fixed_r - nx;
    } else {
        if (nx + nw < need_r) {
            nw = need_r - nx;
        }
    }
    if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
        if (ny > need_t) {
            ny = need_t;
        }
        nh = fixed_b - ny;
    } else {
        if (ny + nh < need_b) {
            nh = need_b - ny;
        }
    }

    if (nw < R01S_ISLAND_MIN_W) {
        if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
            nx = fixed_r - R01S_ISLAND_MIN_W;
            nw = R01S_ISLAND_MIN_W;
        } else {
            nw = R01S_ISLAND_MIN_W;
        }
    }
    if (nh < R01S_ISLAND_MIN_H) {
        if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
            ny = fixed_b - R01S_ISLAND_MIN_H;
            nh = R01S_ISLAND_MIN_H;
        } else {
            nh = R01S_ISLAND_MIN_H;
        }
    }

    if (nx < 0) {
        if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
            nx = 0;
            nw = fixed_r - nx;
        } else {
            nx = 0;
        }
    }
    if (ny < 0) {
        if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
            ny = 0;
            nh = fixed_b - ny;
        } else {
            ny = 0;
        }
    }
    if (nx + nw > R01S_BOARD_W) {
        if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_TR) {
            nw = R01S_BOARD_W - nx;
        } else {
            nx = R01S_BOARD_W - nw;
            if (nx < 0) {
                nx = 0;
                nw = R01S_BOARD_W;
            }
        }
    }
    if (ny + nh > R01S_BOARD_H) {
        if (corner == R01S_ISLAND_CORNER_BR || corner == R01S_ISLAND_CORNER_BL) {
            nh = R01S_BOARD_H - ny;
        } else {
            ny = R01S_BOARD_H - nh;
            if (ny < 0) {
                ny = 0;
                nh = R01S_BOARD_H;
            }
        }
    }

    /* Snap origin down and size up so frames stay on the universal grid. */
    nx = r01s_grid_snap(nx);
    ny = r01s_grid_snap(ny);
    if (corner == R01S_ISLAND_CORNER_BL || corner == R01S_ISLAND_CORNER_TL) {
        nw = fixed_r - nx;
    }
    if (corner == R01S_ISLAND_CORNER_TR || corner == R01S_ISLAND_CORNER_TL) {
        nh = fixed_b - ny;
    }
    nw = r01s_grid_snap_up(nw);
    nh = r01s_grid_snap_up(nh);
    if (nw < R01S_ISLAND_MIN_W) {
        nw = R01S_ISLAND_MIN_W;
    }
    if (nh < R01S_ISLAND_MIN_H) {
        nh = R01S_ISLAND_MIN_H;
    }
    if (nx + nw > R01S_BOARD_W) {
        nw = r01s_grid_snap(R01S_BOARD_W - nx);
    }
    if (ny + nh > R01S_BOARD_H) {
        nh = r01s_grid_snap(R01S_BOARD_H - ny);
    }

    island->board_x = nx;
    island->board_y = ny;
    island->board_w = nw;
    island->board_h = nh;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] == (uint8_t)island_index) {
            clamp_chip(ui, ui->chips[i], island_index);
        }
    }
}

static void health_rgb(R01sHealth h, Uint8 *R, Uint8 *G, Uint8 *B) {
    switch (h) {
    case R01S_HEALTH_OK:
        *R = 70;
        *G = 210;
        *B = 100;
        break;
    case R01S_HEALTH_WARN:
        *R = 220;
        *G = 170;
        *B = 50;
        break;
    case R01S_HEALTH_FAIL:
        *R = 220;
        *G = 70;
        *B = 70;
        break;
    default:
        *R = 90;
        *G = 190;
        *B = 220;
        break;
    }
}

static void draw_health_dot(SDL_Renderer *r, int x, int y, R01sHealth h) {
    Uint8 R, G, B;
    health_rgb(h, &R, &G, &B);
    fill_rect(r, x, y, 8, 8, R, G, B);
    draw_rect(r, x, y, 8, 8, 30, 40, 35);
}

#define R01S_UI_SIDEBAR_X 8
#define R01S_UI_SIDEBAR_TOP (R01S_UI_HUD_TOP + 6)
#define R01S_UI_SIDEBAR_BOTTOM (R01S_LOGIC_H - R01S_UI_HUD_BOTTOM - 6)
#define R01S_UI_SIDEBAR_W (R01S_UI_SIDEBAR_L - 16)
#define R01S_UI_SIDEBAR_VIEW_H (R01S_UI_SIDEBAR_BOTTOM - R01S_UI_SIDEBAR_TOP)
#define R01S_UI_STATUS_HDR_H 66
#define R01S_UI_STATUS_ROW_H 16
#define R01S_UI_STATUS_FOOTER_H 16
#define R01S_UI_SIDEBAR_GAP 8
#define R01S_UI_RADIO_HDR_H 10
#define R01S_UI_RADIO_BTN_H 14
#define R01S_UI_RADIO_GAP 2
#define R01S_UI_RADIO_SECTION_GAP 4
#define R01S_UI_SCREEN_MODE_N 3
#define R01S_UI_PROBE_H 124
#define R01S_UI_PAD_BIT_STRIDE 8
#define GP_PANEL_GAP 6
#define GP_PANEL_W ((R01S_UI_SIDEBAR_W - GP_PANEL_GAP) / 2)
#define GP_PANEL_H 100
#define GP_STICK_R 14

static int health_needs_debug(R01sHealth h) {
    return h == R01S_HEALTH_WARN || h == R01S_HEALTH_FAIL;
}

static int status_panel_h(const R01sSystemHealth *health) {
    int n = health ? health->island_count : 0;
    return R01S_UI_STATUS_HDR_H + n * R01S_UI_STATUS_ROW_H + R01S_UI_STATUS_FOOTER_H;
}

static int sidebar_status_content_y(void) {
    return 0;
}

static int sidebar_probe_content_y(const R01sUi *ui) {
    return status_panel_h(ui ? &ui->health : NULL) + R01S_UI_SIDEBAR_GAP;
}

static int sidebar_gp_content_y(const R01sUi *ui) {
    return sidebar_probe_content_y(ui) + R01S_UI_PROBE_H + R01S_UI_SIDEBAR_GAP;
}

/* Content-local Y → screen Y (accounts for sidebar scroll). */
static int sidebar_sy(const R01sUi *ui, int content_y) {
    return R01S_UI_SIDEBAR_TOP + content_y - (ui ? ui->sidebar_scroll : 0);
}

static int sidebar_screen_section_y(const R01sUi *ui) {
    return sidebar_gp_content_y(ui) + GP_PANEL_H + R01S_UI_SIDEBAR_GAP;
}

static int sidebar_screen_section_h(void) {
    /* SCREEN modes share one row (Normal | Persist | Phosphor). */
    return R01S_UI_RADIO_HDR_H + R01S_UI_RADIO_BTN_H;
}

static int sidebar_dual_group_y(const R01sUi *ui, int group) {
    int y = sidebar_screen_section_y(ui) + sidebar_screen_section_h() + R01S_UI_RADIO_SECTION_GAP;
    y += group * (R01S_UI_RADIO_HDR_H + R01S_UI_RADIO_BTN_H + R01S_UI_RADIO_SECTION_GAP);
    return y;
}

static void sidebar_screen_mode_rect(const R01sUi *ui, int mode_index, SDL_Rect *rc) {
    int gap = R01S_UI_RADIO_GAP;
    int bw = (R01S_UI_SIDEBAR_W - (R01S_UI_SCREEN_MODE_N - 1) * gap) / R01S_UI_SCREEN_MODE_N;
    rc->h = R01S_UI_RADIO_BTN_H;
    rc->w = bw;
    rc->y = sidebar_sy(ui, sidebar_screen_section_y(ui) + R01S_UI_RADIO_HDR_H);
    rc->x = R01S_UI_SIDEBAR_X + mode_index * (bw + gap);
}

static void sidebar_dual_radio_rect(const R01sUi *ui, int group, int option, SDL_Rect *rc) {
    int gap = R01S_UI_RADIO_GAP;
    int bw = (R01S_UI_SIDEBAR_W - gap) / 2;
    rc->h = R01S_UI_RADIO_BTN_H;
    rc->w = bw;
    rc->y = sidebar_sy(ui, sidebar_dual_group_y(ui, group) + R01S_UI_RADIO_HDR_H);
    rc->x = R01S_UI_SIDEBAR_X + option * (bw + gap);
}

static int sidebar_content_h(const R01sUi *ui) {
    if (!ui) {
        return 0;
    }
    return sidebar_dual_group_y(ui, 1) + R01S_UI_RADIO_HDR_H + R01S_UI_RADIO_BTN_H;
}

static int sidebar_max_scroll(const R01sUi *ui) {
    int max_s = sidebar_content_h(ui) - R01S_UI_SIDEBAR_VIEW_H;
    return max_s > 0 ? max_s : 0;
}

static void sidebar_clamp_scroll(R01sUi *ui) {
    int max_s;
    if (!ui) {
        return;
    }
    max_s = sidebar_max_scroll(ui);
    if (ui->sidebar_scroll < 0) {
        ui->sidebar_scroll = 0;
    }
    if (ui->sidebar_scroll > max_s) {
        ui->sidebar_scroll = max_s;
    }
}

static int sidebar_hit(int lx, int ly) {
    return lx >= 0 && lx < R01S_UI_SIDEBAR_L && ly >= R01S_UI_SIDEBAR_TOP && ly < R01S_UI_SIDEBAR_BOTTOM;
}

static int ui_lcd_scale_2x(const R01sUi *ui) {
    R01sBoard *board = ui ? r01s_board_from_group(ui->group) : NULL;
    return board ? r01s_video_sink_scale_2x(&board->video_sink) : 0;
}

static int ui_screen_render_mode(const R01sUi *ui) {
    R01sBoard *board = ui ? r01s_board_from_group(ui->group) : NULL;
    return board ? r01s_video_sink_render_mode(&board->video_sink) : R01S_VIDEO_RENDER_DEFAULT;
}

static void ui_set_lcd_scale(R01sUi *ui, int scale_2x) {
    R01sBoard *board;
    R01sVideoSink *sink;
    if (!ui) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return;
    }
    sink = &board->video_sink;
    r01s_video_sink_set_scale_2x(sink, scale_2x ? 1 : 0);
    snprintf(ui->status, sizeof(ui->status), "SCALE %s (256x240 field, timing 341x262)",
             r01s_video_sink_scale_2x(sink) ? "2X" : "1X");
}

static void ui_toggle_lcd_scale(R01sUi *ui) {
    ui_set_lcd_scale(ui, !ui_lcd_scale_2x(ui));
}

static void ui_set_screen_render_mode(R01sUi *ui, int mode) {
    R01sBoard *board;
    R01sVideoSink *sink;
    static const char *const labels[R01S_UI_SCREEN_MODE_N] = {"Normal", "Persist", "Phosphor"};
    if (!ui || mode < 0 || mode >= R01S_UI_SCREEN_MODE_N) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return;
    }
    sink = &board->video_sink;
    r01s_video_sink_set_render_mode(sink, mode);
    snprintf(ui->status, sizeof(ui->status), "SCREEN %s", labels[mode]);
}

static void ui_set_sim_fast(R01sUi *ui, int enable) {
    R01sBoard *board;
    if (!ui) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return;
    }
    r01s_board_set_sim_fast(board, enable ? 1 : 0);
    snprintf(ui->status, sizeof(ui->status),
             r01s_board_sim_fast(board)
                 ? "SIM FAST: word MAP catchup + thin settle/beam (R01S_FAST)"
                 : "SIM PIN: full netlist settle (default)");
}

static int radio_hit(const SDL_Rect *rc, int mx, int my) {
    return rc && mx >= rc->x && mx < rc->x + rc->w && my >= rc->y && my < rc->y + rc->h;
}

static void draw_radio_option(SDL_Renderer *r, const SDL_Rect *rc, int selected, const char *label) {
    int cx;
    int cy;
    int label_x;
    if (!r || !rc || !label) {
        return;
    }
    cx = rc->x + 7;
    cy = rc->y + rc->h / 2;
    label_x = rc->x + 14;
    fill_rect(r, rc->x, rc->y, rc->w, rc->h, selected ? 36 : 22, selected ? 52 : 30, selected ? 40 : 28);
    draw_rect(r, rc->x, rc->y, rc->w, rc->h, selected ? 140 : 80, selected ? 170 : 100, selected ? 120 : 85);
    fill_rect(r, cx - 3, cy - 3, 6, 6, 24, 28, 32);
    draw_rect(r, cx - 3, cy - 3, 6, 6, 100, 110, 120);
    if (selected) {
        fill_rect(r, cx - 1, cy - 1, 2, 2, 180, 220, 160);
    }
    font_draw(r, label_x, rc->y + (rc->h - 8) / 2, label, selected ? 220 : 170, selected ? 230 : 180,
              selected ? 200 : 160);
}

static void draw_sidebar_radio_controls(SDL_Renderer *r, R01sUi *ui) {
    static const char *const screen_labels[R01S_UI_SCREEN_MODE_N] = {"Normal", "Persist", "Phosphor"};
    SDL_Rect rc;
    int mode;
    int i;
    R01sBoard *board;

    if (!r || !ui) {
        return;
    }
    mode = ui_screen_render_mode(ui);
    font_draw(r, R01S_UI_SIDEBAR_X, sidebar_sy(ui, sidebar_screen_section_y(ui)), "SCREEN", 150, 160, 140);
    for (i = 0; i < R01S_UI_SCREEN_MODE_N; i++) {
        sidebar_screen_mode_rect(ui, i, &rc);
        draw_radio_option(r, &rc, mode == i, screen_labels[i]);
    }

    font_draw(r, R01S_UI_SIDEBAR_X, sidebar_sy(ui, sidebar_dual_group_y(ui, 0)), "SCALE", 150, 160, 140);
    sidebar_dual_radio_rect(ui, 0, 0, &rc);
    draw_radio_option(r, &rc, !ui_lcd_scale_2x(ui), "1X");
    sidebar_dual_radio_rect(ui, 0, 1, &rc);
    draw_radio_option(r, &rc, ui_lcd_scale_2x(ui), "2X");

    font_draw(r, R01S_UI_SIDEBAR_X, sidebar_sy(ui, sidebar_dual_group_y(ui, 1)), "SIM", 150, 160, 140);
    board = r01s_board_from_group(ui->group);
    sidebar_dual_radio_rect(ui, 1, 0, &rc);
    draw_radio_option(r, &rc, !(board && r01s_board_sim_fast(board)), "PIN");
    sidebar_dual_radio_rect(ui, 1, 1, &rc);
    draw_radio_option(r, &rc, board && r01s_board_sim_fast(board), "FAST");
}

static void health_copy_btn_rect(const R01sUi *ui, int island_index, SDL_Rect *rc) {
    int status_y = sidebar_sy(ui, sidebar_status_content_y());
    rc->x = R01S_UI_SIDEBAR_X + R01S_UI_SIDEBAR_W - 42;
    rc->y = status_y + R01S_UI_STATUS_HDR_H + island_index * R01S_UI_STATUS_ROW_H;
    rc->w = 34;
    rc->h = 12;
}

static void health_system_copy_rect(const R01sUi *ui, SDL_Rect *rc) {
    int status_y = sidebar_sy(ui, sidebar_status_content_y());
    /* Title row — above the system color bar so it does not overlap. */
    rc->x = R01S_UI_SIDEBAR_X + R01S_UI_SIDEBAR_W - 50;
    rc->y = status_y + 4;
    rc->w = 42;
    rc->h = 14;
}

static int ui_copy_health_text(R01sUi *ui, const char *text) {
    if (!ui || !text || !text[0]) {
        return -1;
    }
    if (SDL_SetClipboardText(text) != 0) {
        snprintf(ui->status, sizeof(ui->status), "clipboard failed: %s", SDL_GetError());
        return -1;
    }
    snprintf(ui->status, sizeof(ui->status), "copied debug to clipboard (%zu chars)", strlen(text));
    return 0;
}

static void draw_system_health_panel(SDL_Renderer *r, R01sUi *ui, int py) {
    const R01sSystemHealth *health;
    int i;
    int px = R01S_UI_SIDEBAR_X;
    int pw = R01S_UI_SIDEBAR_W;
    int ph;
    int show_sys_copy;
    Uint8 sr, sg, sb;
    char row[56];
    SDL_Rect copy_rc;

    if (!ui) {
        return;
    }
    health = &ui->health;
    ph = status_panel_h(health);
    show_sys_copy = health_needs_debug(health->system) || health->system == R01S_HEALTH_BOOT;

    fill_rect(r, px, py, pw, ph, 14, 20, 16);
    draw_rect(r, px, py, pw, ph, 70, 90, 75);
    font_draw_ellipsize(r, px + 8, py + 6, "SYSTEM STATUS",
                        show_sys_copy ? (pw - 66) : (pw - 16), 190, 205, 180);

    if (show_sys_copy) {
        health_system_copy_rect(ui, &copy_rc);
        fill_rect(r, copy_rc.x, copy_rc.y, copy_rc.w, copy_rc.h, 50, 58, 48);
        draw_rect(r, copy_rc.x, copy_rc.y, copy_rc.w, copy_rc.h, 200, 190, 100);
        font_draw(r, copy_rc.x + 4, copy_rc.y + 3, "COPY", 230, 220, 140);
    }

    health_rgb(health->system, &sr, &sg, &sb);
    fill_rect(r, px + 8, py + 22, pw - 16, 18, sr, sg, sb);
    font_draw_ellipsize(r, px + 12, py + 26, health->system_label[0] ? health->system_label : "?", pw - 28, 20,
                        24, 22);

    font_draw_ellipsize(r, px + 8, py + 44, health->system_detail, pw - 16, 150, 165, 145);
    fill_rect(r, px + 8, py + 58, pw - 16, 1, 40, 55, 45);

    for (i = 0; i < health->island_count; i++) {
        const R01sIslandHealth *ih = &health->islands[i];
        int ry = py + R01S_UI_STATUS_HDR_H + i * R01S_UI_STATUS_ROW_H;
        char letter[2];
        int row_max_w;
        int has_copy = health_needs_debug(ih->health);

        letter[0] = ih->letter ? ih->letter : '?';
        letter[1] = '\0';
        draw_health_dot(r, px + 10, ry + 1, ih->health);
        font_draw(r, px + 22, ry + 2, letter, 180, 200, 170);
        snprintf(row, sizeof(row), "%s %s", r01s_health_tag(ih->health), ih->activity);
        row_max_w = has_copy ? (pw - 34 - 46) : (pw - 34 - 12);
        if (row_max_w < 24) {
            row_max_w = 24;
        }
        font_draw_ellipsize(r, px + 34, ry + 2, row, row_max_w, 140, 155, 135);
        if (has_copy) {
            health_copy_btn_rect(ui, i, &copy_rc);
            fill_rect(r, copy_rc.x, copy_rc.y, copy_rc.w, copy_rc.h, 50, 58, 48);
            draw_rect(r, copy_rc.x, copy_rc.y, copy_rc.w, copy_rc.h, 160, 150, 80);
            font_draw(r, copy_rc.x + 4, copy_rc.y + 2, "COPY", 210, 200, 120);
        }
    }

    font_draw_ellipsize(r, px + 8, py + ph - 14, "WARN/FAIL COPY PASTES DEBUG", pw - 16, 100, 115, 100);
}

static void draw_pad_bits_compact(SDL_Renderer *r, int x, int y, uint8_t bits) {
    static const char *names[8] = {"R", "L", "D", "U", "X", "Y", "C", "S"};
    int i;
    for (i = 0; i < 8; i++) {
        int on = (bits & (1u << i)) != 0;
        int bx = x + i * R01S_UI_PAD_BIT_STRIDE;
        font_draw(r, bx, y, names[i], on ? 80 : 140, on ? 220 : 150, on ? 100 : 140);
    }
}

static void draw_pin_swatch(SDL_Renderer *r, int x, int y, Uint8 R, Uint8 G, Uint8 B, const char *label) {
    fill_rect(r, x, y, 7, 7, R, G, B);
    draw_rect(r, x, y, 7, 7, 90, 100, 85);
    font_draw(r, x + 10, y, label, 160, 175, 155);
}

static void draw_live_probe(SDL_Renderer *r, const R01sUi *ui, int py) {
    int px = R01S_UI_SIDEBAR_X;
    int pw = R01S_UI_SIDEBAR_W;
    Uint8 pr, pg, pb;
    fill_rect(r, px, py, pw, R01S_UI_PROBE_H, 16, 22, 18);
    draw_rect(r, px, py, pw, R01S_UI_PROBE_H, 80, 90, 70);
    font_draw(r, px + 8, py + 4, "PROBE", 200, 210, 180);
    draw_led(r, px + 8, py + 16, ui->probe_vdd, 80, 220, 100, "VDD");
    draw_led(r, px + 78, py + 16, ui->probe_phi2, 220, 200, 60, "PHI2");
    draw_led(r, px + 148, py + 16, ui->probe_resb_low, 220, 80, 80, "RST");
    font_draw(r, px + 8, py + 34, "P1", 160, 180, 160);
    draw_pad_bits_compact(r, px + 26, py + 32, ui->probe_pad_p1);
    font_draw(r, px + 8, py + 48, "P2", 160, 180, 160);
    draw_pad_bits_compact(r, px + 26, py + 46, ui->probe_pad_p2);

    font_draw(r, px + 8, py + 64, "PINS", 200, 210, 180);
    pin_level_rgb(R01S_LVL_H, R01S_PIN_OUT, &pr, &pg, &pb);
    draw_pin_swatch(r, px + 8, py + 76, pr, pg, pb, "HIGH");
    pin_level_rgb(R01S_LVL_L, R01S_PIN_OUT, &pr, &pg, &pb);
    draw_pin_swatch(r, px + 108, py + 76, pr, pg, pb, "LOW");
    pin_level_rgb(R01S_LVL_X, R01S_PIN_OUT, &pr, &pg, &pb);
    draw_pin_swatch(r, px + 8, py + 90, pr, pg, pb, "X");
    pin_level_rgb(R01S_LVL_Z, R01S_PIN_OUT, &pr, &pg, &pb);
    draw_pin_swatch(r, px + 108, py + 90, pr, pg, pb, "Hi-Z");
    pin_level_rgb(R01S_LVL_L, R01S_PIN_PWR, &pr, &pg, &pb);
    draw_pin_swatch(r, px + 8, py + 104, pr, pg, pb, "PWR");
    pin_level_rgb(R01S_LVL_L, R01S_PIN_NC, &pr, &pg, &pb);
    draw_pin_swatch(r, px + 108, py + 104, pr, pg, pb, "N/C");
}

static void draw_island_resize_grip(SDL_Renderer *r, int hx, int hy) {
    fill_rect(r, hx, hy, R01S_ISLAND_RESIZE_HANDLE, R01S_ISLAND_RESIZE_HANDLE, 40, 70, 50);
    draw_rect(r, hx, hy, R01S_ISLAND_RESIZE_HANDLE, R01S_ISLAND_RESIZE_HANDLE, 120, 160, 130);
}

static void draw_island_frame(SDL_Renderer *r, const R01sUi *ui, const R01sIsland *island, int active,
                              const R01sIslandHealth *ih) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int hs = R01S_ISLAND_RESIZE_HANDLE;
    Uint8 br, bg, bb;

    if (ih) {
        health_rgb(ih->health, &br, &bg, &bb);
    } else {
        br = active ? 140 : 60;
        bg = active ? 200 : 100;
        bb = active ? 120 : 70;
    }

    fill_rect(r, x, y, island->board_w, island->board_h, R01S_BOARD_BG_R, R01S_BOARD_BG_G, R01S_BOARD_BG_B);
    fill_rect(r, x + 2, y + 2, island->board_w - 4, R01S_ISLAND_HEADER_H, 22, 48, 32);
    draw_rect(r, x, y, island->board_w, island->board_h, br, bg, bb);
    /* Corner resize grips */
    draw_island_resize_grip(r, x, y);
    draw_island_resize_grip(r, x + island->board_w - hs, y);
    draw_island_resize_grip(r, x, y + island->board_h - hs);
    draw_island_resize_grip(r, x + island->board_w - hs, y + island->board_h - hs);
}

/* Drawn after chips so header text stays above packages. */
static void draw_island_header(SDL_Renderer *r, const R01sUi *ui, const R01sIsland *island,
                               const R01sIslandHealth *ih) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int title_max;
    int act_max;
    Uint8 br = 180, bg = 220, bb = 160;
    char badge[12];

    fill_rect(r, x + 2, y + 2, island->board_w - 4, R01S_ISLAND_HEADER_H, 22, 48, 32);
    title_max = island->board_w - 56;
    if (title_max < 24) {
        title_max = 24;
    }
    font_draw_ellipsize(r, x + 8, y + 6, island->title ? island->title : "ISLAND", title_max, 180, 220, 160);
    if (ih && ih->activity[0]) {
        act_max = island->board_w - 16;
        if (act_max < 24) {
            act_max = 24;
        }
        font_draw_ellipsize(r, x + 8, y + 16, ih->activity, act_max, 130, 150, 125);
        health_rgb(ih->health, &br, &bg, &bb);
        snprintf(badge, sizeof(badge), "%s", r01s_health_tag(ih->health));
        font_draw(r, x + island->board_w - 44, y + 6, badge, br, bg, bb);
        draw_health_dot(r, x + island->board_w - 18, y + 5, ih->health);
    }
}

static int hit_chip(const R01sUi *ui, const R01sEntity *e, int lx, int ly);
static int hit_board_top(const R01sUi *ui, int lx, int ly, int *chip_out, int *island_out, int *corner_out);

static void draw_tooltip(SDL_Renderer *r, int lx, int ly, const char *text) {
    int tw;
    int pad = 4;
    int box_x;
    int box_y;
    int box_w;
    int box_h = 7 + pad * 2;

    if (!text || !text[0]) {
        return;
    }
    tw = font_text_width(text);
    box_w = tw + pad * 2;
    box_x = lx + 14;
    box_y = ly + 16;
    if (box_x + box_w > R01S_LOGIC_W - 4) {
        box_x = lx - box_w - 8;
    }
    if (box_y + box_h > R01S_LOGIC_H - 4) {
        box_y = ly - box_h - 8;
    }
    if (box_x < 4) {
        box_x = 4;
    }
    if (box_y < 4) {
        box_y = 4;
    }
    fill_rect(r, box_x, box_y, box_w, box_h, 20, 24, 18);
    draw_rect(r, box_x, box_y, box_w, box_h, 180, 200, 160);
    font_draw(r, box_x + pad, box_y + pad, text, 220, 230, 200);
}

static void ui_fill_tooltip(const R01sUi *ui, char *out, size_t out_len) {
    int chip_i = -1;
    int island_i = -1;
    int kind;

    if (!ui || !out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (!ui_logic_in_view(ui->mouse_lx, ui->mouse_ly)) {
        return;
    }

    kind = hit_board_top(ui, ui->mouse_lx, ui->mouse_ly, &chip_i, &island_i, NULL);
    if (kind == 1 && chip_i >= 0 && chip_i < ui->chip_count) {
        const R01sEntity *e = ui->chips[chip_i];
        if (!e) {
            return;
        }
        if (e->refdes && e->part) {
            snprintf(out, out_len, "%s  %s", e->refdes, e->part);
        } else if (e->part) {
            snprintf(out, out_len, "%s", e->part);
        } else if (e->refdes) {
            snprintf(out, out_len, "%s", e->refdes);
        }
        return;
    }
    if ((kind == 2 || kind == 3) && island_i >= 0 && ui->group) {
        const R01sIsland *island = r01s_island_group_at(ui->group, island_i);
        const R01sIslandHealth *ih = NULL;
        if (!island) {
            return;
        }
        if (island_i < ui->health.island_count) {
            ih = &ui->health.islands[island_i];
        }
        if (ih && ih->activity[0]) {
            snprintf(out, out_len, "%s - %s", island->title ? island->title : "ISLAND", ih->activity);
        } else {
            snprintf(out, out_len, "%s", island->title ? island->title : "ISLAND");
        }
    }
}

static void gp_panel_origin(const R01sUi *ui, int player, int *px, int *py) {
    *px = R01S_UI_SIDEBAR_X + player * (GP_PANEL_W + GP_PANEL_GAP);
    *py = sidebar_sy(ui, sidebar_gp_content_y(ui));
}

static void gp_stick_center(const R01sUi *ui, int player, int *cx, int *cy) {
    int px, py;
    gp_panel_origin(ui, player, &px, &py);
    *cx = px + 26;
    *cy = py + 48;
}

static void gp_btn_rect(const R01sUi *ui, int player, int btn, SDL_Rect *rc) {
    int px, py;
    gp_panel_origin(ui, player, &px, &py);
    rc->x = px + 54 + (btn % 2) * 22;
    rc->y = py + 28 + (btn / 2) * 22;
    rc->w = 20;
    rc->h = 20;
}

static int gp_hit_stick(const R01sUi *ui, int player, int lx, int ly) {
    int cx, cy;
    int dx, dy;
    gp_stick_center(ui, player, &cx, &cy);
    dx = lx - cx;
    dy = ly - cy;
    return dx * dx + dy * dy <= (GP_STICK_R + 6) * (GP_STICK_R + 6);
}

static int gp_hit_btn(const R01sUi *ui, int player, int lx, int ly) {
    int b;
    for (b = 0; b < 4; b++) {
        SDL_Rect rc;
        gp_btn_rect(ui, player, b, &rc);
        if (lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h) {
            return b;
        }
    }
    return -1;
}

static int gp_hit_any(const R01sUi *ui, int lx, int ly, int *player_out, int *btn_out) {
    int p;
    for (p = 0; p < R01S_UI_GAMEPAD_COUNT; p++) {
        int b = gp_hit_btn(ui, p, lx, ly);
        if (b >= 0) {
            *player_out = p;
            *btn_out = b;
            return 2;
        }
        if (gp_hit_stick(ui, p, lx, ly)) {
            *player_out = p;
            *btn_out = -1;
            return 1;
        }
    }
    return 0;
}

static void gp_stick_from_point(R01sUi *ui, R01sGamepadInput *gp, int player, int lx, int ly) {
    int cx, cy;
    gp_stick_center(ui, player, &cx, &cy);
    gp->stick_x = lx - cx;
    gp->stick_y = ly - cy;
    r01s_gamepad_stick_clamp(&gp->stick_x, &gp->stick_y, GP_STICK_R);
}

static void draw_stick(SDL_Renderer *r, int cx, int cy, int sx, int sy) {
    fill_rect(r, cx - GP_STICK_R, cy - GP_STICK_R, GP_STICK_R * 2, GP_STICK_R * 2, 24, 28, 32);
    draw_rect(r, cx - GP_STICK_R, cy - GP_STICK_R, GP_STICK_R * 2, GP_STICK_R * 2, 70, 80, 90);
    fill_rect(r, cx + sx - 4, cy + sy - 4, 8, 8, 180, 190, 200);
    draw_rect(r, cx + sx - 4, cy + sy - 4, 8, 8, 240, 240, 240);
}

static void draw_btn(SDL_Renderer *r, const SDL_Rect *rc, int pressed, const char *label) {
    fill_rect(r, rc->x, rc->y, rc->w, rc->h, pressed ? 90 : 40, pressed ? 120 : 48, pressed ? 160 : 56);
    draw_rect(r, rc->x, rc->y, rc->w, rc->h, 120, 130, 140);
    font_draw(r, rc->x + 3, rc->y + 6, label, 210, 210, 200);
}

static void draw_video_pixels(SDL_Renderer *r, R01sUi *ui, R01sVideoSink *sink, int px, int py) {
    const uint8_t *rgb;
    SDL_Rect dst;

    if (!r || !ui || !sink) {
        return;
    }
    rgb = r01s_video_sink_rgb(sink);
    if (!rgb) {
        return;
    }
    if (!ui->lcd_tex) {
        ui->lcd_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                        R01S_VIDEO_W, R01S_VIDEO_H);
        if (!ui->lcd_tex) {
            /* Fallback if RGB24 unsupported on this renderer. */
            ui->lcd_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                            R01S_VIDEO_W, R01S_VIDEO_H);
        }
        if (!ui->lcd_tex) {
            return;
        }
        SDL_SetTextureScaleMode(ui->lcd_tex, SDL_ScaleModeNearest);
    }
    {
        Uint32 fmt = 0;
        SDL_QueryTexture(ui->lcd_tex, &fmt, NULL, NULL, NULL);
        if (fmt == SDL_PIXELFORMAT_RGB24) {
            if (SDL_UpdateTexture(ui->lcd_tex, NULL, rgb, R01S_VIDEO_W * 3) != 0) {
                return;
            }
        } else {
            static uint8_t rgba[R01S_VIDEO_W * R01S_VIDEO_H * 4];
            int x, y;
            for (y = 0; y < R01S_VIDEO_H; y++) {
                for (x = 0; x < R01S_VIDEO_W; x++) {
                    size_t si = ((size_t)y * R01S_VIDEO_W + (size_t)x) * 3u;
                    size_t di = ((size_t)y * R01S_VIDEO_W + (size_t)x) * 4u;
                    rgba[di] = rgb[si];
                    rgba[di + 1] = rgb[si + 1];
                    rgba[di + 2] = rgb[si + 2];
                    rgba[di + 3] = 255;
                }
            }
            if (SDL_UpdateTexture(ui->lcd_tex, NULL, rgba, R01S_VIDEO_W * 4) != 0) {
                return;
            }
        }
    }
    dst.x = px;
    dst.y = py;
    dst.w = R01S_VIDEO_W;
    dst.h = R01S_VIDEO_H;
    SDL_RenderCopy(r, ui->lcd_tex, NULL, &dst);
}

static void draw_gamepad_panel(SDL_Renderer *r, const R01sUi *ui, int player) {
    int px, py, cx, cy, b;
    char hex[8];
    uint8_t bits;
    SDL_Rect brc;

    gp_panel_origin(ui, player, &px, &py);
    fill_rect(r, px, py, GP_PANEL_W, GP_PANEL_H, 16, 20, 24);
    draw_rect(r, px, py, GP_PANEL_W, GP_PANEL_H, 60, 70, 80);
    font_draw(r, px + 8, py + 6, player == 0 ? "P1 FE60" : "P2 FE61", 180, 200, 220);

    gp_stick_center(ui, player, &cx, &cy);
    draw_stick(r, cx, cy, ui->gamepad[player].stick_x, ui->gamepad[player].stick_y);

    for (b = 0; b < 4; b++) {
        const char *labels[4] = {"X", "Y", "C", "S"};
        int pressed = 0;
        gp_btn_rect(ui, player, b, &brc);
        switch (b) {
        case 0:
            pressed = ui->gamepad[player].btn_x;
            break;
        case 1:
            pressed = ui->gamepad[player].btn_y;
            break;
        case 2:
            pressed = ui->gamepad[player].btn_coin;
            break;
        case 3:
            pressed = ui->gamepad[player].btn_start;
            break;
        default:
            break;
        }
        draw_btn(r, &brc, pressed, labels[b]);
    }

    bits = r01s_gamepad_encode(&ui->gamepad[player]);
    snprintf(hex, sizeof(hex), "%02X", bits);
    font_draw(r, px + 8, py + GP_PANEL_H - 14, hex, 140, 160, 140);
}

void r01s_ui_sync_gamepads(R01sUi *ui) {
    const Uint8 *keys;
    if (!ui) {
        return;
    }
    keys = SDL_GetKeyboardState(NULL);

    if (ui->drag_stick != 0) {
        ui->gamepad[0].stick_x = 0;
        ui->gamepad[0].stick_y = 0;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
            ui->gamepad[0].stick_y = -GP_STICK_R;
        }
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
            ui->gamepad[0].stick_y = GP_STICK_R;
        }
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            ui->gamepad[0].stick_x = -GP_STICK_R;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            ui->gamepad[0].stick_x = GP_STICK_R;
        }
        if ((keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) &&
            (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S])) {
            ui->gamepad[0].stick_y = 0;
        }
        if ((keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) &&
            (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D])) {
            ui->gamepad[0].stick_x = 0;
        }
    }
    if (ui->drag_stick != 1) {
        ui->gamepad[1].stick_x = 0;
        ui->gamepad[1].stick_y = 0;
        if (keys[SDL_SCANCODE_I]) {
            ui->gamepad[1].stick_y = -GP_STICK_R;
        }
        if (keys[SDL_SCANCODE_K]) {
            ui->gamepad[1].stick_y = GP_STICK_R;
        }
        if (keys[SDL_SCANCODE_J]) {
            ui->gamepad[1].stick_x = -GP_STICK_R;
        }
        if (keys[SDL_SCANCODE_L]) {
            ui->gamepad[1].stick_x = GP_STICK_R;
        }
    }

    /* P1: X/Y match Studio warps (Z also fires X — legacy). */
    ui->gamepad[0].btn_x = ui->mouse_btn[0][0] || keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_Z];
    ui->gamepad[0].btn_y = ui->mouse_btn[0][1] || keys[SDL_SCANCODE_Y];
    ui->gamepad[0].btn_coin = ui->mouse_btn[0][2] || keys[SDL_SCANCODE_1];
    ui->gamepad[0].btn_start = ui->mouse_btn[0][3] || keys[SDL_SCANCODE_RETURN];

    ui->gamepad[1].btn_x = ui->mouse_btn[1][0] || keys[SDL_SCANCODE_N];
    ui->gamepad[1].btn_y = ui->mouse_btn[1][1] || keys[SDL_SCANCODE_M];
    ui->gamepad[1].btn_coin = ui->mouse_btn[1][2] || keys[SDL_SCANCODE_2];
    ui->gamepad[1].btn_start = ui->mouse_btn[1][3] || keys[SDL_SCANCODE_BACKSPACE];
}

uint8_t r01s_ui_gamepad_port(const R01sUi *ui, int player) {
    if (!ui || player < 0 || player >= R01S_UI_GAMEPAD_COUNT) {
        return 0;
    }
    return r01s_gamepad_encode(&ui->gamepad[player]);
}

void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r) {
    int i, gx, gy, pass;
    int ox = (-ui->pan_x) % 32;
    int oy = (-ui->pan_y) % 32;
    SDL_Rect view_clip = {R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H};
    SDL_Rect sidebar_clip = {0, R01S_UI_SIDEBAR_TOP, R01S_UI_SIDEBAR_L, R01S_UI_SIDEBAR_VIEW_H};
    int max_s;
    char fps_buf[16];

    sidebar_clamp_scroll(ui);
    max_s = sidebar_max_scroll(ui);

    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 12, 14, 16);
    fill_rect(r, 0, 0, R01S_UI_SIDEBAR_L, R01S_LOGIC_H, 14, 18, 16);
    fill_rect(r, R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H, R01S_BOARD_BG_R,
              R01S_BOARD_BG_G, R01S_BOARD_BG_B);

    SDL_RenderSetClipRect(r, &view_clip);
    SDL_SetRenderDrawColor(r, 24, 52, 34, 255);
    for (gx = R01S_UI_VIEW_X + ox; gx < R01S_UI_VIEW_X + R01S_UI_VIEW_W; gx += 32) {
        SDL_RenderDrawLine(r, gx, R01S_UI_VIEW_Y, gx, R01S_UI_VIEW_Y + R01S_UI_VIEW_H);
    }
    for (gy = R01S_UI_VIEW_Y + oy; gy < R01S_UI_VIEW_Y + R01S_UI_VIEW_H; gy += 32) {
        SDL_RenderDrawLine(r, R01S_UI_VIEW_X, gy, R01S_UI_VIEW_X + R01S_UI_VIEW_W, gy);
    }

    /* Islands back→front as complete units so a front island fully occludes
     * anything behind it (frame fill + chips + header). Compact: chips only. */
    if (ui->group && !ui->layout_compact) {
        int n_islands = r01s_island_group_count(ui->group);
        int front = -1;
        if (ui->drag_island >= 0) {
            front = ui->drag_island;
        } else if (ui->resize_island >= 0) {
            front = ui->resize_island;
        }
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < n_islands; i++) {
                const R01sIsland *island;
                const R01sIslandHealth *ih = NULL;
                int active;
                int j;
                if (pass == 0 && front >= 0 && i == front) {
                    continue; /* draw focused island last */
                }
                if (pass == 1 && i != front) {
                    continue;
                }
                island = r01s_island_group_at(ui->group, i);
                if (!island) {
                    continue;
                }
                active = (i == ui->drag_island || i == ui->resize_island);
                if (i < ui->health.island_count) {
                    ih = &ui->health.islands[i];
                }
                draw_island_frame(r, ui, island, active, ih);
                for (j = 0; j < ui->chip_count; j++) {
                    if (ui->chip_island[j] != (uint8_t)i) {
                        continue;
                    }
                    draw_board_item(r, ui, ui->chips[j], j == ui->selected);
                }
                draw_island_header(r, ui, island, ih);
            }
            if (front < 0) {
                break;
            }
        }
    } else {
        for (i = 0; i < ui->chip_count; i++) {
            draw_board_item(r, ui, ui->chips[i], ui->chip_sel[i] || i == ui->selected);
        }
        if (ui->box_sel) {
            int x0 = ui_board_sx(ui, ui->box_bx0 < ui->box_bx1 ? ui->box_bx0 : ui->box_bx1);
            int y0 = ui_board_sy(ui, ui->box_by0 < ui->box_by1 ? ui->box_by0 : ui->box_by1);
            int x1 = ui_board_sx(ui, ui->box_bx0 < ui->box_bx1 ? ui->box_bx1 : ui->box_bx0);
            int y1 = ui_board_sy(ui, ui->box_by0 < ui->box_by1 ? ui->box_by1 : ui->box_by0);
            int bw = x1 - x0;
            int bh = y1 - y0;
            if (bw < 1) {
                bw = 1;
            }
            if (bh < 1) {
                bh = 1;
            }
            SDL_SetRenderDrawColor(r, 80, 180, 120, 40);
            {
                SDL_Rect fill = {x0, y0, bw, bh};
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_RenderFillRect(r, &fill);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            }
            draw_rect(r, x0, y0, bw, bh, 120, 220, 160);
        }
    }
    SDL_RenderSetClipRect(r, NULL);

    draw_rect(r, R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H, 48, 64, 52);

    /* Fixed HUD */
    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_UI_HUD_TOP, 12, 14, 16);
    font_draw(r, 8, 7, "RETR01 SIM  ISLANDS O+A+C+D+G+H+J+K+L", 200, 210, 220);
    if (ui->layout_compact) {
        font_draw(r, R01S_UI_VIEW_X + 8, 7,
                  "COMPACT  DRAG-BOX SELECT  S SAVE  R ROTATE  G SCALE  SHIFT+CLICK", 120, 130, 140);
    } else {
        font_draw(r, R01S_UI_VIEW_X + 8, 7, "S SAVE  R ROTATE  G SCALE  Ctrl+R RESET  DRAG/RESIZE  SHIFT+ARROWS PAN", 120,
                  130, 140);
    }
    {
        SDL_Rect sbtn;
        save_btn_rect(ui, &sbtn);
        fill_rect(r, sbtn.x, sbtn.y, sbtn.w, sbtn.h, ui->layout_dirty ? 70 : 28, ui->layout_dirty ? 55 : 40,
                  ui->layout_dirty ? 30 : 32);
        draw_rect(r, sbtn.x, sbtn.y, sbtn.w, sbtn.h, ui->layout_dirty ? 220 : 120, ui->layout_dirty ? 180 : 160,
                  ui->layout_dirty ? 100 : 130);
        font_draw(r, sbtn.x + 8, sbtn.y + 5, "SAVE", ui->layout_dirty ? 255 : 200, ui->layout_dirty ? 230 : 220,
                  ui->layout_dirty ? 180 : 180);
    }
    {
        SDL_Rect cbtn;
        const char *clabel = ui->layout_compact ? "ISLANDS" : "COMPACT";
        compact_btn_rect(ui, &cbtn);
        fill_rect(r, cbtn.x, cbtn.y, cbtn.w, cbtn.h, ui->layout_compact ? 40 : 28, ui->layout_compact ? 70 : 40,
                  ui->layout_compact ? 50 : 32);
        draw_rect(r, cbtn.x, cbtn.y, cbtn.w, cbtn.h, 120, 160, 130);
        font_draw(r, cbtn.x + 8, cbtn.y + 5, clabel, 200, 220, 180);
    }

    /* Left sidebar: system status, probe, controllers, screen/scale/sim radios (scrollable). */
    SDL_RenderSetClipRect(r, &sidebar_clip);
    draw_system_health_panel(r, ui, sidebar_sy(ui, sidebar_status_content_y()));
    draw_live_probe(r, ui, sidebar_sy(ui, sidebar_probe_content_y(ui)));
    draw_gamepad_panel(r, ui, 0);
    draw_gamepad_panel(r, ui, 1);
    draw_sidebar_radio_controls(r, ui);
    SDL_RenderSetClipRect(r, NULL);

    if (max_s > 0) {
        int track_x = R01S_UI_SIDEBAR_L - 6;
        int track_y = R01S_UI_SIDEBAR_TOP;
        int track_h = R01S_UI_SIDEBAR_VIEW_H;
        int thumb_h = track_h * R01S_UI_SIDEBAR_VIEW_H / sidebar_content_h(ui);
        int thumb_y;
        if (thumb_h < 12) {
            thumb_h = 12;
        }
        thumb_y = track_y + (track_h - thumb_h) * ui->sidebar_scroll / max_s;
        fill_rect(r, track_x, track_y, 4, track_h, 28, 36, 30);
        fill_rect(r, track_x, thumb_y, 4, thumb_h, 90, 120, 95);
    }

    fill_rect(r, 0, R01S_LOGIC_H - R01S_UI_HUD_BOTTOM, R01S_LOGIC_W, R01S_UI_HUD_BOTTOM, 12, 14, 16);
    font_draw(r, 8, R01S_LOGIC_H - 15, ui->status, 160, 170, 160);
    snprintf(fps_buf, sizeof(fps_buf), "%d FPS %d STP", ui->fps, ui->sim_steps);
    font_draw(r, R01S_LOGIC_W - font_text_width(fps_buf) - 8, R01S_LOGIC_H - 15, fps_buf, 160, 180, 160);

    if (ui->ctx_chip >= 0 && ui->ctx_chip < ui->chip_count) {
        const R01sEntity *ce = ui->chips[ui->ctx_chip];
        const char *item = (ce && ce->orient == R01S_ORIENT_V) ? "ORIENT HORIZONTAL" : "ORIENT VERTICAL";
        int mw = font_text_width(item) + 16;
        int mh = 22;
        int mx = ui->ctx_x;
        int my = ui->ctx_y;
        if (mx + mw > R01S_LOGIC_W - 4) {
            mx = R01S_LOGIC_W - 4 - mw;
        }
        if (my + mh > R01S_LOGIC_H - 4) {
            my = R01S_LOGIC_H - 4 - mh;
        }
        fill_rect(r, mx, my, mw, mh, 24, 28, 22);
        draw_rect(r, mx, my, mw, mh, 180, 200, 160);
        font_draw(r, mx + 8, my + 7, item, 220, 230, 200);
    }

    {
        char tip[160];
        ui_fill_tooltip(ui, tip, sizeof(tip));
        if (tip[0]) {
            draw_tooltip(r, ui->mouse_lx, ui->mouse_ly, tip);
        }
    }
}

void r01s_ui_draw_boot(R01sUi *ui, SDL_Renderer *r, int spin_frame) {
    static const char spin_chars[] = {'|', '/', '-', '\\'};
    char line[48];
    char fps_buf[16];
    char spin;
    int cx;

    if (!ui || !r) {
        return;
    }
    if (spin_frame < 0) {
        spin_frame = 0;
    }
    spin = spin_chars[spin_frame & 3];

    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 0, 0, 0);
    snprintf(line, sizeof(line), "Booting console... %c", spin);
    cx = (R01S_LOGIC_W - font_text_width(line)) / 2;
    if (cx < 8) {
        cx = 8;
    }
    font_draw(r, cx, R01S_LOGIC_H / 2 - 4, line, 200, 210, 180);
    snprintf(fps_buf, sizeof(fps_buf), "%d FPS", ui->fps);
    font_draw(r, R01S_LOGIC_W - font_text_width(fps_buf) - 8, 7, fps_buf, 100, 110, 100);
    font_draw(r, 8, R01S_LOGIC_H - 16, "ESC QUIT", 90, 90, 90);
}

static int hit_chip(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    if (e->orient == R01S_ORIENT_H) {
        return lx >= x - 4 && lx < x + e->body_w + 4 && ly >= y - 12 && ly < y + e->body_h + 12;
    }
    return lx >= x - 12 && lx < x + e->body_w + 12 && ly >= y - 4 && ly < y + e->body_h + 4;
}

static int hit_island_frame(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    return lx >= x && lx < x + island->board_w && ly >= y && ly < y + island->board_h;
}

/* Returns corner id, or -1 if miss. */
static int hit_island_resize(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int hs = R01S_ISLAND_RESIZE_HANDLE;
    int right = x + island->board_w;
    int bottom = y + island->board_h;

    if (lx >= right - hs && lx < right && ly >= bottom - hs && ly < bottom) {
        return R01S_ISLAND_CORNER_BR;
    }
    if (lx >= x && lx < x + hs && ly >= bottom - hs && ly < bottom) {
        return R01S_ISLAND_CORNER_BL;
    }
    if (lx >= right - hs && lx < right && ly >= y && ly < y + hs) {
        return R01S_ISLAND_CORNER_TR;
    }
    if (lx >= x && lx < x + hs && ly >= y && ly < y + hs) {
        return R01S_ISLAND_CORNER_TL;
    }
    return -1;
}

/* Front-most island first (matches draw: higher index on top; active drag/resize on top). */
static int island_hit_stack(const R01sUi *ui, int *out_idx, int max_out) {
    int n;
    int front;
    int i;
    int k = 0;

    if (!ui || !ui->group || !out_idx || max_out <= 0) {
        return 0;
    }
    n = r01s_island_group_count(ui->group);
    if (n > max_out) {
        n = max_out;
    }
    front = -1;
    if (ui->drag_island >= 0) {
        front = ui->drag_island;
    } else if (ui->resize_island >= 0) {
        front = ui->resize_island;
    }
    if (front >= 0 && front < n) {
        out_idx[k++] = front;
    }
    for (i = n - 1; i >= 0; i--) {
        if (i == front) {
            continue;
        }
        out_idx[k++] = i;
    }
    return k;
}

/* Topmost chip of this island under (lx,ly), or -1. */
static int hit_chip_in_island(const R01sUi *ui, int island_index, int lx, int ly) {
    int i;
    for (i = ui->chip_count - 1; i >= 0; i--) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (ui->chips[i] && hit_chip(ui, ui->chips[i], lx, ly)) {
            return i;
        }
    }
    return -1;
}

/*
 * Board pick matching draw occlusion.
 * Returns: 0 miss, 1 chip (*chip_out), 2 move island (*island_out), 3 resize (*island_out, *corner_out).
 */
static int hit_board_top(const R01sUi *ui, int lx, int ly, int *chip_out, int *island_out, int *corner_out) {
    int stack[R01S_MAX_ISLANDS];
    int nstack;
    int s;

    if (chip_out) {
        *chip_out = -1;
    }
    if (island_out) {
        *island_out = -1;
    }
    if (corner_out) {
        *corner_out = -1;
    }
    if (!ui || !ui_logic_in_view(lx, ly)) {
        return 0;
    }

    if (ui->group && !ui->layout_compact) {
        nstack = island_hit_stack(ui, stack, R01S_MAX_ISLANDS);
        for (s = 0; s < nstack; s++) {
            int ii = stack[s];
            const R01sIsland *island = r01s_island_group_at(ui->group, ii);
            int corner;
            int chip_i;
            if (!island || !hit_island_frame(ui, island, lx, ly)) {
                continue;
            }
            /* This island fully occludes anything behind it. */
            corner = hit_island_resize(ui, island, lx, ly);
            if (corner >= 0) {
                if (island_out) {
                    *island_out = ii;
                }
                if (corner_out) {
                    *corner_out = corner;
                }
                return 3;
            }
            chip_i = hit_chip_in_island(ui, ii, lx, ly);
            if (chip_i >= 0) {
                if (chip_out) {
                    *chip_out = chip_i;
                }
                if (island_out) {
                    *island_out = ii;
                }
                return 1;
            }
            if (island_out) {
                *island_out = ii;
            }
            return 2;
        }
        return 0;
    }

    /* Compact (or no islands): chips only, last-drawn wins. */
    {
        int i;
        for (i = ui->chip_count - 1; i >= 0; i--) {
            if (ui->chips[i] && hit_chip(ui, ui->chips[i], lx, ly)) {
                if (chip_out) {
                    *chip_out = i;
                }
                return 1;
            }
        }
    }
    return 0;
}

int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y) {
    int i;
    int board_mx = 0;
    int board_my = 0;
    if (!ui || !e) {
        return 0;
    }
    ui->mouse_lx = logic_x;
    ui->mouse_ly = logic_y;
    if (ui_logic_in_view(logic_x, logic_y)) {
        ui_logic_to_board(ui, logic_x, logic_y, &board_mx, &board_my);
    }
    if (e->type == SDL_MOUSEWHEEL) {
        if (sidebar_hit(logic_x, logic_y)) {
            ui->sidebar_scroll -= e->wheel.y * R01S_UI_STATUS_ROW_H;
            sidebar_clamp_scroll(ui);
            return 1;
        }
        if (ui_logic_in_view(logic_x, logic_y)) {
            ui->pan_x -= e->wheel.x * 32;
            ui->pan_y -= e->wheel.y * 32;
            r01s_ui_clamp_pan(ui);
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        /* Chip context menu (orient); otherwise board pan. */
        if (ui_logic_in_view(logic_x, logic_y)) {
            int chip_i = -1;
            int kind = hit_board_top(ui, logic_x, logic_y, &chip_i, NULL, NULL);
            if (kind == 1 && chip_i >= 0 && chip_i < ui->chip_count && ui->chips[chip_i] &&
                ui->chips[chip_i]->visual == R01S_ENTITY_VIS_IC) {
                ui->ctx_chip = chip_i;
                ui->ctx_x = logic_x;
                ui->ctx_y = logic_y;
                if (ui->layout_compact) {
                    if (!ui->chip_sel[chip_i]) {
                        ui_sel_set_one(ui, chip_i);
                    } else {
                        ui->selected = chip_i;
                    }
                } else {
                    ui->selected = chip_i;
                }
                return 1;
            }
            ui->ctx_chip = -1;
            ui->drag_pan = 1;
            ui->drag_last_x = logic_x;
            ui->drag_last_y = logic_y;
            return 1;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        e->button.button == SDL_BUTTON_MIDDLE && ui_logic_in_view(logic_x, logic_y)) {
        ui->ctx_chip = -1;
        ui->drag_pan = 1;
        ui->drag_last_x = logic_x;
        ui->drag_last_y = logic_y;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP &&
        (e->button.button == SDL_BUTTON_MIDDLE || e->button.button == SDL_BUTTON_RIGHT)) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        if (ui_logic_in_view(logic_x, logic_y) || ui_logic_in_view(ui->drag_last_x, ui->drag_last_y)) {
            ui->pan_x -= (logic_x - ui->drag_last_x);
            ui->pan_y -= (logic_y - ui->drag_last_y);
            r01s_ui_clamp_pan(ui);
        }
        ui->drag_last_x = logic_x;
        ui->drag_last_y = logic_y;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_stick >= 0) {
        gp_stick_from_point(ui, &ui->gamepad[ui->drag_stick], ui->drag_stick, logic_x, logic_y);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->resize_island >= 0) {
        resize_island_drag(ui, ui->resize_island, board_mx, board_my);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_island >= 0) {
        move_island_drag(ui, ui->drag_island, board_mx, board_my);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->box_sel) {
        ui->box_bx1 = board_mx;
        ui->box_by1 = board_my;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        if (ui->layout_compact && ui_sel_count(ui) > 1) {
            move_selection_drag(ui, board_mx, board_my);
        } else {
            move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
        }
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        const Uint8 *mods = SDL_GetKeyboardState(NULL);
        int step = 48;
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_s) {
            ui_save_layout_now(ui);
            return 1;
        }
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_r) {
            if (r01s_ui_rotate_selected(ui)) {
                return 1;
            }
        }
        if (!(e->key.keysym.mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_g) {
            if (r01s_board_from_group(ui->group)) {
                ui_toggle_lcd_scale(ui);
                return 1;
            }
        }
        if (mods[SDL_SCANCODE_LSHIFT] || mods[SDL_SCANCODE_RSHIFT]) {
            if (e->key.keysym.sym == SDLK_LEFT) {
                ui->pan_x -= step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHT) {
                ui->pan_x += step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_UP) {
                ui->pan_y -= step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_DOWN) {
                ui->pan_y += step;
                r01s_ui_clamp_pan(ui);
                return 1;
            }
        }
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        int was_layout_drag =
            (ui->drag_chip >= 0 || ui->drag_island >= 0 || ui->resize_island >= 0);
        if (ui->drag_stick >= 0) {
            ui->gamepad[ui->drag_stick].stick_x = 0;
            ui->gamepad[ui->drag_stick].stick_y = 0;
            ui->drag_stick = -1;
            return 1;
        }
        if (ui->drag_btn >= 0) {
            ui->mouse_btn[ui->drag_btn / 4][ui->drag_btn % 4] = 0;
            ui->drag_btn = -1;
            return 1;
        }
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
                ui_sel_from_box(ui, shift);
                snprintf(ui->status, sizeof(ui->status), "selected %d", ui_sel_count(ui));
            } else if (!shift) {
                ui_sel_clear(ui);
            }
            return 1;
        }
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;
        if (was_layout_drag) {
            ui->layout_dirty = 1;
        }
        return ui->selected >= 0 || ui_sel_count(ui) > 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int gp_player = 0;
        int gp_btn = -1;
        int hit;
        SDL_Rect copy_rc;

        /* Context menu: toggle package orientation. */
        if (ui->ctx_chip >= 0 && ui->ctx_chip < ui->chip_count) {
            const R01sEntity *ce = ui->chips[ui->ctx_chip];
            const char *item =
                (ce && ce->orient == R01S_ORIENT_V) ? "ORIENT HORIZONTAL" : "ORIENT VERTICAL";
            int mw = font_text_width(item) + 16;
            int mh = 22;
            int mx = ui->ctx_x;
            int my = ui->ctx_y;
            if (mx + mw > R01S_LOGIC_W - 4) {
                mx = R01S_LOGIC_W - 4 - mw;
            }
            if (my + mh > R01S_LOGIC_H - 4) {
                my = R01S_LOGIC_H - 4 - mh;
            }
            if (logic_x >= mx && logic_x < mx + mw && logic_y >= my && logic_y < my + mh) {
                ui->selected = ui->ctx_chip;
                r01s_ui_rotate_selected(ui);
                ui->ctx_chip = -1;
                return 1;
            }
            ui->ctx_chip = -1; /* click elsewhere dismisses */
        }

        /* Save layout (top HUD). */
        {
            SDL_Rect sbtn;
            save_btn_rect(ui, &sbtn);
            if (logic_x >= sbtn.x && logic_x < sbtn.x + sbtn.w && logic_y >= sbtn.y &&
                logic_y < sbtn.y + sbtn.h) {
                ui_save_layout_now(ui);
                return 1;
            }
        }

        /* Compact / Islands layout toggle (top HUD). */
        {
            SDL_Rect cbtn;
            compact_btn_rect(ui, &cbtn);
            if (logic_x >= cbtn.x && logic_x < cbtn.x + cbtn.w && logic_y >= cbtn.y &&
                logic_y < cbtn.y + cbtn.h) {
                ui_toggle_compact(ui);
                return 1;
            }
        }

        /* SCREEN render mode (left sidebar). */
        {
            SDL_Rect rc;
            int i;
            for (i = 0; i < R01S_UI_SCREEN_MODE_N; i++) {
                sidebar_screen_mode_rect(ui, i, &rc);
                if (sidebar_hit(logic_x, logic_y) && radio_hit(&rc, logic_x, logic_y)) {
                    ui_set_screen_render_mode(ui, i);
                    return 1;
                }
            }
        }

        /* SCREEN SCALE 1X/2X (left sidebar). */
        {
            SDL_Rect rc;
            sidebar_dual_radio_rect(ui, 0, 0, &rc);
            if (sidebar_hit(logic_x, logic_y) && radio_hit(&rc, logic_x, logic_y)) {
                ui_set_lcd_scale(ui, 0);
                return 1;
            }
            sidebar_dual_radio_rect(ui, 0, 1, &rc);
            if (sidebar_hit(logic_x, logic_y) && radio_hit(&rc, logic_x, logic_y)) {
                ui_set_lcd_scale(ui, 1);
                return 1;
            }
        }

        /* SIM PIN / FAST (left sidebar). */
        {
            SDL_Rect rc;
            sidebar_dual_radio_rect(ui, 1, 0, &rc);
            if (sidebar_hit(logic_x, logic_y) && radio_hit(&rc, logic_x, logic_y)) {
                ui_set_sim_fast(ui, 0);
                return 1;
            }
            sidebar_dual_radio_rect(ui, 1, 1, &rc);
            if (sidebar_hit(logic_x, logic_y) && radio_hit(&rc, logic_x, logic_y)) {
                ui_set_sim_fast(ui, 1);
                return 1;
            }
        }

        /* System / island COPY buttons (sidebar) before gamepad / board hits. */
        if (health_needs_debug(ui->health.system) || ui->health.system == R01S_HEALTH_BOOT) {
            health_system_copy_rect(ui, &copy_rc);
            if (sidebar_hit(logic_x, logic_y) && logic_x >= copy_rc.x && logic_x < copy_rc.x + copy_rc.w &&
                logic_y >= copy_rc.y && logic_y < copy_rc.y + copy_rc.h) {
                ui_copy_health_text(ui, ui->health.system_debug);
                return 1;
            }
        }
        for (i = 0; i < ui->health.island_count; i++) {
            if (!health_needs_debug(ui->health.islands[i].health)) {
                continue;
            }
            health_copy_btn_rect(ui, i, &copy_rc);
            if (!sidebar_hit(copy_rc.x + copy_rc.w / 2, copy_rc.y + copy_rc.h / 2)) {
                continue; /* scrolled out of sidebar viewport */
            }
            if (logic_x >= copy_rc.x && logic_x < copy_rc.x + copy_rc.w && logic_y >= copy_rc.y &&
                logic_y < copy_rc.y + copy_rc.h) {
                ui_copy_health_text(ui, ui->health.islands[i].debug);
                return 1;
            }
        }

        hit = gp_hit_any(ui, logic_x, logic_y, &gp_player, &gp_btn);
        if (hit == 2) {
            ui->drag_btn = gp_player * 4 + gp_btn;
            ui->mouse_btn[gp_player][gp_btn] = 1;
            return 1;
        }
        if (hit == 1) {
            ui->drag_stick = gp_player;
            gp_stick_from_point(ui, &ui->gamepad[gp_player], gp_player, logic_x, logic_y);
            return 1;
        }
        ui->selected = -1;
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;

        if (!ui_logic_in_view(logic_x, logic_y)) {
            return 1;
        }

        {
            int chip_i = -1;
            int island_i = -1;
            int corner = -1;
            int kind = hit_board_top(ui, logic_x, logic_y, &chip_i, &island_i, &corner);
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

            if (kind == 3 && island_i >= 0) {
                const R01sIsland *island = r01s_island_group_at(ui->group, island_i);
                ui_sel_clear(ui);
                ui->resize_island = island_i;
                ui->resize_corner = corner;
                snprintf(ui->status, sizeof(ui->status), "resize %s",
                         island && island->title ? island->title : "ISLAND");
                return 1;
            }
            if (kind == 1 && chip_i >= 0 && chip_i < ui->chip_count && ui->chips[chip_i]) {
                if (ui->layout_compact && shift) {
                    ui_sel_toggle(ui, chip_i);
                    snprintf(ui->status, sizeof(ui->status), "selected %d", ui_sel_count(ui));
                    return 1;
                }
                if (ui->layout_compact && ui->chip_sel[chip_i] && ui_sel_count(ui) > 1) {
                    /* Drag whole selection; keep multi-select. */
                    ui->selected = chip_i;
                    ui->drag_chip = chip_i;
                    ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                    ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                    ui_begin_sel_drag(ui, board_mx, board_my);
                    snprintf(ui->status, sizeof(ui->status), "drag %d chips", ui_sel_count(ui));
                    return 1;
                }
                if (ui->layout_compact) {
                    ui_sel_set_one(ui, chip_i);
                } else {
                    ui_sel_clear(ui);
                    ui->selected = chip_i;
                }
                ui->drag_chip = chip_i;
                ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                ui_begin_sel_drag(ui, board_mx, board_my);
                snprintf(ui->status, sizeof(ui->status), "drag %s (%s)  pins=%d",
                         ui->chips[chip_i]->refdes ? ui->chips[chip_i]->refdes : "?",
                         ui->chips[chip_i]->part ? ui->chips[chip_i]->part : "?",
                         ui->chips[chip_i]->pin_count);
                return 1;
            }
            if (kind == 2 && island_i >= 0) {
                const R01sIsland *island = r01s_island_group_at(ui->group, island_i);
                ui_sel_clear(ui);
                ui->drag_island = island_i;
                ui->drag_grab_bx = board_mx - island->board_x;
                ui->drag_grab_by = board_my - island->board_y;
                snprintf(ui->status, sizeof(ui->status), "move %s",
                         island && island->title ? island->title : "ISLAND");
                return 1;
            }
            /* Compact empty board: start marquee select. */
            if (ui->layout_compact) {
                if (!shift) {
                    ui_sel_clear(ui);
                }
                ui->box_sel = 1;
                ui->box_bx0 = ui->box_bx1 = board_mx;
                ui->box_by0 = ui->box_by1 = board_my;
                return 1;
            }
            ui_sel_clear(ui);
        }
        return 1;
    }
    return 0;
}
