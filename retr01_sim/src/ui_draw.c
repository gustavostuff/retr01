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

#define R01S_UI_SIDEBAR_PAD 2
#define R01S_UI_SIDEBAR_INSET 2
#define R01S_UI_SCROLL_W 4
#define R01S_UI_SIDEBAR_X R01S_UI_SIDEBAR_PAD
#define R01S_UI_SIDEBAR_W (R01S_UI_SIDEBAR_L - R01S_UI_SIDEBAR_PAD * 2 - R01S_UI_SCROLL_W)
#define R01S_UI_SIDEBAR_TX (R01S_UI_SIDEBAR_X + R01S_UI_SIDEBAR_INSET)
#define R01S_UI_SIDEBAR_IW (R01S_UI_SIDEBAR_W - R01S_UI_SIDEBAR_INSET * 2)
#define R01S_UI_SCROLL_X (R01S_UI_SIDEBAR_L - R01S_UI_SIDEBAR_PAD - R01S_UI_SCROLL_W)
#define R01S_UI_SIDEBAR_TOP (R01S_UI_HUD_TOP + R01S_UI_UNIT)
#define R01S_UI_SIDEBAR_BOTTOM (R01S_LOGIC_H - R01S_UI_HUD_BOTTOM - R01S_UI_UNIT)
#define R01S_UI_SIDEBAR_VIEW_H (R01S_UI_SIDEBAR_BOTTOM - R01S_UI_SIDEBAR_TOP)
#define R01S_UI_SEC_PAD R01S_UI_UNIT
#define R01S_UI_STATUS_HDR_H 30
#define R01S_UI_STATUS_ROW_H 16
#define R01S_UI_STATUS_FOOTER_H 12
#define R01S_UI_PROBE_H 148
#define R01S_UI_PIN_QUIET_BTN_H 16
#define R01S_UI_PROBE_ROW 14
#define R01S_UI_PROBE_LED_SZ 10
#define R01S_UI_PROBE_LED_LABEL 14
#define R01S_UI_CART_HDR_H R01S_UI_UNIT * 2
#define R01S_UI_CART_BTN_H 16
#define R01S_UI_CART_BTN_GAP 4
#define R01S_UI_CART_H (R01S_UI_CART_HDR_H + 2 * R01S_UI_CART_BTN_H + R01S_UI_CART_BTN_GAP)
#define R01S_UI_SCREEN_MODE_N 3
#define R01S_UI_PAD_BIT_STRIDE 11
#define R01S_UI_PAD_GROUP_GAP 8
#define GP_PANEL_GAP R01S_UI_SEC_PAD
#define GP_PANEL_W R01S_UI_SIDEBAR_IW
#define GP_PANEL_HDR_H 16
#define GP_DPAD_SZ 32
#define GP_BTN_SZ 16
#define GP_KNOB_SZ 16
#define GP_STICK_TRAVEL 8
#define GP_PANEL_BOTTOM_PAD R01S_UI_UNIT
#define GP_PANEL_H (GP_PANEL_HDR_H + GP_DPAD_SZ + GP_PANEL_BOTTOM_PAD)
#define GP_OFF_DPAD_Y GP_PANEL_HDR_H
#define GP_OFF_BTN_ROW (GP_PANEL_HDR_H + 16)
#define GP_OFF_C_X 40
#define GP_OFF_S_X 56
#define GP_OFF_X_X 80
#define GP_OFF_Y_X 96
#define GP_OFF_ACT_TOP GP_PANEL_HDR_H

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

int sidebar_probe_content_y(const R01sUi *ui) {
    return status_panel_h(ui ? &ui->health : NULL) + 1 + R01S_UI_SEC_PAD;
}

int sidebar_cart_content_y(const R01sUi *ui) {
    return sidebar_probe_content_y(ui) + R01S_UI_PROBE_H + 1 + R01S_UI_SEC_PAD;
}

static int sidebar_gp_content_y(const R01sUi *ui) {
    return sidebar_cart_content_y(ui) + R01S_UI_CART_H + 1 + R01S_UI_SEC_PAD;
}

static void sidebar_section_sep(SDL_Renderer *r, int y) {
    fill_rect(r, R01S_UI_SIDEBAR_X, y, R01S_UI_SIDEBAR_W, 1, 48, 62, 52);
}

/* Content-local Y → screen Y (accounts for sidebar scroll). */
int sidebar_sy(const R01sUi *ui, int content_y) {
    return R01S_UI_SIDEBAR_TOP + content_y - (ui ? ui->sidebar_scroll : 0);
}

static int sidebar_content_h(const R01sUi *ui) {
    if (!ui) {
        return 0;
    }
    return sidebar_gp_content_y(ui) + 2 * GP_PANEL_H + GP_PANEL_GAP;
}

static int sidebar_max_scroll(const R01sUi *ui) {
    int max_s = sidebar_content_h(ui) - R01S_UI_SIDEBAR_VIEW_H;
    return max_s > 0 ? max_s : 0;
}

void sidebar_clamp_scroll(R01sUi *ui) {
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

int sidebar_hit(int lx, int ly) {
    return lx >= 0 && lx < R01S_UI_SIDEBAR_L && ly >= R01S_UI_SIDEBAR_TOP && ly < R01S_UI_SIDEBAR_BOTTOM;
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
    if (!ui) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return;
    }
    sink = &board->video_sink;
    r01s_video_sink_set_scale_2x(sink, scale_2x ? 1 : 0);
    {
        int i;
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *e = ui->chips[i];
            if (e && e->visual == R01S_ENTITY_VIS_DISPLAY) {
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

int radio_hit(const SDL_Rect *rc, int mx, int my) {
    return rc && mx >= rc->x && mx < rc->x + rc->w && my >= rc->y && my < rc->y + rc->h;
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

static void health_island_row_tooltip(const R01sUi *ui, int island_index, char *out, size_t out_len) {
    const R01sIslandHealth *ih;

    if (!ui || !out || out_len == 0 || island_index < 0 || island_index >= ui->health.island_count) {
        return;
    }
    ih = &ui->health.islands[island_index];
    if (health_needs_debug(ih->health) && ih->debug[0]) {
        snprintf(out, out_len, "%s — %s", health_sidebar_island_label(ui, island_index), ih->debug);
    } else if (ih->activity[0]) {
        snprintf(out, out_len, "%s — %s", health_sidebar_island_label(ui, island_index), ih->activity);
    } else {
        snprintf(out, out_len, "%s", health_sidebar_island_label(ui, island_index));
    }
}

void health_island_row_rect(const R01sUi *ui, int island_index, SDL_Rect *rc) {
    int status_y = sidebar_sy(ui, sidebar_status_content_y());
    rc->x = R01S_UI_SIDEBAR_TX;
    rc->y = status_y + R01S_UI_STATUS_HDR_H + island_index * R01S_UI_STATUS_ROW_H;
    rc->w = R01S_UI_SIDEBAR_IW;
    rc->h = R01S_UI_STATUS_ROW_H;
}

void health_system_bar_rect(const R01sUi *ui, SDL_Rect *rc) {
    int status_y = sidebar_sy(ui, sidebar_status_content_y());
    rc->x = R01S_UI_SIDEBAR_TX;
    rc->y = status_y + 14;
    rc->w = R01S_UI_SIDEBAR_IW;
    rc->h = 14;
}

static void draw_system_health_panel(SDL_Renderer *r, R01sUi *ui, int py) {
    const R01sSystemHealth *health;
    int i;
    int px = R01S_UI_SIDEBAR_TX;
    int pw = R01S_UI_SIDEBAR_IW;
    int ph;
    Uint8 sr, sg, sb;
    char row[56];
    int tag_w;

    if (!ui) {
        return;
    }
    health = &ui->health;
    ph = status_panel_h(health);

    font_draw(r, px, py + 2, "STATUS", 190, 205, 180);

    health_rgb(health->system, &sr, &sg, &sb);
    fill_rect(r, px, py + 14, pw, 14, sr, sg, sb);
    font_draw_ellipsize(r, px + 4, py + 17, health->system_label[0] ? health->system_label : "?", pw - 8, 20, 24,
                        22);

    for (i = 0; i < health->island_count; i++) {
        const R01sIslandHealth *ih = &health->islands[i];
        int ry = py + R01S_UI_STATUS_HDR_H + i * R01S_UI_STATUS_ROW_H;

        draw_health_dot(r, px, ry + 4, ih->health);
        font_draw(r, px + 12, ry + 4, health_sidebar_island_letter(ui, i), 140, 155, 135);
        snprintf(row, sizeof(row), "%s", r01s_health_tag(ih->health));
        tag_w = font_text_width(row);
        font_draw(r, px + pw - tag_w, ry + 3, row, 140, 155, 135);
    }

    font_draw_ellipsize(r, px, py + ph - R01S_UI_STATUS_FOOTER_H + 1, "CLICK WARN TO COPY", pw, 100, 115, 100);
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
    if (!health_needs_debug(ih->health)) {
        return 0;
    }
    health_island_copy_line(ui, island_index, line, sizeof(line));
    snprintf(status, sizeof(status), "clipboard: island %c %s", ih->letter ? ih->letter : '?',
             r01s_health_tag(ih->health));
    return health_copy_text(ui, line, status);
}

static int health_copy_system_warnings(R01sUi *ui) {
    char buf[R01S_HEALTH_SYSTEM_DEBUG_LEN + 256];
    size_t used = 0;
    int count = 0;
    int i;

    if (!ui) {
        return 0;
    }
    if (health_needs_debug(ui->health.system)) {
        int n = snprintf(buf, sizeof(buf), "%s %s: %s\n", r01s_health_tag(ui->health.system),
                         ui->health.system_label[0] ? ui->health.system_label : "?",
                         ui->health.system_detail[0] ? ui->health.system_detail : "?");
        if (n > 0) {
            used = (size_t)n;
            count++;
        }
    }
    for (i = 0; i < ui->health.island_count; i++) {
        const R01sIslandHealth *ih = &ui->health.islands[i];
        char line[R01S_HEALTH_DEBUG_LEN + 64];
        int n;

        if (!health_needs_debug(ih->health)) {
            continue;
        }
        health_island_copy_line(ui, i, line, sizeof(line));
        if (!line[0]) {
            continue;
        }
        n = snprintf(buf + used, sizeof(buf) - used, "%s\n", line);
        if (n <= 0 || (size_t)n >= sizeof(buf) - used) {
            break;
        }
        used += (size_t)n;
        count++;
    }
    if (count == 0) {
        return 0;
    }
    while (used > 0 && (buf[used - 1] == '\n' || buf[used - 1] == '\r')) {
        buf[--used] = '\0';
    }
    {
        char status[96];
        snprintf(status, sizeof(status), "clipboard: %d health warning%s", count, count == 1 ? "" : "s");
        return health_copy_text(ui, buf, status);
    }
}

int ui_health_copy_at(R01sUi *ui, int lx, int ly) {
    SDL_Rect rc;
    int i;

    if (!ui || !sidebar_hit(lx, ly)) {
        return 0;
    }

    health_system_bar_rect(ui, &rc);
    if (lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h) {
        return health_copy_system_warnings(ui);
    }

    for (i = 0; i < ui->health.island_count; i++) {
        health_island_row_rect(ui, i, &rc);
        if (lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h) {
            return health_copy_island(ui, i);
        }
    }
    return 0;
}

static void draw_pad_bits_compact(SDL_Renderer *r, int x, int y, uint8_t bits) {
    static const int order[8] = {0, 1, 2, 3, 6, 7, 4, 5}; /* R L D U | C S | X Y */
    static const char *names[8] = {"R", "L", "D", "U", "C", "S", "X", "Y"};
    int i;
    int bx = x;
    for (i = 0; i < 8; i++) {
        int bit = order[i];
        int on = (bits & (1u << bit)) != 0;
        if (i == 4 || i == 6) {
            bx += R01S_UI_PAD_GROUP_GAP;
        }
        if (on) {
            fill_rect(r, bx - 1, y - 1, 7, 9, 0, 255, 0);
            font_draw(r, bx, y, names[i], 0, 0, 0);
        } else {
            font_draw(r, bx, y, names[i], 140, 150, 140);
        }
        bx += R01S_UI_PAD_BIT_STRIDE;
    }
}

static void draw_probe_led(SDL_Renderer *r, int x, int y, int on, Uint8 R, Uint8 G, Uint8 B, const char *label) {
    fill_rect(r, x, y + 1, R01S_UI_PROBE_LED_SZ, R01S_UI_PROBE_LED_SZ, on ? R : 30, on ? G : 30, on ? B : 30);
    draw_rect(r, x, y + 1, R01S_UI_PROBE_LED_SZ, R01S_UI_PROBE_LED_SZ, 200, 200, 200);
    font_draw(r, x + R01S_UI_PROBE_LED_LABEL, y + 2, label, 180, 180, 170);
}

static void draw_pin_swatch(SDL_Renderer *r, int x, int y, Uint8 R, Uint8 G, Uint8 B, const char *label) {
    fill_rect(r, x, y + 1, 7, 7, R, G, B);
    draw_rect(r, x, y + 1, 7, 7, 90, 100, 85);
    font_draw(r, x + 10, y + 2, label, 160, 175, 155);
}

void sidebar_probe_quiet_btn_rect(const R01sUi *ui, int probe_py, SDL_Rect *rc) {
    if (!rc) {
        return;
    }
    rc->x = R01S_UI_SIDEBAR_TX;
    rc->w = R01S_UI_SIDEBAR_IW;
    rc->h = R01S_UI_PIN_QUIET_BTN_H;
    rc->y = probe_py + R01S_UI_PROBE_H - rc->h - R01S_UI_SIDEBAR_INSET;
}

/* Cart ICs on island J: SST39SF040 (U40) and 24C64 (U50). */
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

int ui_chip_hidden(const R01sUi *ui, const R01sEntity *e) {
    if (!ui || !e) {
        return 0;
    }
    if (ui_chip_is_cart_flash(e) && !ui->show_cart_flash) {
        return 1;
    }
    if (ui_chip_is_cart_eeprom(e) && !ui->show_cart_eeprom) {
        return 1;
    }
    return 0;
}

void sidebar_cart_btn_rect(const R01sUi *ui, int which, SDL_Rect *rc) {
    /* which: 0 = FLASH, 1 = EEPROM */
    if (!rc) {
        return;
    }
    rc->x = R01S_UI_SIDEBAR_TX;
    rc->w = R01S_UI_SIDEBAR_IW;
    rc->h = R01S_UI_CART_BTN_H;
    rc->y = sidebar_sy(ui, sidebar_cart_content_y(ui) + R01S_UI_CART_HDR_H +
                                which * (R01S_UI_CART_BTN_H + R01S_UI_CART_BTN_GAP));
}

static void draw_cart_toggles(SDL_Renderer *r, const R01sUi *ui) {
    SDL_Rect rc;
    int cy = sidebar_sy(ui, sidebar_cart_content_y(ui));

    font_draw(r, R01S_UI_SIDEBAR_TX, cy + 2, "CART", 150, 160, 140);
    sidebar_cart_btn_rect(ui, 0, &rc);
    draw_segment_btn(r, &rc, ui->show_cart_flash, "FLASH");
    sidebar_cart_btn_rect(ui, 1, &rc);
    draw_segment_btn(r, &rc, ui->show_cart_eeprom, "24C64");
}

static void draw_live_probe(SDL_Renderer *r, const R01sUi *ui, int py) {
    int tx = R01S_UI_SIDEBAR_TX;
    int bits_x = tx + 10;
    int row;
    SDL_Rect quiet_rc;
    Uint8 pr, pg, pb;
    int btn_top = py + R01S_UI_PROBE_H - R01S_UI_PIN_QUIET_BTN_H - R01S_UI_SIDEBAR_INSET;

    font_draw(r, tx, py + 2, "PROBE", 200, 210, 180);
    row = py + 14;
    draw_probe_led(r, tx, row, ui->probe_vdd, 80, 220, 100, "VDD");
    row += R01S_UI_PROBE_ROW;
    draw_probe_led(r, tx, row, ui->probe_phi2, 220, 200, 60, "PHI2");
    row += R01S_UI_PROBE_ROW;
    draw_probe_led(r, tx, row, ui->probe_resb_low, 220, 80, 80, "RST");
    row += R01S_UI_PROBE_ROW + 4;
    font_draw(r, tx, row, "P1", 160, 180, 160);
    row += 10;
    draw_pad_bits_compact(r, bits_x, row, ui->probe_pad_p1);
    row += 12;
    font_draw(r, tx, row, "P2", 160, 180, 160);
    row += 10;
    draw_pad_bits_compact(r, bits_x, row, ui->probe_pad_p2);
    if (ui->pins_quiet) {
        draw_pin_swatch(r, tx, btn_top - 14, R01S_UI_PIN_GRAY_R, R01S_UI_PIN_GRAY_G, R01S_UI_PIN_GRAY_B, "GRAY");
    } else {
        font_draw(r, tx, btn_top - 28, "PINS", 200, 210, 180);
        pin_level_rgb(R01S_LVL_H, R01S_PIN_OUT, &pr, &pg, &pb);
        draw_pin_swatch(r, tx, btn_top - 14, pr, pg, pb, "HI");
        pin_level_rgb(R01S_LVL_L, R01S_PIN_OUT, &pr, &pg, &pb);
        draw_pin_swatch(r, tx + 44, btn_top - 14, pr, pg, pb, "LO");
    }
    quiet_rc.x = R01S_UI_SIDEBAR_TX;
    quiet_rc.y = btn_top;
    quiet_rc.w = R01S_UI_SIDEBAR_IW;
    quiet_rc.h = R01S_UI_PIN_QUIET_BTN_H;
    draw_segment_btn(r, &quiet_rc, ui->pins_quiet, "GRAY PINS");
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
    Uint8 br = 180, bg = 220, bb = 160;
    char badge[12];
    int tag_w;

    fill_rect(r, x + 2, y + 2, island->board_w - 4, R01S_ISLAND_HEADER_H, 22, 48, 32);
    tag_w = ih ? font_text_width(r01s_health_tag(ih->health)) + 14 : 0;
    title_max = island->board_w - tag_w - 12;
    if (title_max < 24) {
        title_max = 24;
    }
    font_draw_ellipsize(r, x + 4, y + 6, island->title ? island->title : "ISLAND", title_max, 180, 220, 160);
    if (ih) {
        health_rgb(ih->health, &br, &bg, &bb);
        snprintf(badge, sizeof(badge), "%s", r01s_health_tag(ih->health));
        font_draw(r, x + island->board_w - tag_w, y + 6, badge, br, bg, bb);
        draw_health_dot(r, x + island->board_w - 10, y + 7, ih->health);
    }
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
    fill_rect(r, box_x, box_y, box_w, box_h, 20, 24, 18);
    draw_rect(r, box_x, box_y, box_w, box_h, 180, 200, 160);
    font_draw(r, box_x + pad, box_y + pad, text, 220, 230, 200);
}

static void ui_fill_tooltip(const R01sUi *ui, char *out, size_t out_len) {
    int chip_i = -1;
    int island_i = -1;
    int kind;
    int i;

    if (!ui || !out || out_len == 0) {
        return;
    }
    out[0] = '\0';

    if (sidebar_hit(ui->mouse_lx, ui->mouse_ly)) {
        SDL_Rect rc;
        health_system_bar_rect(ui, &rc);
        if (ui->mouse_lx >= rc.x && ui->mouse_lx < rc.x + rc.w && ui->mouse_ly >= rc.y &&
            ui->mouse_ly < rc.y + rc.h) {
            if ((health_needs_debug(ui->health.system) || ui->health.system == R01S_HEALTH_BOOT) &&
                ui->health.system_debug[0]) {
                snprintf(out, out_len, "%s", ui->health.system_debug);
            } else if (ui->health.system_detail[0]) {
                snprintf(out, out_len, "%s", ui->health.system_detail);
            }
            return;
        }
        for (i = 0; i < ui->health.island_count; i++) {
            health_island_row_rect(ui, i, &rc);
            if (ui->mouse_lx >= rc.x && ui->mouse_lx < rc.x + rc.w && ui->mouse_ly >= rc.y &&
                ui->mouse_ly < rc.y + rc.h) {
                health_island_row_tooltip(ui, i, out, out_len);
                return;
            }
        }
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

static void gp_panel_origin(const R01sUi *ui, int player, int *px, int *py) {
    *px = R01S_UI_SIDEBAR_TX;
    *py = sidebar_sy(ui, sidebar_gp_content_y(ui) + player * (GP_PANEL_H + GP_PANEL_GAP));
}

static void gp_dpad_rect(const R01sUi *ui, int player, SDL_Rect *rc) {
    int px, py;
    gp_panel_origin(ui, player, &px, &py);
    if (!rc) {
        return;
    }
    rc->x = px;
    rc->y = py + GP_OFF_DPAD_Y;
    rc->w = GP_DPAD_SZ;
    rc->h = GP_DPAD_SZ;
}

static void gp_stick_center(const R01sUi *ui, int player, int *cx, int *cy) {
    SDL_Rect rc;
    gp_dpad_rect(ui, player, &rc);
    *cx = rc.x + GP_DPAD_SZ / 2;
    *cy = rc.y + GP_DPAD_SZ / 2;
}

static void gp_btn_rect(const R01sUi *ui, int player, int btn, SDL_Rect *rc) {
    int px, py;
    gp_panel_origin(ui, player, &px, &py);
    if (!rc) {
        return;
    }
    rc->w = GP_BTN_SZ;
    rc->h = GP_BTN_SZ;
    switch (btn) {
    case 0: /* X upper-left of action pair */
        rc->x = px + GP_OFF_X_X;
        rc->y = py + GP_OFF_ACT_TOP;
        break;
    case 1: /* Y lower-right of action pair */
        rc->x = px + GP_OFF_Y_X;
        rc->y = py + GP_OFF_BTN_ROW;
        break;
    case 2: /* C (select) */
        rc->x = px + GP_OFF_C_X;
        rc->y = py + GP_OFF_BTN_ROW;
        break;
    default: /* S (start) */
        rc->x = px + GP_OFF_S_X;
        rc->y = py + GP_OFF_BTN_ROW;
        break;
    }
}

static int gp_hit_stick(const R01sUi *ui, int player, int lx, int ly) {
    SDL_Rect rc;
    gp_dpad_rect(ui, player, &rc);
    return lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h;
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

int gp_hit_any(const R01sUi *ui, int lx, int ly, int *player_out, int *btn_out) {
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

void gp_stick_from_point(R01sUi *ui, R01sGamepadInput *gp, int player, int lx, int ly) {
    int cx, cy;
    gp_stick_center(ui, player, &cx, &cy);
    gp->stick_x = lx - cx;
    gp->stick_y = ly - cy;
    r01s_gamepad_stick_clamp(&gp->stick_x, &gp->stick_y, GP_STICK_TRAVEL);
    r01s_gamepad_stick_snap_digital(&gp->stick_x, &gp->stick_y, GP_STICK_TRAVEL, R01S_GAMEPAD_MOUSE_DEAD);
}

static void draw_dpad(SDL_Renderer *r, int x, int y, int sx, int sy) {
    int kx = x + GP_DPAD_SZ / 2 + sx - GP_KNOB_SZ / 2;
    int ky = y + GP_DPAD_SZ / 2 + sy - GP_KNOB_SZ / 2;
    fill_rect(r, x, y, GP_DPAD_SZ, GP_DPAD_SZ, 24, 28, 32);
    draw_rect(r, x, y, GP_DPAD_SZ, GP_DPAD_SZ, 70, 80, 90);
    fill_rect(r, kx, ky, GP_KNOB_SZ, GP_KNOB_SZ, 90, 100, 110);
    draw_rect(r, kx, ky, GP_KNOB_SZ, GP_KNOB_SZ, 140, 150, 160);
}

static void draw_btn(SDL_Renderer *r, const SDL_Rect *rc, int pressed, const char *label) {
    int tw;
    int tx;
    int ty;
    if (!r || !rc || !label) {
        return;
    }
    tw = font_text_width(label);
    tx = rc->x + (rc->w - tw) / 2;
    ty = rc->y + (rc->h - font_line_h()) / 2;
    fill_rect(r, rc->x, rc->y, rc->w, rc->h, pressed ? 90 : 40, pressed ? 120 : 48, pressed ? 160 : 56);
    draw_rect(r, rc->x, rc->y, rc->w, rc->h, 120, 130, 140);
    font_draw(r, tx, ty, label, 210, 210, 200);
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

static void draw_gamepad_panel(SDL_Renderer *r, const R01sUi *ui, int player) {
    int px, py, b;
    char hex[8];
    uint8_t bits;
    SDL_Rect brc;
    SDL_Rect dpad;

    gp_panel_origin(ui, player, &px, &py);
    bits = r01s_gamepad_encode(&ui->gamepad[player]);
    snprintf(hex, sizeof(hex), "%02X", bits);
    font_draw(r, px, py + 2, player == 0 ? "P1" : "P2", 180, 200, 220);
    font_draw(r, px + GP_PANEL_W - font_text_width(hex) - 0, py + 2, hex, 140, 160, 140);

    gp_dpad_rect(ui, player, &dpad);
    draw_dpad(r, dpad.x, dpad.y, ui->gamepad[player].stick_x, ui->gamepad[player].stick_y);

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
}

void r01s_ui_sync_gamepads(R01sUi *ui) {
    const Uint8 *keys;
    if (!ui) {
        return;
    }
    keys = SDL_GetKeyboardState(NULL);

    /* Keyboard only when that player's d-pad is not held with the mouse. */
    if (ui->drag_stick != 0) {
        ui->gamepad[0].stick_x = 0;
        ui->gamepad[0].stick_y = 0;
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
            ui->gamepad[0].stick_y = -GP_STICK_TRAVEL;
        }
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
            ui->gamepad[0].stick_y = GP_STICK_TRAVEL;
        }
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            ui->gamepad[0].stick_x = -GP_STICK_TRAVEL;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            ui->gamepad[0].stick_x = GP_STICK_TRAVEL;
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
            ui->gamepad[1].stick_y = -GP_STICK_TRAVEL;
        }
        if (keys[SDL_SCANCODE_K]) {
            ui->gamepad[1].stick_y = GP_STICK_TRAVEL;
        }
        if (keys[SDL_SCANCODE_J]) {
            ui->gamepad[1].stick_x = -GP_STICK_TRAVEL;
        }
        if (keys[SDL_SCANCODE_L]) {
            ui->gamepad[1].stick_x = GP_STICK_TRAVEL;
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
    int i;
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

    /* Islands back→front as complete units so a front island fully occludes
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
            draw_island_header(r, ui, island, ih);
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
    font_draw(r, R01S_UI_UNIT, R01S_UI_UNIT, "retr01 SIM", 200, 210, 220);
    {
        SDL_Rect sbtn;
        const char *hint;
        int hint_x;
        int hint_max;
        int text_y = R01S_UI_UNIT;
        save_btn_rect(ui, &sbtn);
        hint_x = R01S_UI_VIEW_X + R01S_UI_UNIT;
        hint_max = sbtn.x - hint_x - R01S_UI_UNIT;
        if (hint_max < 24) {
            hint_max = 24;
        }
        hint = ui->layout_compact ? "BOX SEL  S SAVE  R ROT  G SCALE" : "S SAVE  R ROT  G SCALE  PAN  DRAG";
        font_draw_ellipsize(r, hint_x, text_y, hint, hint_max, 120, 130, 140);
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

    /* Left sidebar: system status, probe, controllers (scrollable). */
    SDL_RenderSetClipRect(r, &sidebar_clip);
    {
        int status_h = status_panel_h(&ui->health);
        int status_py = sidebar_sy(ui, sidebar_status_content_y());
        int probe_cy = sidebar_probe_content_y(ui);
        int probe_py = sidebar_sy(ui, probe_cy);
        int cart_cy = sidebar_cart_content_y(ui);
        int cart_py = sidebar_sy(ui, cart_cy);
        int gp_cy = sidebar_gp_content_y(ui);
        int gp_py = sidebar_sy(ui, gp_cy);

        draw_system_health_panel(r, ui, status_py);
        sidebar_section_sep(r, status_py + status_h);
        draw_live_probe(r, ui, probe_py);
        sidebar_section_sep(r, probe_py + R01S_UI_PROBE_H);
        draw_cart_toggles(r, ui);
        sidebar_section_sep(r, cart_py + R01S_UI_CART_H);
        draw_gamepad_panel(r, ui, 0);
        sidebar_section_sep(r, gp_py + GP_PANEL_H);
        draw_gamepad_panel(r, ui, 1);
    }
    SDL_RenderSetClipRect(r, NULL);

    if (max_s > 0) {
        int track_x = R01S_UI_SCROLL_X;
        int track_y = R01S_UI_SIDEBAR_TOP;
        int track_h = R01S_UI_SIDEBAR_VIEW_H;
        int thumb_h = track_h * R01S_UI_SIDEBAR_VIEW_H / sidebar_content_h(ui);
        int thumb_y;
        if (thumb_h < 12) {
            thumb_h = 12;
        }
        thumb_y = track_y + (track_h - thumb_h) * ui->sidebar_scroll / max_s;
        fill_rect(r, track_x, track_y, R01S_UI_SCROLL_W, track_h, 28, 36, 30);
        fill_rect(r, track_x, thumb_y, R01S_UI_SCROLL_W, thumb_h, 90, 120, 95);
    }

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

