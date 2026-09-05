#include "ui.h"
#include "ui_internal.h"

#include "atmega328p.h"
#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/frame_log.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define R01S_UI_ISLANDS_PAD 6
#define R01S_UI_ISLANDS_DOT 8
#define R01S_UI_ISLANDS_GAP 2
#define R01S_UI_ISLANDS_LETTER_W 8
#define R01S_UI_ISLANDS_ITEM_W (R01S_UI_ISLANDS_DOT + R01S_UI_ISLANDS_GAP + R01S_UI_ISLANDS_LETTER_W)
#define R01S_UI_ISLANDS_H (R01S_UI_ISLANDS_DOT + R01S_UI_ISLANDS_PAD * 2)
#define R01S_UI_LEGEND_COUNT 5
#define R01S_UI_LEGEND_PAD R01S_UI_ISLANDS_PAD
#define R01S_UI_LEGEND_SWATCH R01S_UI_ISLANDS_DOT
#define R01S_UI_LEGEND_GAP 5
#define R01S_UI_LEGEND_ROW_H R01S_UI_LEGEND_SWATCH
#define R01S_UI_LEGEND_ROW_GAP 2
#define R01S_UI_GP_OVERLAY_W 12
#define R01S_UI_GP_OVERLAY_H 5
#define R01S_UI_GP_OVERLAY_SCALE 2
#define R01S_UI_GP_OVERLAY_GAP 6
#define R01S_UI_GP_OVERLAY_MARGIN 8

/* Wave monitor: 8 digital lanes + analog mix (bottom-left). */
#define R01S_UI_WAVE_LANE_W 96
#define R01S_UI_WAVE_LANE_H 10
#define R01S_UI_WAVE_GAP 2
#define R01S_UI_WAVE_LABEL_W 18
#define R01S_UI_WAVE_MARGIN 8
#define R01S_UI_WAVE_LANES (R01S_APU_CH_N + 1) /* + MIX */
#define R01S_UI_SCREEN_MODE_N 3

static int health_needs_debug(R01sHealth h) {
    return h == R01S_HEALTH_WARN || h == R01S_HEALTH_FAIL;
}

void ui_tip_reset(R01sUi *ui, int mx, int my) {
    if (!ui) {
        return;
    }
    ui->tip_stable_mx = mx;
    ui->tip_stable_my = my;
    ui->tip_show_at = SDL_GetTicks() + R01S_UI_TOOLTIP_DELAY_MS;
}

static int islands_strip_w(const R01sSystemHealth *health) {
    int n = health ? health->island_count : 0;
    if (n <= 0) {
        return R01S_UI_ISLANDS_PAD * 2;
    }
    return R01S_UI_ISLANDS_PAD * 2 + n * R01S_UI_ISLANDS_ITEM_W - R01S_UI_ISLANDS_GAP;
}

static void islands_strip_screen_origin(const R01sUi *ui, int *sx, int *sy) {
    if (!ui) {
        if (sx) {
            *sx = R01S_UI_VIEW_X + R01S_UI_ISLANDS_STRIP_DEFAULT_X;
        }
        if (sy) {
            *sy = R01S_UI_VIEW_Y + R01S_UI_ISLANDS_STRIP_DEFAULT_Y;
        }
        return;
    }
    if (sx) {
        *sx = R01S_UI_VIEW_X + ui->islands_strip_x;
    }
    if (sy) {
        *sy = R01S_UI_VIEW_Y + ui->islands_strip_y;
    }
}

static void islands_strip_bounds(const R01sUi *ui, SDL_Rect *rc) {
    int sx, sy;
    if (!rc) {
        return;
    }
    islands_strip_screen_origin(ui, &sx, &sy);
    rc->x = sx;
    rc->y = sy;
    rc->w = islands_strip_w(ui ? &ui->health : NULL);
    rc->h = R01S_UI_ISLANDS_H;
}

static void islands_strip_clamp(R01sUi *ui) {
    int max_x;
    int max_y;
    if (!ui) {
        return;
    }
    max_x = R01S_UI_VIEW_W - islands_strip_w(&ui->health);
    max_y = R01S_UI_VIEW_H - R01S_UI_ISLANDS_H;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (ui->islands_strip_x < 0) {
        ui->islands_strip_x = 0;
    }
    if (ui->islands_strip_y < 0) {
        ui->islands_strip_y = 0;
    }
    if (ui->islands_strip_x > max_x) {
        ui->islands_strip_x = max_x;
    }
    if (ui->islands_strip_y > max_y) {
        ui->islands_strip_y = max_y;
    }
}

int ui_islands_strip_contains(const R01sUi *ui, int lx, int ly) {
    SDL_Rect rc;
    if (!ui) {
        return 0;
    }
    islands_strip_bounds(ui, &rc);
    return lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h;
}

void ui_islands_strip_clamp(R01sUi *ui) {
    islands_strip_clamp(ui);
}

static void islands_strip_item_rect(const R01sUi *ui, int island_index, SDL_Rect *rc) {
    int sx, sy;
    int x0;
    if (!ui || !rc) {
        return;
    }
    islands_strip_screen_origin(ui, &sx, &sy);
    x0 = sx + R01S_UI_ISLANDS_PAD + island_index * R01S_UI_ISLANDS_ITEM_W;
    rc->x = x0;
    rc->y = sy + R01S_UI_ISLANDS_PAD;
    rc->w = R01S_UI_ISLANDS_ITEM_W - R01S_UI_ISLANDS_GAP;
    rc->h = R01S_UI_ISLANDS_DOT;
}

static int islands_strip_hit(const R01sUi *ui, int lx, int ly) {
    int i;
    SDL_Rect strip;
    if (!ui || !ui_islands_strip_contains(ui, lx, ly)) {
        return -1;
    }
    islands_strip_bounds(ui, &strip);
    for (i = 0; i < ui->health.island_count; i++) {
        SDL_Rect item;
        islands_strip_item_rect(ui, i, &item);
        if (lx >= item.x && lx < item.x + item.w && ly >= item.y && ly < item.y + item.h) {
            return i;
        }
    }
    return -1;
}

int ui_lcd_scale_2x(const R01sUi *ui) {
    R01sBoard *board = ui ? r01s_board_from_group(ui->group) : NULL;
    return board ? r01s_video_sink_scale_2x(&board->video_sink) : 0;
}

int ui_screen_render_mode(const R01sUi *ui) {
    R01sBoard *board = ui ? r01s_board_from_group(ui->group) : NULL;
    return board ? r01s_video_sink_render_mode(&board->video_sink) : R01S_VIDEO_RENDER_DEFAULT;
}

void ui_set_lcd_scale(R01sUi *ui, int scale_2x) {
    R01sBoard *board;
    R01sVideoSink *sink;
    uint8_t touched[R01S_MAX_ISLANDS];
    if (!ui) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return;
    }
    sink = &board->video_sink;
    r01s_video_sink_set_scale_2x(sink, scale_2x ? 1 : 0);
    memset(touched, 0, sizeof(touched));
    {
        int i;
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *e = ui->chips[i];
            int ii;
            if (!e || e->visual != R01S_ENTITY_VIS_DISPLAY || !e->part ||
                strcmp(e->part, "SCREEN_SINK") != 0) {
                continue;
            }
            ii = (int)ui->chip_island[i];
            if (scale_2x && ii >= 0 && ii < R01S_MAX_ISLANDS) {
                touched[ii] = 1;
            }
        }
        if (scale_2x) {
            for (i = 0; i < R01S_MAX_ISLANDS; i++) {
                if (touched[i]) {
                    /* Grow the video island so the larger SCR1 body still fits. */
                    ui_expand_island_to_chips(ui, i);
                }
            }
        }
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *e = ui->chips[i];
            if (e && e->visual == R01S_ENTITY_VIS_DISPLAY && e->part &&
                strcmp(e->part, "SCREEN_SINK") == 0) {
                clamp_chip(ui, e, (int)ui->chip_island[i]);
            }
        }
    }
    ui->layout_dirty = 1;
    snprintf(ui->status, sizeof(ui->status), "SCALE %s (%dx%d preview, 256x240 field)",
             r01s_video_sink_scale_2x(sink) ? "2X" : "1X",
             r01s_video_sink_scale_2x(sink) ? R01S_VIDEO_W : R01S_LOGICAL_W,
             r01s_video_sink_scale_2x(sink) ? R01S_VIDEO_H : R01S_LOGICAL_H);
}

void ui_toggle_lcd_scale(R01sUi *ui) {
    ui_set_lcd_scale(ui, !ui_lcd_scale_2x(ui));
}

void ui_set_screen_render_mode(R01sUi *ui, int mode) {
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

static const char *health_island_title(const R01sUi *ui, int island_index) {
    const R01sIsland *island;
    if (!ui || !ui->group) {
        return "?";
    }
    island = r01s_island_group_at(ui->group, island_index);
    return island && island->title ? island->title : "?";
}

/* Sidebar row label: drop "ISLAND " and keep id + description. */
static const char *health_sidebar_island_label(const R01sUi *ui, int island_index) {
    const char *title = health_island_title(ui, island_index);
    if (strncmp(title, "ISLAND ", 7) == 0 && title[7]) {
        return title + 7;
    }
    return title;
}

static const char *health_sidebar_island_letter(const R01sUi *ui, int island_index) {
    static char letter[2];
    const char *title = health_island_title(ui, island_index);

    letter[0] = '?';
    letter[1] = '\0';
    if (strncmp(title, "ISLAND ", 7) == 0 && title[7]) {
        letter[0] = title[7];
    }
    return letter;
}

static void draw_islands_strip(SDL_Renderer *r, R01sUi *ui) {
    const R01sSystemHealth *health;
    int i;
    int sw;
    int sx, sy;

    if (!ui) {
        return;
    }
    islands_strip_clamp(ui);
    health = &ui->health;
    islands_strip_screen_origin(ui, &sx, &sy);
    sw = islands_strip_w(health);
    fill_rect_a(r, sx, sy, sw, R01S_UI_ISLANDS_H, 20, 24, 18, 200);
    draw_rect(r, sx, sy, sw, R01S_UI_ISLANDS_H, 70, 90, 75);

    for (i = 0; i < health->island_count; i++) {
        const R01sIslandHealth *ih = &health->islands[i];
        SDL_Rect item;
        int dot_x;
        int letter_x;

        islands_strip_item_rect(ui, i, &item);
        dot_x = item.x;
        letter_x = item.x + R01S_UI_ISLANDS_DOT + R01S_UI_ISLANDS_GAP;
        draw_health_dot(r, dot_x, item.y, ih->health);
        font_draw(r, letter_x, item.y, health_sidebar_island_letter(ui, i), 140, 155, 135);
    }
}

typedef struct UiLegendItem {
    R01sLevel lvl;
    R01sPinDir dir;
    const char *label;
} UiLegendItem;

static const UiLegendItem k_legend_items[R01S_UI_LEGEND_COUNT] = {
    {R01S_LVL_H, R01S_PIN_IN, "high"},
    {R01S_LVL_L, R01S_PIN_IN, "low"},
    {R01S_LVL_X, R01S_PIN_IN, "conflict"},
    {R01S_LVL_Z, R01S_PIN_IN, "Hi-Z"},
    {R01S_LVL_Z, R01S_PIN_NC, "Not conn"},
};

static int legend_row_text_y(int row_y) {
    int lh = font_line_h();
    int row_h = R01S_UI_LEGEND_ROW_H;

    if (lh >= row_h) {
        return row_y;
    }
    return row_y + (row_h - lh) / 2;
}

static void legend_item_rgb(const UiLegendItem *item, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (!item) {
        return;
    }
    pin_level_rgb(item->lvl, item->dir, r, g, b);
}

static int legend_strip_label_w(void) {
    int i;
    int max_w = 0;

    for (i = 0; i < R01S_UI_LEGEND_COUNT; i++) {
        int w = font_text_width(k_legend_items[i].label);
        if (w > max_w) {
            max_w = w;
        }
    }
    return max_w;
}

static int legend_strip_w(void) {
    return R01S_UI_LEGEND_PAD * 2 + R01S_UI_LEGEND_SWATCH + R01S_UI_LEGEND_GAP + legend_strip_label_w();
}

static int legend_strip_h(void) {
    return R01S_UI_LEGEND_PAD * 2 + R01S_UI_LEGEND_COUNT * R01S_UI_LEGEND_ROW_H +
           (R01S_UI_LEGEND_COUNT - 1) * R01S_UI_LEGEND_ROW_GAP;
}

static void legend_strip_screen_origin(const R01sUi *ui, int *sx, int *sy) {
    if (!ui) {
        if (sx) {
            *sx = R01S_UI_VIEW_X + R01S_UI_LEGEND_STRIP_DEFAULT_X;
        }
        if (sy) {
            *sy = R01S_UI_VIEW_Y + R01S_UI_LEGEND_STRIP_DEFAULT_Y;
        }
        return;
    }
    if (sx) {
        *sx = R01S_UI_VIEW_X + ui->legend_strip_x;
    }
    if (sy) {
        *sy = R01S_UI_VIEW_Y + ui->legend_strip_y;
    }
}

static void legend_strip_bounds(const R01sUi *ui, SDL_Rect *rc) {
    int sx, sy;

    if (!rc) {
        return;
    }
    legend_strip_screen_origin(ui, &sx, &sy);
    rc->x = sx;
    rc->y = sy;
    rc->w = legend_strip_w();
    rc->h = legend_strip_h();
}

static void legend_strip_clamp(R01sUi *ui) {
    int max_x;
    int max_y;

    if (!ui) {
        return;
    }
    max_x = R01S_UI_VIEW_W - legend_strip_w();
    max_y = R01S_UI_VIEW_H - legend_strip_h();
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (ui->legend_strip_x < 0) {
        ui->legend_strip_x = 0;
    }
    if (ui->legend_strip_y < 0) {
        ui->legend_strip_y = 0;
    }
    if (ui->legend_strip_x > max_x) {
        ui->legend_strip_x = max_x;
    }
    if (ui->legend_strip_y > max_y) {
        ui->legend_strip_y = max_y;
    }
}

int ui_legend_strip_contains(const R01sUi *ui, int lx, int ly) {
    SDL_Rect rc;

    if (!ui) {
        return 0;
    }
    legend_strip_bounds(ui, &rc);
    return lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h;
}

void ui_legend_strip_clamp(R01sUi *ui) {
    legend_strip_clamp(ui);
}

static void draw_legend_strip(SDL_Renderer *r, R01sUi *ui) {
    int sx, sy;
    int swatch_x;
    int label_x;
    int y;
    int i;
    int sw;
    int sh;

    if (!ui) {
        return;
    }
    legend_strip_clamp(ui);
    legend_strip_screen_origin(ui, &sx, &sy);
    sw = legend_strip_w();
    sh = legend_strip_h();
    fill_rect_a(r, sx, sy, sw, sh, 20, 24, 18, 200);
    draw_rect(r, sx, sy, sw, sh, 70, 90, 75);

    swatch_x = sx + R01S_UI_LEGEND_PAD;
    label_x = swatch_x + R01S_UI_LEGEND_SWATCH + R01S_UI_LEGEND_GAP;
    y = sy + R01S_UI_LEGEND_PAD;
    for (i = 0; i < R01S_UI_LEGEND_COUNT; i++) {
        const UiLegendItem *item = &k_legend_items[i];
        Uint8 pr, pg, pb;

        legend_item_rgb(item, &pr, &pg, &pb);
        fill_rect(r, swatch_x, y, R01S_UI_LEGEND_SWATCH, R01S_UI_LEGEND_SWATCH, pr, pg, pb);
        draw_rect(r, swatch_x, y, R01S_UI_LEGEND_SWATCH, R01S_UI_LEGEND_SWATCH, 40, 50, 42);
        font_draw(r, label_x, legend_row_text_y(y), item->label, 140, 155, 135);
        y += R01S_UI_LEGEND_ROW_H;
        if (i + 1 < R01S_UI_LEGEND_COUNT) {
            y += R01S_UI_LEGEND_ROW_GAP;
        }
    }
}

static void health_island_copy_line(const R01sUi *ui, int island_index, char *out, size_t out_len) {
    const R01sIslandHealth *ih;

    if (!ui || !out || out_len == 0 || island_index < 0 || island_index >= ui->health.island_count) {
        if (out && out_len > 0) {
            out[0] = '\0';
        }
        return;
    }
    ih = &ui->health.islands[island_index];
    if (health_needs_debug(ih->health) && ih->debug[0]) {
        snprintf(out, out_len, "%s", ih->debug);
    } else {
        snprintf(out, out_len, "%s %s: %s", health_sidebar_island_label(ui, island_index),
                 r01s_health_tag(ih->health), ih->activity[0] ? ih->activity : "?");
    }
}

static int health_copy_text(R01sUi *ui, const char *text, const char *status) {
    if (!ui || !text || !text[0]) {
        return 0;
    }
    if (SDL_SetClipboardText(text) != 0) {
        snprintf(ui->status, sizeof(ui->status), "clipboard failed: %s", SDL_GetError());
        return 1;
    }
    snprintf(ui->status, sizeof(ui->status), "%s", status ? status : "copied health to clipboard");
    return 1;
}

static int health_copy_island(R01sUi *ui, int island_index) {
    char line[R01S_HEALTH_DEBUG_LEN + 64];
    char status[96];
    const R01sIslandHealth *ih;

    if (!ui || island_index < 0 || island_index >= ui->health.island_count) {
        return 0;
    }
    ih = &ui->health.islands[island_index];
    health_island_copy_line(ui, island_index, line, sizeof(line));
    if (!line[0]) {
        return 0;
    }
    snprintf(status, sizeof(status), "clipboard: island %c %s", ih->letter ? ih->letter : '?',
             r01s_health_tag(ih->health));
    return health_copy_text(ui, line, status);
}

int ui_health_copy_at(R01sUi *ui, int lx, int ly) {
    int island_i = islands_strip_hit(ui, lx, ly);
    if (island_i < 0) {
        return 0;
    }
    return health_copy_island(ui, island_i);
}

int ui_chip_is_cart_flash(const R01sEntity *e) {
    if (!e) {
        return 0;
    }
    if (e->refdes && strcmp(e->refdes, "U40") == 0) {
        return 1;
    }
    return e->part && strcmp(e->part, "SST39SF040") == 0;
}

int ui_chip_is_cart_eeprom(const R01sEntity *e) {
    if (!e) {
        return 0;
    }
    if (e->refdes && strcmp(e->refdes, "U50") == 0) {
        return 1;
    }
    return e->part && strcmp(e->part, "24C64") == 0;
}

int ui_chip_is_controller_attiny(const R01sEntity *e) {
    if (!e || !e->part) {
        return 0;
    }
    if (strncmp(e->part, "ATtiny", 6) == 0 || strncmp(e->part, "ATTINY", 6) == 0) {
        return 1;
    }
    return 0;
}

void ui_chip_body_rgb(const R01sEntity *e, int selected, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (!r || !g || !b) {
        return;
    }
    if (ui_chip_is_cart_flash(e) || ui_chip_is_cart_eeprom(e)) {
        *r = R01S_UI_CHIP_CART_R;
        *g = R01S_UI_CHIP_CART_G;
        *b = R01S_UI_CHIP_CART_B;
        (void)selected;
        return;
    }
    if (ui_chip_is_controller_attiny(e)) {
        *r = R01S_UI_CHIP_ATTINY_R;
        *g = R01S_UI_CHIP_ATTINY_G;
        *b = R01S_UI_CHIP_ATTINY_B;
        (void)selected;
        return;
    }
    *r = selected ? (Uint8)40 : (Uint8)28;
    *g = selected ? (Uint8)48 : (Uint8)32;
    *b = selected ? (Uint8)36 : (Uint8)28;
}

int ui_chip_hidden(const R01sUi *ui, const R01sEntity *e) {
    (void)ui;
    (void)e;
    return 0;
}

static void draw_island_resize_grip(SDL_Renderer *r, int hx, int hy) {
    int sz = R01S_ISLAND_RESIZE_HANDLE;
    draw_rect(r, hx, hy, sz, sz, 255, 255, 255);
}

#define R01S_ISLAND_TITLE_PAUSE_MS 700
#define R01S_ISLAND_TITLE_SCROLL_PX_PER_SEC 28

static int rect_intersect(const SDL_Rect *a, const SDL_Rect *b, SDL_Rect *out) {
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);

    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }
    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return 1;
}

static int island_title_scroll_offset(const char *title, int view_w, int island_index) {
    const char *text = title && title[0] ? title : "ISLAND";
    int tw = font_text_width(text);
    int overflow = tw - view_w;
    Uint32 now_ms;
    Uint32 t;
    int scroll_ms;
    int pause_ms = R01S_ISLAND_TITLE_PAUSE_MS;
    int cycle;

    if (overflow <= 0) {
        return 0;
    }

    scroll_ms = (int)((long long)overflow * 1000 / R01S_ISLAND_TITLE_SCROLL_PX_PER_SEC);
    if (scroll_ms < 1) {
        scroll_ms = 1;
    }
    cycle = pause_ms + scroll_ms + pause_ms + scroll_ms;

    now_ms = SDL_GetTicks();
    t = (now_ms + (Uint32)island_index * 251u) % (Uint32)cycle;

    if (t < (Uint32)pause_ms) {
        return 0;
    }
    t -= (Uint32)pause_ms;
    if (t < (Uint32)scroll_ms) {
        return (int)((long long)t * overflow / scroll_ms);
    }
    t -= (Uint32)scroll_ms;
    if (t < (Uint32)pause_ms) {
        return overflow;
    }
    t -= (Uint32)pause_ms;
    return overflow - (int)((long long)t * overflow / scroll_ms);
}

static void draw_island_scrolling_title(SDL_Renderer *r, int title_x, int y, const char *title, int title_max,
                                        int island_index) {
    const char *text = title && title[0] ? title : "ISLAND";
    int scroll = island_title_scroll_offset(text, title_max, island_index);
    SDL_Rect title_clip = {title_x, y, title_max, R01S_ISLAND_HEADER_H};
    SDL_Rect prev_clip;
    SDL_Rect clip;
    SDL_bool had_clip = SDL_RenderIsClipEnabled(r);

    if (had_clip) {
        SDL_RenderGetClipRect(r, &prev_clip);
        if (!rect_intersect(&prev_clip, &title_clip, &clip)) {
            return;
        }
    } else {
        clip = title_clip;
    }

    SDL_RenderSetClipRect(r, &clip);
    font_draw(r, title_x - scroll, y + 1, text, 255, 255, 255);
    if (had_clip) {
        SDL_RenderSetClipRect(r, &prev_clip);
    } else {
        SDL_RenderSetClipRect(r, NULL);
    }
}

static void island_health_fill_rgb(R01sHealth h, Uint8 *R, Uint8 *G, Uint8 *B) {
    switch (h) {
    case R01S_HEALTH_WARN:
        *R = 96;
        *G = 78;
        *B = 18;
        break;
    case R01S_HEALTH_FAIL:
        *R = 96;
        *G = 28;
        *B = 28;
        break;
    case R01S_HEALTH_BOOT:
        *R = 22;
        *G = 58;
        *B = 40;
        break;
    default:
        *R = R01S_ISLAND_OK_R;
        *G = R01S_ISLAND_OK_G;
        *B = R01S_ISLAND_OK_B;
        break;
    }
}

static const char *island_status_tab_label(R01sHealth h) {
    switch (h) {
    case R01S_HEALTH_OK:
        return "OK";
    case R01S_HEALTH_WARN:
        return "WARN";
    case R01S_HEALTH_FAIL:
        return "ERR";
    default:
        return "BOOT";
    }
}

static void draw_island_frame(SDL_Renderer *r, const R01sUi *ui, const R01sIsland *island, int active,
                              const R01sIslandHealth *ih) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    R01sHealth health = ih ? ih->health : R01S_HEALTH_OK;
    Uint8 fr, fg, fb;

    (void)active;
    island_health_fill_rgb(health, &fr, &fg, &fb);
    fill_rect(r, x, y, island->board_w, island->board_h, fr, fg, fb);
}

/* Drawn after chips so title/status stay above packages. */
static void draw_island_header(SDL_Renderer *r, const R01sUi *ui, const R01sIsland *island,
                               const R01sIslandHealth *ih, int island_index) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int hs = R01S_ISLAND_RESIZE_HANDLE;
    R01sHealth health = ih ? ih->health : R01S_HEALTH_OK;
    Uint8 fr, fg, fb;
    const char *tag;
    int tab_w;
    int tag_w;
    int title_x;
    int title_max;

    island_health_fill_rgb(health, &fr, &fg, &fb);

    tag = island_status_tab_label(health);
    tag_w = font_text_width(tag);
    tab_w = tag_w + 8;
    if (tab_w < 24) {
        tab_w = 24;
    }

    fill_rect(r, x, y, tab_w, R01S_ISLAND_HEADER_H, fr, fg, fb);
    font_draw(r, x + (tab_w - tag_w) / 2, y + 1, tag, 255, 255, 255);

    title_x = x + tab_w + 4;
    title_max = island->board_w - tab_w - 8;
    if (title_max < 24) {
        title_max = 24;
    }
    draw_island_scrolling_title(r, title_x, y, island->title, title_max, island_index);

    draw_island_resize_grip(r, x + island->board_w - hs, y + island->board_h - hs);
}

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
    fill_rect(r, box_x, box_y, box_w, box_h, 255, 245, 180);
    draw_rect(r, box_x, box_y, box_w, box_h, 200, 180, 100);
    font_draw(r, box_x + pad, box_y + pad, text, 0, 0, 0);
}

static void ui_fill_tooltip(const R01sUi *ui, char *out, size_t out_len) {
    int chip_i = -1;
    int island_i = -1;
    int kind;
    int strip_i;

    if (!ui || !out || out_len == 0) {
        return;
    }
    out[0] = '\0';

    if ((SDL_GetModState() & KMOD_CTRL) && ui_logic_in_view(ui->mouse_lx, ui->mouse_ly) &&
        hit_board_top(ui, ui->mouse_lx, ui->mouse_ly, &chip_i, NULL, NULL) == 1 && chip_i >= 0 &&
        chip_i < ui->chip_count && ui->chips[chip_i] &&
        ui->chips[chip_i]->visual == R01S_ENTITY_VIS_IC) {
        int n = ui_ic_connected_peers(NULL, ui, chip_i);
        if (n == 1) {
            snprintf(out, out_len, "1 IC connected");
        } else {
            snprintf(out, out_len, "%d ICs connected", n);
        }
        return;
    }

    if (SDL_GetTicks() < ui->tip_show_at) {
        return;
    }
    if (ui->mouse_lx != ui->tip_stable_mx || ui->mouse_ly != ui->tip_stable_my) {
        return;
    }

    strip_i = islands_strip_hit(ui, ui->mouse_lx, ui->mouse_ly);
    if (strip_i >= 0) {
        snprintf(out, out_len, "click to copy");
        return;
    }

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

static void gp_overlay_origin(int player, int *ox, int *oy) {
    int panel_h = R01S_UI_GP_OVERLAY_H * R01S_UI_GP_OVERLAY_SCALE;
    int total_h = 2 * panel_h + R01S_UI_GP_OVERLAY_GAP;
    int base_x =
        R01S_UI_VIEW_X + R01S_UI_VIEW_W - R01S_UI_GP_OVERLAY_W * R01S_UI_GP_OVERLAY_SCALE - R01S_UI_GP_OVERLAY_MARGIN;
    int base_y = R01S_UI_VIEW_Y + R01S_UI_VIEW_H - total_h - R01S_UI_GP_OVERLAY_MARGIN;

    if (ox) {
        *ox = base_x;
    }
    if (oy) {
        *oy = base_y + player * (panel_h + R01S_UI_GP_OVERLAY_GAP);
    }
}

static void gp_overlay_pixel(SDL_Renderer *r, int ox, int oy, int px, int py, int pressed) {
    int sx = ox + px * R01S_UI_GP_OVERLAY_SCALE;
    int sy = oy + py * R01S_UI_GP_OVERLAY_SCALE;
    if (pressed) {
        fill_rect(r, sx, sy, R01S_UI_GP_OVERLAY_SCALE, R01S_UI_GP_OVERLAY_SCALE, 40, 220, 90);
    } else {
        fill_rect(r, sx, sy, R01S_UI_GP_OVERLAY_SCALE, R01S_UI_GP_OVERLAY_SCALE, 30, 30, 30);
    }
}

static void draw_controller_overlay(SDL_Renderer *r, int player, const R01sGamepadInput *gp, int input_mode) {
    int ox, oy;
    int x, y;
    uint8_t stick;
    int up, down, left, right;

    if (!r || !gp) {
        return;
    }
    gp_overlay_origin(player, &ox, &oy);
    if (player == 0) {
        const char *tag = (input_mode == R01S_INPUT_PADS) ? "PADS" : "ARCADE";
        font_draw(r, ox, oy - font_line_h() - 2, tag, 140, 160, 190);
    }
    for (y = 0; y < R01S_UI_GP_OVERLAY_H; y++) {
        for (x = 0; x < R01S_UI_GP_OVERLAY_W; x++) {
            fill_rect(r, ox + x * R01S_UI_GP_OVERLAY_SCALE, oy + y * R01S_UI_GP_OVERLAY_SCALE,
                      R01S_UI_GP_OVERLAY_SCALE, R01S_UI_GP_OVERLAY_SCALE, 180, 180, 180);
        }
    }
    stick = r01s_gamepad_stick_bits(gp->stick_x, gp->stick_y);
    up = (stick & R01S_PAD_UP) != 0;
    down = (stick & R01S_PAD_DOWN) != 0;
    left = (stick & R01S_PAD_LEFT) != 0;
    right = (stick & R01S_PAD_RIGHT) != 0;
    gp_overlay_pixel(r, ox, oy, 2, 1, up);
    gp_overlay_pixel(r, ox, oy, 1, 2, left);
    gp_overlay_pixel(r, ox, oy, 3, 2, right);
    gp_overlay_pixel(r, ox, oy, 2, 3, down);
    gp_overlay_pixel(r, ox, oy, 5, 2, gp->btn_coin);
    gp_overlay_pixel(r, ox, oy, 7, 2, gp->btn_start);
    gp_overlay_pixel(r, ox, oy, 9, 2, gp->btn_x);
    gp_overlay_pixel(r, ox, oy, 10, 3, gp->btn_y);
}

static void wave_monitor_origin(int *ox, int *oy) {
    int total_h = R01S_UI_WAVE_LANES * (R01S_UI_WAVE_LANE_H + R01S_UI_WAVE_GAP) + font_line_h() + 4;
    if (ox) {
        *ox = R01S_UI_VIEW_X + R01S_UI_WAVE_MARGIN;
    }
    if (oy) {
        *oy = R01S_UI_VIEW_Y + R01S_UI_VIEW_H - total_h - R01S_UI_WAVE_MARGIN;
    }
}

static void wave_lane_rgb(int ch, Uint8 *r, Uint8 *g, Uint8 *b) {
    static const Uint8 RGB[R01S_UI_WAVE_LANES][3] = {
        {80, 200, 120},  /* B1 pulse */
        {80, 180, 200},  /* B2 pulse */
        {200, 160, 80},  /* B3 triangle */
        {180, 120, 200}, /* B4 noise */
        {200, 100, 100}, /* B5 DPCM */
        {120, 200, 160}, /* S6 */
        {120, 160, 200}, /* S7 */
        {160, 140, 200}, /* S8 */
        {220, 220, 180}, /* MIX */
    };
    int i = ch;
    if (i < 0) {
        i = 0;
    }
    if (i >= R01S_UI_WAVE_LANES) {
        i = R01S_UI_WAVE_LANES - 1;
    }
    *r = RGB[i][0];
    *g = RGB[i][1];
    *b = RGB[i][2];
}

static const char *wave_lane_label(int ch) {
    static const char *const LABELS[R01S_UI_WAVE_LANES] = {"B1", "B2", "B3", "B4", "B5", "S6", "S7", "S8", "A"};
    if (ch < 0 || ch >= R01S_UI_WAVE_LANES) {
        return "?";
    }
    return LABELS[ch];
}

static void draw_wave_lane_math(SDL_Renderer *r, int lx, int ly, int lw, int lh, const R01sApuVoice *v,
                                Uint8 cr, Uint8 cg, Uint8 cb) {
    int x;
    int mid = ly + lh / 2;
    int prev_y = mid;
    fill_rect(r, lx, ly, lw, lh, 18, 20, 22);
    draw_rect(r, lx, ly, lw, lh, 40, 44, 48);
    /* Center line. */
    fill_rect(r, lx, mid, lw, 1, 32, 36, 40);
    for (x = 0; x < lw; x++) {
        int amp = r01s_apu_voice_wave_y(v, x, lw);
        int y = mid - (amp * (lh / 2 - 1)) / 128;
        if (y < ly + 1) {
            y = ly + 1;
        }
        if (y > ly + lh - 2) {
            y = ly + lh - 2;
        }
        if (x > 0) {
            int y0 = prev_y;
            int y1 = y;
            int step = (y1 >= y0) ? 1 : -1;
            int yy;
            for (yy = y0; yy != y1; yy += step) {
                fill_rect(r, lx + x, yy, 1, 1, cr, cg, cb);
            }
        }
        fill_rect(r, lx + x, y, 1, 1, cr, cg, cb);
        prev_y = y;
    }
}

static void draw_wave_lane_scope(SDL_Renderer *r, int lx, int ly, int lw, int lh, const uint8_t *scope, int n,
                                 Uint8 cr, Uint8 cg, Uint8 cb) {
    int x;
    int mid = ly + lh / 2;
    int prev_y = mid;
    fill_rect(r, lx, ly, lw, lh, 18, 20, 22);
    draw_rect(r, lx, ly, lw, lh, 40, 44, 48);
    fill_rect(r, lx, mid, lw, 1, 32, 36, 40);
    if (!scope || n < 2) {
        return;
    }
    for (x = 0; x < lw; x++) {
        int idx = (x * (n - 1)) / (lw > 1 ? lw - 1 : 1);
        int amp = (int)scope[idx] - 128;
        int y = mid - (amp * (lh / 2 - 1)) / 128;
        if (y < ly + 1) {
            y = ly + 1;
        }
        if (y > ly + lh - 2) {
            y = ly + lh - 2;
        }
        if (x > 0) {
            int y0 = prev_y;
            int y1 = y;
            int step = (y1 >= y0) ? 1 : -1;
            int yy;
            for (yy = y0; yy != y1; yy += step) {
                fill_rect(r, lx + x, yy, 1, 1, cr, cg, cb);
            }
        }
        fill_rect(r, lx + x, y, 1, 1, cr, cg, cb);
        prev_y = y;
    }
}

static void draw_wave_monitor(SDL_Renderer *r, R01sUi *ui) {
    R01sBoard *board;
    const R01sAtmega328p *apu;
    int ox, oy;
    int ch;
    uint8_t scope[R01S_APU_SCOPE_N];
    int scope_n;
    int panel_w;
    int panel_h;

    if (!r || !ui) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return;
    }
    apu = &board->apu;
    wave_monitor_origin(&ox, &oy);
    panel_w = R01S_UI_WAVE_LABEL_W + R01S_UI_WAVE_LANE_W + 6;
    panel_h = font_line_h() + 2 + R01S_UI_WAVE_LANES * (R01S_UI_WAVE_LANE_H + R01S_UI_WAVE_GAP);
    fill_rect(r, ox - 2, oy - 2, panel_w + 4, panel_h + 4, 10, 12, 14);
    draw_rect(r, ox - 2, oy - 2, panel_w + 4, panel_h + 4, 50, 55, 60);
    font_draw(r, ox, oy, "WAVE", 160, 170, 180);

    scope_n = r01s_atmega328p_scope_copy(apu, scope, R01S_APU_SCOPE_N);

    for (ch = 0; ch < R01S_APU_CH_N; ch++) {
        const R01sApuVoice *v = r01s_atmega328p_voice(apu, ch);
        Uint8 cr, cg, cb;
        int ly = oy + font_line_h() + 2 + ch * (R01S_UI_WAVE_LANE_H + R01S_UI_WAVE_GAP);
        int lx = ox + R01S_UI_WAVE_LABEL_W;
        wave_lane_rgb(ch, &cr, &cg, &cb);
        font_draw(r, ox, ly + 1, wave_lane_label(ch), cr, cg, cb);
        draw_wave_lane_math(r, lx, ly, R01S_UI_WAVE_LANE_W, R01S_UI_WAVE_LANE_H, v, cr, cg, cb);
    }
    {
        Uint8 cr, cg, cb;
        int ly = oy + font_line_h() + 2 + R01S_APU_CH_N * (R01S_UI_WAVE_LANE_H + R01S_UI_WAVE_GAP);
        int lx = ox + R01S_UI_WAVE_LABEL_W;
        wave_lane_rgb(R01S_APU_CH_N, &cr, &cg, &cb);
        font_draw(r, ox, ly + 1, wave_lane_label(R01S_APU_CH_N), cr, cg, cb);
        draw_wave_lane_scope(r, lx, ly, R01S_UI_WAVE_LANE_W, R01S_UI_WAVE_LANE_H, scope, scope_n, cr, cg, cb);
    }
    (void)ui;
}

void draw_video_pixels(SDL_Renderer *r, R01sUi *ui, R01sVideoSink *sink, int px, int py, int dw, int dh) {
    const uint8_t *rgb;
    SDL_Rect dst;

    if (!r || !ui || !sink || dw < 1 || dh < 1) {
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
    dst.w = dw;
    dst.h = dh;
    if (r01s_video_sink_scale_2x(sink)) {
        SDL_RenderCopy(r, ui->lcd_tex, NULL, &dst);
    } else {
        SDL_Rect src = {R01S_SCALE_1X_OX, R01S_SCALE_1X_OY, R01S_LOGICAL_W, R01S_LOGICAL_H};
        SDL_RenderCopy(r, ui->lcd_tex, &src, &dst);
    }
}

static void quit_modal_layout(SDL_Rect *panel, SDL_Rect *save, SDL_Rect *discard, SDL_Rect *cancel) {
    const char *labels[3] = {"Save", "Don't Save", "Cancel"};
    int btn_w[3];
    int total_w = 0;
    int gap = R01S_UI_UNIT;
    int i;
    int x;
    int panel_w = 380;
    int panel_h = 108;
    int btn_h = R01S_UI_UNIT * 2;

    for (i = 0; i < 3; i++) {
        btn_w[i] = font_text_width(labels[i]) + R01S_UI_UNIT * 2;
        total_w += btn_w[i];
    }
    total_w += gap * 2;
    if (panel) {
        panel->w = panel_w;
        panel->h = panel_h;
        panel->x = (R01S_LOGIC_W - panel_w) / 2;
        panel->y = (R01S_LOGIC_H - panel_h) / 2;
    }
    x = panel ? panel->x + (panel_w - total_w) / 2 : 0;
    if (save) {
        save->x = x;
        save->y = panel ? panel->y + panel_h - btn_h - R01S_UI_UNIT : 0;
        save->w = btn_w[0];
        save->h = btn_h;
        x += btn_w[0] + gap;
    }
    if (discard) {
        discard->x = x;
        discard->y = panel ? panel->y + panel_h - btn_h - R01S_UI_UNIT : 0;
        discard->w = btn_w[1];
        discard->h = btn_h;
        x += btn_w[1] + gap;
    }
    if (cancel) {
        cancel->x = x;
        cancel->y = panel ? panel->y + panel_h - btn_h - R01S_UI_UNIT : 0;
        cancel->w = btn_w[2];
        cancel->h = btn_h;
    }
}

static void draw_modal_button(SDL_Renderer *r, const SDL_Rect *rc, const char *label, int primary) {
    int tx;
    int ty;
    if (!r || !rc || !label) {
        return;
    }
    if (primary) {
        fill_rect(r, rc->x, rc->y, rc->w, rc->h, 70, 55, 30);
        draw_rect(r, rc->x, rc->y, rc->w, rc->h, 220, 180, 100);
    } else {
        fill_rect(r, rc->x, rc->y, rc->w, rc->h, 28, 40, 32);
        draw_rect(r, rc->x, rc->y, rc->w, rc->h, 120, 160, 130);
    }
    tx = rc->x + (rc->w - font_text_width(label)) / 2;
    ty = rc->y + (rc->h - font_line_h()) / 2;
    font_draw(r, tx, ty, label, primary ? 255 : 200, primary ? 230 : 220, primary ? 180 : 180);
}

static void draw_quit_modal(SDL_Renderer *r) {
    SDL_Rect panel;
    SDL_Rect save;
    SDL_Rect discard;
    SDL_Rect cancel;

    fill_rect_a(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 0, 0, 0, 160);
    quit_modal_layout(&panel, &save, &discard, &cancel);
    fill_rect(r, panel.x, panel.y, panel.w, panel.h, 20, 24, 18);
    draw_rect(r, panel.x, panel.y, panel.w, panel.h, 180, 200, 160);
    font_draw(r, panel.x + R01S_UI_UNIT, panel.y + R01S_UI_UNIT, "Unsaved layout", 255, 245, 180);
    font_draw(r, panel.x + R01S_UI_UNIT, panel.y + R01S_UI_UNIT + font_line_h() + 2,
              "Layout has changed. Save layout before closing?", 200, 210, 190);
    draw_modal_button(r, &save, "Save", 1);
    draw_modal_button(r, &discard, "Don't Save", 0);
    draw_modal_button(r, &cancel, "Cancel", 0);
}

void r01s_ui_modal_open_quit(R01sUi *ui) {
    if (!ui) {
        return;
    }
    ui->modal = R01S_UI_MODAL_QUIT;
    ui->modal_result = R01S_UI_MODAL_RES_NONE;
    ui->ctx_chip = -1;
}

void r01s_ui_modal_cancel(R01sUi *ui) {
    if (!ui) {
        return;
    }
    ui->modal = R01S_UI_MODAL_NONE;
    ui->modal_result = R01S_UI_MODAL_RES_CANCEL;
}

int r01s_ui_modal_active(const R01sUi *ui) {
    return ui && ui->modal != R01S_UI_MODAL_NONE;
}

int r01s_ui_modal_take_result(R01sUi *ui) {
    int res;
    if (!ui || ui->modal_result == R01S_UI_MODAL_RES_NONE) {
        return R01S_UI_MODAL_RES_NONE;
    }
    res = ui->modal_result;
    ui->modal = R01S_UI_MODAL_NONE;
    ui->modal_result = R01S_UI_MODAL_RES_NONE;
    return res;
}

int r01s_ui_modal_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y) {
    SDL_Rect panel;
    SDL_Rect save;
    SDL_Rect discard;
    SDL_Rect cancel;

    if (!ui || ui->modal == R01S_UI_MODAL_NONE) {
        return 0;
    }
    quit_modal_layout(&panel, &save, &discard, &cancel);
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            r01s_ui_modal_cancel(ui);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_RETURN) {
            ui->modal_result = R01S_UI_MODAL_RES_SAVE;
            return 1;
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (logic_x >= save.x && logic_x < save.x + save.w && logic_y >= save.y && logic_y < save.y + save.h) {
            ui->modal_result = R01S_UI_MODAL_RES_SAVE;
            return 1;
        }
        if (logic_x >= discard.x && logic_x < discard.x + discard.w && logic_y >= discard.y &&
            logic_y < discard.y + discard.h) {
            ui->modal_result = R01S_UI_MODAL_RES_DISCARD;
            return 1;
        }
        if (logic_x >= cancel.x && logic_x < cancel.x + cancel.w && logic_y >= cancel.y &&
            logic_y < cancel.y + cancel.h) {
            r01s_ui_modal_cancel(ui);
            return 1;
        }
        if (logic_x < panel.x || logic_x >= panel.x + panel.w || logic_y < panel.y || logic_y >= panel.y + panel.h) {
            r01s_ui_modal_cancel(ui);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEWHEEL) {
        return 1;
    }
    return 1;
}

void r01s_ui_sync_gamepads(R01sUi *ui) {
    const Uint8 *keys;
    if (!ui) {
        return;
    }
    keys = SDL_GetKeyboardState(NULL);

    /* P1: WASD move, 1/2 coin/start, G/H X/Y */
    ui->gamepad[0].stick_x = 0;
    ui->gamepad[0].stick_y = 0;
    if (keys[SDL_SCANCODE_W]) {
        ui->gamepad[0].stick_y = -R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_S]) {
        ui->gamepad[0].stick_y = R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_A]) {
        ui->gamepad[0].stick_x = -R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_D]) {
        ui->gamepad[0].stick_x = R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_W] && keys[SDL_SCANCODE_S]) {
        ui->gamepad[0].stick_y = 0;
    }
    if (keys[SDL_SCANCODE_A] && keys[SDL_SCANCODE_D]) {
        ui->gamepad[0].stick_x = 0;
    }
    ui->gamepad[0].btn_x = keys[SDL_SCANCODE_G];
    ui->gamepad[0].btn_y = keys[SDL_SCANCODE_H];
    ui->gamepad[0].btn_coin = keys[SDL_SCANCODE_1];
    ui->gamepad[0].btn_start = keys[SDL_SCANCODE_2];

    /* P2: arrows move, Shift/Enter coin/start, ,/. or numpad 1/2 X/Y */
    ui->gamepad[1].stick_x = 0;
    ui->gamepad[1].stick_y = 0;
    if (keys[SDL_SCANCODE_UP]) {
        ui->gamepad[1].stick_y = -R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_DOWN]) {
        ui->gamepad[1].stick_y = R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_LEFT]) {
        ui->gamepad[1].stick_x = -R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_RIGHT]) {
        ui->gamepad[1].stick_x = R01S_GAMEPAD_STICK_RADIUS;
    }
    if (keys[SDL_SCANCODE_UP] && keys[SDL_SCANCODE_DOWN]) {
        ui->gamepad[1].stick_y = 0;
    }
    if (keys[SDL_SCANCODE_LEFT] && keys[SDL_SCANCODE_RIGHT]) {
        ui->gamepad[1].stick_x = 0;
    }
    ui->gamepad[1].btn_x = keys[SDL_SCANCODE_COMMA] || keys[SDL_SCANCODE_KP_1];
    ui->gamepad[1].btn_y = keys[SDL_SCANCODE_PERIOD] || keys[SDL_SCANCODE_KP_2];
    ui->gamepad[1].btn_coin = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    ui->gamepad[1].btn_start = keys[SDL_SCANCODE_RETURN];
}

uint8_t r01s_ui_gamepad_port(const R01sUi *ui, int player) {
    if (!ui || player < 0 || player >= R01S_UI_GAMEPAD_COUNT) {
        return 0;
    }
    return r01s_gamepad_encode(&ui->gamepad[player]);
}

static void draw_frame_log_panel(SDL_Renderer *r, R01sUi *ui) {
    static const char *const cat_tag[R01S_FLOG_CAT_N] = {"SYS", "CPU", "FLH", "MAP", "VRM", "IO ",
                                                         "BEM", "BG0", "SPR", "VID", "PLY"};
    int page;
    int pages;
    int start;
    int i;
    int y;
    int panel_h;
    int line_h;
    char hdr[96];
    char body[R01S_FLOG_LINE_LEN + 16];

    (void)ui;
    if (!r01s_frame_log_enabled()) {
        return;
    }
    line_h = font_line_h();
    if (line_h < 10) {
        line_h = 10;
    }
    panel_h = 16 + (R01S_FLOG_PAGE_LINES + 1) * line_h + 8;
    if (panel_h > R01S_LOGIC_H - 24) {
        panel_h = R01S_LOGIC_H - 24;
    }
    fill_rect_a(r, 8, R01S_LOGIC_H - panel_h - 8, R01S_LOGIC_W - 16, panel_h, 8, 10, 14, 220);
    draw_rect(r, 8, R01S_LOGIC_H - panel_h - 8, R01S_LOGIC_W - 16, panel_h, 90, 110, 100);

    page = r01s_frame_log_page();
    pages = r01s_frame_log_page_count();
    snprintf(hdr, sizeof(hdr), "1_FRAME_DEBUG  %s  lines=%d  page %d/%d  [ ] PgUp/PgDn",
             r01s_frame_log_sealed() ? "SEALED" : (r01s_frame_log_active() ? "LOGGING" : "off"),
             r01s_frame_log_line_count(), page + 1, pages);
    font_draw(r, 14, R01S_LOGIC_H - panel_h - 4, hdr, 200, 220, 180);

    start = page * R01S_FLOG_PAGE_LINES;
    y = R01S_LOGIC_H - panel_h - 4 + line_h + 4;
    for (i = 0; i < R01S_FLOG_PAGE_LINES; i++) {
        const R01sFrameLogLine *ln = r01s_frame_log_line(start + i);
        uint8_t cr, cg, cb;
        const char *tag;
        if (!ln) {
            break;
        }
        r01s_frame_log_cat_rgb(ln->cat, &cr, &cg, &cb);
        tag = ((int)ln->cat >= 0 && ln->cat < R01S_FLOG_CAT_N) ? cat_tag[ln->cat] : "???";
        if (ln->count > 1) {
            snprintf(body, sizeof(body), "%s %s  x%u", tag, ln->text, (unsigned)ln->count);
        } else {
            snprintf(body, sizeof(body), "%s %s", tag, ln->text);
        }
        font_draw_ellipsize(r, 14, y, body, R01S_LOGIC_W - 36, cr, cg, cb);
        y += line_h;
    }
}

void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r) {
    SDL_Rect view_clip = {R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H};
    char fps_buf[16];

    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 12, 14, 16);
    fill_rect(r, R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H, R01S_BOARD_BG_R,
              R01S_BOARD_BG_G, R01S_BOARD_BG_B);

    SDL_RenderSetClipRect(r, &view_clip);

    /* Islands back->front as complete units so a front island fully occludes
     * anything behind it (frame fill + chips + header). Compact: chips only. */
    if (ui->group && !ui->layout_compact) {
        int rank;
        int n_islands = ui->island_z_count;
        if (n_islands <= 0) {
            n_islands = r01s_island_group_count(ui->group);
        }
        for (rank = 0; rank < n_islands; rank++) {
            const R01sIsland *island;
            const R01sIslandHealth *ih = NULL;
            int i;
            int active;
            int j;
            if (rank >= ui->island_z_count) {
                i = rank;
            } else {
                i = ui->island_z_order[rank];
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
                if (ui_chip_hidden(ui, ui->chips[j])) {
                    continue;
                }
                draw_board_item(r, ui, ui->chips[j], j == ui->selected);
            }
        }
        ui_draw_pin_wire_overlay(r, ui);
        for (rank = 0; rank < n_islands; rank++) {
            const R01sIsland *island;
            const R01sIslandHealth *ih = NULL;
            int i;
            if (rank >= ui->island_z_count) {
                i = rank;
            } else {
                i = ui->island_z_order[rank];
            }
            island = r01s_island_group_at(ui->group, i);
            if (!island) {
                continue;
            }
            if (i < ui->health.island_count) {
                ih = &ui->health.islands[i];
            }
            draw_island_header(r, ui, island, ih, i);
        }
    } else {
        int rank;
        int n_chips = ui->chip_z_count;
        if (n_chips <= 0) {
            n_chips = ui->chip_count;
        }
        for (rank = 0; rank < n_chips; rank++) {
            int ci = (rank < ui->chip_z_count) ? (int)ui->chip_z_order[rank] : rank;
            if (ci < 0 || ci >= ui->chip_count) {
                continue;
            }
            if (ui_chip_hidden(ui, ui->chips[ci])) {
                continue;
            }
            draw_board_item(r, ui, ui->chips[ci], ui->chip_sel[ci] || ci == ui->selected);
        }
        ui_draw_pin_wire_overlay(r, ui);
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

    /* Fixed HUD */
    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_UI_HUD_TOP, 12, 14, 16);
    font_draw(r, R01S_UI_UNIT, R01S_UI_UNIT, "Retr01 Sim", 200, 210, 220);
    {
        SDL_Rect ibtn;
        const char *hint;
        int hint_x;
        int hint_max;
        int text_y = R01S_UI_UNIT;
        input_mode_btn_rect(ui, &ibtn);
        hint_x = R01S_UI_VIEW_X + R01S_UI_UNIT;
        hint_max = ibtn.x - hint_x - R01S_UI_UNIT;
        if (hint_max < 24) {
            hint_max = 24;
        }
        hint = ui->layout_compact ? "BOX SEL  S SAVE  R ROT  Ctrl+. SORT  Ctrl+Z UNDO  DBL-CLK SCR"
                                  : "S SAVE  R ROT  DBL-CLK SCR SCALE  PAN  DRAG";
        font_draw_ellipsize(r, hint_x, text_y, hint, hint_max, 120, 130, 140);
    }
    {
        SDL_Rect ibtn;
        const char *ilabel = ui->input_mode == R01S_INPUT_PADS ? "PADS" : "ARCADE";
        input_mode_btn_rect(ui, &ibtn);
        fill_rect(r, ibtn.x, ibtn.y, ibtn.w, ibtn.h, ui->input_mode == R01S_INPUT_PADS ? 40 : 28,
                  ui->input_mode == R01S_INPUT_PADS ? 55 : 40, ui->input_mode == R01S_INPUT_PADS ? 70 : 32);
        draw_rect(r, ibtn.x, ibtn.y, ibtn.w, ibtn.h, 120, 140, 180);
        font_draw(r, ibtn.x + (ibtn.w - font_text_width(ilabel)) / 2, ibtn.y + (ibtn.h - font_line_h()) / 2, ilabel,
                  200, 210, 230);
    }
    {
        SDL_Rect sbtn;
        save_btn_rect(ui, &sbtn);
        fill_rect(r, sbtn.x, sbtn.y, sbtn.w, sbtn.h, ui->layout_dirty ? 70 : 28, ui->layout_dirty ? 55 : 40,
                  ui->layout_dirty ? 30 : 32);
        draw_rect(r, sbtn.x, sbtn.y, sbtn.w, sbtn.h, ui->layout_dirty ? 220 : 120, ui->layout_dirty ? 180 : 160,
                  ui->layout_dirty ? 100 : 130);
        font_draw(r, sbtn.x + (sbtn.w - font_text_width("SAVE")) / 2, sbtn.y + (sbtn.h - font_line_h()) / 2, "SAVE",
                  ui->layout_dirty ? 255 : 200, ui->layout_dirty ? 230 : 220, ui->layout_dirty ? 180 : 180);
    }
    {
        SDL_Rect cbtn;
        const char *clabel = ui->layout_compact ? "ISLANDS" : "COMPACT";
        compact_btn_rect(ui, &cbtn);
        fill_rect(r, cbtn.x, cbtn.y, cbtn.w, cbtn.h, ui->layout_compact ? 40 : 28, ui->layout_compact ? 70 : 40,
                  ui->layout_compact ? 50 : 32);
        draw_rect(r, cbtn.x, cbtn.y, cbtn.w, cbtn.h, 120, 160, 130);
        font_draw(r, cbtn.x + (cbtn.w - font_text_width(clabel)) / 2, cbtn.y + (cbtn.h - font_line_h()) / 2, clabel, 200, 220,
                  180);
    }

    /* Floating overlays */
    draw_islands_strip(r, ui);
    draw_legend_strip(r, ui);
    draw_controller_overlay(r, 0, &ui->gamepad[0], ui->input_mode);
    draw_controller_overlay(r, 1, &ui->gamepad[1], ui->input_mode);
    draw_wave_monitor(r, ui);
    draw_frame_log_panel(r, ui);

    fill_rect(r, 0, R01S_LOGIC_H - R01S_UI_HUD_BOTTOM, R01S_LOGIC_W, R01S_UI_HUD_BOTTOM, 12, 14, 16);
    snprintf(fps_buf, sizeof(fps_buf), "%d FPS %d STP", ui->fps, ui->sim_steps);
    {
        int fps_w = font_text_width(fps_buf) + R01S_UI_UNIT;
        int status_y = R01S_LOGIC_H - font_line_h() - R01S_UI_UNIT;
        font_draw_ellipsize(r, R01S_UI_UNIT, status_y, ui->status, R01S_LOGIC_W - fps_w - R01S_UI_UNIT, 160, 170,
                            160);
        font_draw(r, R01S_LOGIC_W - fps_w, status_y, fps_buf, 160, 180, 160);
    }

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

    if (ui->modal != R01S_UI_MODAL_NONE) {
        if (ui->modal == R01S_UI_MODAL_QUIT) {
            draw_quit_modal(r);
        }
        return;
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
    if (cx < R01S_UI_UNIT) {
        cx = R01S_UI_UNIT;
    }
    font_draw(r, cx, R01S_LOGIC_H / 2 - 4, line, 200, 210, 180);
    snprintf(fps_buf, sizeof(fps_buf), "%d FPS", ui->fps);
    font_draw(r, R01S_LOGIC_W - font_text_width(fps_buf) - R01S_UI_UNIT, R01S_UI_UNIT, fps_buf, 100, 110, 100);
    font_draw(r, R01S_UI_UNIT, R01S_LOGIC_H - font_line_h() - R01S_UI_UNIT, "ESC QUIT", 90, 90, 90);
}

