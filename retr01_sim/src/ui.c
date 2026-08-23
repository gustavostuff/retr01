#include "ui.h"

#include "retr01_sim/board_layout.h"

#include <stdio.h>
#include <string.h>

static void clamp_chip_in_island(R01sUi *ui, R01sEntity *e, int island_index);

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
    snprintf(ui->status, sizeof(ui->status), "islands A-E+G+H — drag empty / resize BR — SPACE run/pause");
    return 0;
}

void r01s_ui_shutdown(R01sUi *ui) {
    if (ui) {
        memset(ui, 0, sizeof(*ui));
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
    clamp_chip_in_island(ui, chip, island_index);
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
        *pr = 200;
        *pg = 80;
        *pb = 80;
        return;
    }
    switch (lvl) {
    case R01S_LVL_H:
        *pr = 80;
        *pg = 220;
        *pb = 100;
        break;
    case R01S_LVL_L:
        *pr = 40;
        *pg = 50;
        *pb = 45;
        break;
    case R01S_LVL_X:
        *pr = 220;
        *pg = 80;
        *pb = 200;
        break;
    default:
        *pr = 110;
        *pg = 110;
        *pb = 90;
        break;
    }
}

static int dip_pin_y(const R01sEntity *e, int pin_num, int *side_left) {
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int rows = dip / 2;
    int idx;

    if (dip <= 0 || pin_num <= 0 || pin_num > dip) {
        *side_left = 1;
        return e->board_y + e->body_h / 2;
    }
    *side_left = pin_num <= rows;
    idx = *side_left ? (pin_num - 1) : (dip - pin_num);
    /* Fixed pitch — body_h is derived from the same constants in r01s_entity_set_dip. */
    return e->board_y + R01S_DIP_PIN_MARGIN_Y + idx * R01S_DIP_PIN_PITCH;
}

static void draw_chip(SDL_Renderer *r, const R01sUi *ui, const R01sEntity *e, int selected) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    int i;
    int label_y;

    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int side_left;
        int py = ui_board_sy(ui, dip_pin_y(e, num, &side_left));
        int px0, px1;
        Uint8 pr, pg, pb;
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
        fill_rect(r, side_left ? px0 : px1 - 3, py - 1, 3, 3, pr, pg, pb);
    }

    fill_rect(r, x, y, e->body_w, e->body_h, 28, 32, 28);
    draw_rect(r, x, y, e->body_w, e->body_h, selected ? 255 : 140, selected ? 220 : 140,
              selected ? 80 : 120);
    fill_rect(r, x + e->body_w / 2 - 4, y - 2, 8, 4, 20, 22, 20);

    /* Labels inset below notch / top edge */
    label_y = y + 14;
    if (e->refdes) {
        font_draw(r, x + 6, label_y, e->refdes, 220, 220, 200);
    }
    if (e->part) {
        font_draw(r, x + 6, label_y + 12, e->part, 160, 180, 140);
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

static void clamp_chip_in_island(R01sUi *ui, R01sEntity *e, int island_index) {
    const R01sIsland *island;
    int min_x, min_y, max_x, max_y;
    int bx, by;

    if (!ui || !e) {
        return;
    }
    island = r01s_island_group_at(ui->group, island_index);
    if (!island) {
        return;
    }
    min_x = island->board_x + R01S_ISLAND_PAD_X + R01S_CHIP_PIN_OUT;
    min_y = island->board_y + R01S_ISLAND_PAD_TOP;
    max_x = island->board_x + island->board_w - R01S_ISLAND_PAD_X - R01S_CHIP_PIN_OUT - e->body_w;
    max_y = island->board_y + island->board_h - R01S_ISLAND_PAD_BOTTOM - e->body_h;
    if (max_x < min_x) {
        min_x = max_x = island->board_x + (island->board_w - e->body_w) / 2;
    }
    if (max_y < min_y) {
        min_y = max_y = island->board_y + (island->board_h - e->body_h) / 2;
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
    r01s_entity_place(e, bx, by);
}

static void move_chip_drag(R01sUi *ui, int chip_i, int board_mx, int board_my) {
    R01sEntity *e = ui->chips[chip_i];
    r01s_entity_place(e, board_mx - ui->drag_grab_bx, board_my - ui->drag_grab_by);
    clamp_chip_in_island(ui, e, ui->chip_island[chip_i]);
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
    nx = board_mx - ui->drag_grab_bx;
    ny = board_my - ui->drag_grab_by;
    if (nx < 0) {
        nx = 0;
    }
    if (ny < 0) {
        ny = 0;
    }
    if (nx + island->board_w > R01S_BOARD_W) {
        nx = R01S_BOARD_W - island->board_w;
    }
    if (ny + island->board_h > R01S_BOARD_H) {
        ny = R01S_BOARD_H - island->board_h;
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

static void resize_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my) {
    R01sIsland *island = r01s_island_group_at_mut(ui->group, island_index);
    int min_w, min_h, nw, nh, i;

    if (!island) {
        return;
    }
    island_content_min_size(ui, island_index, &min_w, &min_h);
    nw = board_mx - island->board_x;
    nh = board_my - island->board_y;
    if (nw < min_w) {
        nw = min_w;
    }
    if (nh < min_h) {
        nh = min_h;
    }
    if (island->board_x + nw > R01S_BOARD_W) {
        nw = R01S_BOARD_W - island->board_x;
    }
    if (island->board_y + nh > R01S_BOARD_H) {
        nh = R01S_BOARD_H - island->board_y;
    }
    if (nw < min_w) {
        nw = min_w;
    }
    if (nh < min_h) {
        nh = min_h;
    }
    island->board_w = nw;
    island->board_h = nh;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] == (uint8_t)island_index) {
            clamp_chip_in_island(ui, ui->chips[i], island_index);
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

static void draw_system_health_panel(SDL_Renderer *r, const R01sSystemHealth *health) {
    int i;
    int px = 8;
    int py = R01S_UI_HUD_TOP + 6;
    int pw = R01S_UI_SIDEBAR_L - 16;
    int ph = 188;
    Uint8 sr, sg, sb;
    char row[48];

    if (!health) {
        return;
    }

    fill_rect(r, px, py, pw, ph, 14, 20, 16);
    draw_rect(r, px, py, pw, ph, 70, 90, 75);
    font_draw(r, px + 8, py + 6, "SYSTEM STATUS", 190, 205, 180);

    health_rgb(health->system, &sr, &sg, &sb);
    fill_rect(r, px + 8, py + 22, pw - 16, 18, sr, sg, sb);
    font_draw(r, px + 12, py + 26, health->system_label[0] ? health->system_label : "?", 20, 24, 22);

    font_draw(r, px + 8, py + 44, health->system_detail, 150, 165, 145);

    fill_rect(r, px + 8, py + 58, pw - 16, 1, 40, 55, 45);

    for (i = 0; i < health->island_count; i++) {
        const R01sIslandHealth *ih = &health->islands[i];
        int ry = py + 66 + i * 16;
        char letter[2];

        letter[0] = ih->letter ? ih->letter : '?';
        letter[1] = '\0';
        draw_health_dot(r, px + 10, ry + 1, ih->health);
        font_draw(r, px + 22, ry + 2, letter, 180, 200, 170);
        snprintf(row, sizeof(row), "%s  %s", r01s_health_tag(ih->health), ih->activity);
        font_draw(r, px + 34, ry + 2, row, 140, 155, 135);
    }
}

static void draw_island_frame(SDL_Renderer *r, const R01sUi *ui, const R01sIsland *island, int active,
                              const R01sIslandHealth *ih) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int hx = x + island->board_w - R01S_ISLAND_RESIZE_HANDLE;
    int hy = y + island->board_h - R01S_ISLAND_RESIZE_HANDLE;
    Uint8 br, bg, bb;
    char badge[12];

    if (ih) {
        health_rgb(ih->health, &br, &bg, &bb);
    } else {
        br = active ? 140 : 60;
        bg = active ? 200 : 100;
        bb = active ? 120 : 70;
    }

    fill_rect(r, x + 2, y + 2, island->board_w - 4, 28, 22, 48, 32);
    font_draw(r, x + 8, y + 6, island->title ? island->title : "ISLAND", 180, 220, 160);
    if (ih && ih->activity[0]) {
        font_draw(r, x + 8, y + 16, ih->activity, 130, 150, 125);
        snprintf(badge, sizeof(badge), "%s", r01s_health_tag(ih->health));
        font_draw(r, x + island->board_w - 44, y + 6, badge, br, bg, bb);
        draw_health_dot(r, x + island->board_w - 18, y + 5, ih->health);
    }
    draw_rect(r, x, y, island->board_w, island->board_h, br, bg, bb);
    /* Bottom-right resize grip */
    fill_rect(r, hx, hy, R01S_ISLAND_RESIZE_HANDLE, R01S_ISLAND_RESIZE_HANDLE, 40, 70, 50);
    draw_rect(r, hx, hy, R01S_ISLAND_RESIZE_HANDLE, R01S_ISLAND_RESIZE_HANDLE, 120, 160, 130);
    SDL_SetRenderDrawColor(r, 160, 200, 170, 255);
    SDL_RenderDrawLine(r, hx + 3, hy + R01S_ISLAND_RESIZE_HANDLE - 3, hx + R01S_ISLAND_RESIZE_HANDLE - 3,
                       hy + 3);
    SDL_RenderDrawLine(r, hx + 6, hy + R01S_ISLAND_RESIZE_HANDLE - 3, hx + R01S_ISLAND_RESIZE_HANDLE - 3,
                       hy + 6);
}

#define GP_PANEL_W 156
#define GP_PANEL_H 132
#define R01S_UI_GP1_Y (R01S_UI_HUD_TOP + 6 + 188 + 8)
#define R01S_UI_GP2_Y (R01S_UI_GP1_Y + GP_PANEL_H + 8)

static void gp_panel_origin(int player, int *px, int *py) {
    *px = 8;
    *py = player == 0 ? R01S_UI_GP1_Y : R01S_UI_GP2_Y;
}

static void gp_stick_center(int player, int *cx, int *cy) {
    int px, py;
    gp_panel_origin(player, &px, &py);
    *cx = px + 36;
    *cy = py + 56;
}

static void gp_btn_rect(int player, int btn, SDL_Rect *rc) {
    int px, py;
    gp_panel_origin(player, &px, &py);
    rc->x = px + 88 + (btn % 2) * 28;
    rc->y = py + 28 + (btn / 2) * 28;
    rc->w = 24;
    rc->h = 24;
}

static int gp_hit_stick(int player, int lx, int ly) {
    int cx, cy;
    int dx, dy;
    gp_stick_center(player, &cx, &cy);
    dx = lx - cx;
    dy = ly - cy;
    return dx * dx + dy * dy <= (R01S_GAMEPAD_STICK_RADIUS + 8) * (R01S_GAMEPAD_STICK_RADIUS + 8);
}

static int gp_hit_btn(int player, int lx, int ly) {
    int b;
    for (b = 0; b < 4; b++) {
        SDL_Rect rc;
        gp_btn_rect(player, b, &rc);
        if (lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h) {
            return b;
        }
    }
    return -1;
}

static int gp_hit_any(int lx, int ly, int *player_out, int *btn_out) {
    int p;
    for (p = 0; p < R01S_UI_GAMEPAD_COUNT; p++) {
        int b = gp_hit_btn(p, lx, ly);
        if (b >= 0) {
            *player_out = p;
            *btn_out = b;
            return 2;
        }
        if (gp_hit_stick(p, lx, ly)) {
            *player_out = p;
            *btn_out = -1;
            return 1;
        }
    }
    return 0;
}

static void gp_stick_from_point(R01sGamepadInput *gp, int player, int lx, int ly) {
    int cx, cy;
    gp_stick_center(player, &cx, &cy);
    gp->stick_x = lx - cx;
    gp->stick_y = ly - cy;
    r01s_gamepad_stick_clamp(&gp->stick_x, &gp->stick_y, R01S_GAMEPAD_STICK_RADIUS);
}

static void draw_stick(SDL_Renderer *r, int cx, int cy, int sx, int sy) {
    fill_rect(r, cx - R01S_GAMEPAD_STICK_RADIUS, cy - R01S_GAMEPAD_STICK_RADIUS,
              R01S_GAMEPAD_STICK_RADIUS * 2, R01S_GAMEPAD_STICK_RADIUS * 2, 24, 28, 32);
    draw_rect(r, cx - R01S_GAMEPAD_STICK_RADIUS, cy - R01S_GAMEPAD_STICK_RADIUS,
              R01S_GAMEPAD_STICK_RADIUS * 2, R01S_GAMEPAD_STICK_RADIUS * 2, 70, 80, 90);
    fill_rect(r, cx + sx - 6, cy + sy - 6, 12, 12, 180, 190, 200);
    draw_rect(r, cx + sx - 6, cy + sy - 6, 12, 12, 240, 240, 240);
}

static void draw_btn(SDL_Renderer *r, const SDL_Rect *rc, int pressed, const char *label) {
    fill_rect(r, rc->x, rc->y, rc->w, rc->h, pressed ? 90 : 40, pressed ? 120 : 48, pressed ? 160 : 56);
    draw_rect(r, rc->x, rc->y, rc->w, rc->h, 120, 130, 140);
    font_draw(r, rc->x + 4, rc->y + 8, label, 210, 210, 200);
}

static void draw_gamepad_panel(SDL_Renderer *r, const R01sUi *ui, int player) {
    int px, py, cx, cy, b;
    char hex[8];
    uint8_t bits;
    SDL_Rect brc;

    gp_panel_origin(player, &px, &py);
    fill_rect(r, px, py, GP_PANEL_W, GP_PANEL_H, 16, 20, 24);
    draw_rect(r, px, py, GP_PANEL_W, GP_PANEL_H, 60, 70, 80);
    font_draw(r, px + 8, py + 6, player == 0 ? "P1 FE60" : "P2 FE61", 180, 200, 220);

    gp_stick_center(player, &cx, &cy);
    draw_stick(r, cx, cy, ui->gamepad[player].stick_x, ui->gamepad[player].stick_y);

    for (b = 0; b < 4; b++) {
        const char *labels[4] = {"X", "Y", "C", "S"};
        int pressed = 0;
        gp_btn_rect(player, b, &brc);
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
        if (keys[SDL_SCANCODE_UP]) {
            ui->gamepad[0].stick_y = -20;
        }
        if (keys[SDL_SCANCODE_DOWN]) {
            ui->gamepad[0].stick_y = 20;
        }
        if (keys[SDL_SCANCODE_LEFT]) {
            ui->gamepad[0].stick_x = -20;
        }
        if (keys[SDL_SCANCODE_RIGHT]) {
            ui->gamepad[0].stick_x = 20;
        }
    }
    if (ui->drag_stick != 1) {
        ui->gamepad[1].stick_x = 0;
        ui->gamepad[1].stick_y = 0;
        if (keys[SDL_SCANCODE_W]) {
            ui->gamepad[1].stick_y = -20;
        }
        if (keys[SDL_SCANCODE_S]) {
            ui->gamepad[1].stick_y = 20;
        }
        if (keys[SDL_SCANCODE_A]) {
            ui->gamepad[1].stick_x = -20;
        }
        if (keys[SDL_SCANCODE_D]) {
            ui->gamepad[1].stick_x = 20;
        }
    }

    ui->gamepad[0].btn_x = ui->mouse_btn[0][0] || keys[SDL_SCANCODE_Z];
    ui->gamepad[0].btn_y = ui->mouse_btn[0][1] || keys[SDL_SCANCODE_X];
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
    int i, gx, gy;
    int ox = (-ui->pan_x) % 32;
    int oy = (-ui->pan_y) % 32;
    SDL_Rect view_clip = {R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H};
    int probe_x = R01S_LOGIC_W - R01S_UI_SIDEBAR_R + 8;

    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 12, 14, 16);
    fill_rect(r, 0, 0, R01S_UI_SIDEBAR_L, R01S_LOGIC_H, 14, 18, 16);
    fill_rect(r, R01S_LOGIC_W - R01S_UI_SIDEBAR_R, 0, R01S_UI_SIDEBAR_R, R01S_LOGIC_H, 16, 20, 18);
    fill_rect(r, R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H, 18, 42, 28);

    SDL_RenderSetClipRect(r, &view_clip);
    SDL_SetRenderDrawColor(r, 24, 52, 34, 255);
    for (gx = R01S_UI_VIEW_X + ox; gx < R01S_UI_VIEW_X + R01S_UI_VIEW_W; gx += 32) {
        SDL_RenderDrawLine(r, gx, R01S_UI_VIEW_Y, gx, R01S_UI_VIEW_Y + R01S_UI_VIEW_H);
    }
    for (gy = R01S_UI_VIEW_Y + oy; gy < R01S_UI_VIEW_Y + R01S_UI_VIEW_H; gy += 32) {
        SDL_RenderDrawLine(r, R01S_UI_VIEW_X, gy, R01S_UI_VIEW_X + R01S_UI_VIEW_W, gy);
    }

    /* Board island frames (from active group) */
    if (ui->group) {
        for (i = 0; i < r01s_island_group_count(ui->group); i++) {
            const R01sIsland *island = r01s_island_group_at(ui->group, i);
            const R01sIslandHealth *ih = NULL;
            int active = (i == ui->drag_island || i == ui->resize_island);
            if (i < ui->health.island_count) {
                ih = &ui->health.islands[i];
            }
            if (island) {
                draw_island_frame(r, ui, island, active, ih);
            }
        }
    }

    for (i = 0; i < ui->chip_count; i++) {
        draw_chip(r, ui, ui->chips[i], i == ui->selected);
    }
    SDL_RenderSetClipRect(r, NULL);

    draw_rect(r, R01S_UI_VIEW_X, R01S_UI_VIEW_Y, R01S_UI_VIEW_W, R01S_UI_VIEW_H, 48, 64, 52);

    /* Fixed HUD */
    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_UI_HUD_TOP, 12, 14, 16);
    font_draw(r, 8, 7, "RETR01 SIM  ISLANDS A-E+G+H", 200, 210, 220);
    font_draw(r, R01S_UI_VIEW_X + 8, 7, "DRAG EMPTY / RESIZE BR  SHIFT+ARROWS PAN", 120, 130, 140);

    draw_system_health_panel(r, &ui->health);

    fill_rect(r, probe_x - 8, R01S_UI_HUD_TOP + 6, R01S_UI_SIDEBAR_R - 8, 168, 16, 22, 18);
    draw_rect(r, probe_x - 8, R01S_UI_HUD_TOP + 6, R01S_UI_SIDEBAR_R - 8, 168, 80, 90, 70);
    font_draw(r, probe_x, R01S_UI_HUD_TOP + 12, "LIVE PROBE", 200, 210, 180);
    draw_led(r, probe_x, R01S_UI_HUD_TOP + 30, ui->probe_vdd, 80, 220, 100, "VDD");
    draw_led(r, probe_x, R01S_UI_HUD_TOP + 48, ui->probe_phi2, 220, 200, 60, "PHI2");
    draw_led(r, probe_x, R01S_UI_HUD_TOP + 66, ui->probe_resb_low, 220, 80, 80, "RESB LO");
    font_draw(r, probe_x, R01S_UI_HUD_TOP + 88, "P1 FE60", 160, 180, 160);
    draw_pad_bits(r, probe_x, R01S_UI_HUD_TOP + 98, ui->probe_pad_p1);
    font_draw(r, probe_x, R01S_UI_HUD_TOP + 118, "P2 FE61", 160, 180, 160);
    draw_pad_bits(r, probe_x, R01S_UI_HUD_TOP + 128, ui->probe_pad_p2);
    font_draw(r, probe_x, R01S_UI_HUD_TOP + 148, "PINS GLOW = LEVEL", 120, 130, 120);

    draw_gamepad_panel(r, ui, 0);
    draw_gamepad_panel(r, ui, 1);

    fill_rect(r, 0, R01S_LOGIC_H - R01S_UI_HUD_BOTTOM, R01S_LOGIC_W, R01S_UI_HUD_BOTTOM, 12, 14, 16);
    font_draw(r, 8, R01S_LOGIC_H - 15, ui->status, 160, 170, 160);
}

static int hit_chip(const R01sUi *ui, const R01sEntity *e, int lx, int ly) {
    int x = ui_board_sx(ui, e->board_x);
    int y = ui_board_sy(ui, e->board_y);
    return lx >= x - 12 && lx < x + e->body_w + 12 && ly >= y - 4 && ly < y + e->body_h + 4;
}

static int hit_island_frame(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    return lx >= x && lx < x + island->board_w && ly >= y && ly < y + island->board_h;
}

static int hit_island_resize(const R01sUi *ui, const R01sIsland *island, int lx, int ly) {
    int x = ui_board_sx(ui, island->board_x);
    int y = ui_board_sy(ui, island->board_y);
    int hx = x + island->board_w - R01S_ISLAND_RESIZE_HANDLE;
    int hy = y + island->board_h - R01S_ISLAND_RESIZE_HANDLE;
    return lx >= hx && lx < x + island->board_w && ly >= hy && ly < y + island->board_h;
}

static int island_has_chip_at(const R01sUi *ui, int island_index, int lx, int ly) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_island[i] != (uint8_t)island_index) {
            continue;
        }
        if (hit_chip(ui, ui->chips[i], lx, ly)) {
            return 1;
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
    if (ui_logic_in_view(logic_x, logic_y)) {
        ui_logic_to_board(ui, logic_x, logic_y, &board_mx, &board_my);
    }
    if (e->type == SDL_MOUSEWHEEL && ui_logic_in_view(logic_x, logic_y)) {
        ui->pan_x -= e->wheel.x * 32;
        ui->pan_y -= e->wheel.y * 32;
        r01s_ui_clamp_pan(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        (e->button.button == SDL_BUTTON_MIDDLE || e->button.button == SDL_BUTTON_RIGHT) &&
        ui_logic_in_view(logic_x, logic_y)) {
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
        gp_stick_from_point(&ui->gamepad[ui->drag_stick], ui->drag_stick, logic_x, logic_y);
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
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        const Uint8 *mods = SDL_GetKeyboardState(NULL);
        int step = 48;
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
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;
        return ui->selected >= 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int gp_player = 0;
        int gp_btn = -1;
        int hit = gp_hit_any(logic_x, logic_y, &gp_player, &gp_btn);
        if (hit == 2) {
            ui->drag_btn = gp_player * 4 + gp_btn;
            ui->mouse_btn[gp_player][gp_btn] = 1;
            return 1;
        }
        if (hit == 1) {
            ui->drag_stick = gp_player;
            gp_stick_from_point(&ui->gamepad[gp_player], gp_player, logic_x, logic_y);
            return 1;
        }
        ui->selected = -1;
        ui->drag_chip = -1;
        ui->drag_island = -1;
        ui->resize_island = -1;

        /* Resize handle wins over chips (bottom-right grip). */
        if (ui->group) {
            for (i = r01s_island_group_count(ui->group) - 1; i >= 0; i--) {
                const R01sIsland *island = r01s_island_group_at(ui->group, i);
                if (island && hit_island_resize(ui, island, logic_x, logic_y)) {
                    ui->resize_island = i;
                    snprintf(ui->status, sizeof(ui->status), "resize %s",
                             island->title ? island->title : "ISLAND");
                    return 1;
                }
            }
        }

        if (!ui_logic_in_view(logic_x, logic_y)) {
            return 1;
        }

        for (i = ui->chip_count - 1; i >= 0; i--) {
            if (hit_chip(ui, ui->chips[i], logic_x, logic_y)) {
                ui->selected = i;
                ui->drag_chip = i;
                ui->drag_grab_bx = board_mx - ui->chips[i]->board_x;
                ui->drag_grab_by = board_my - ui->chips[i]->board_y;
                snprintf(ui->status, sizeof(ui->status), "drag %s (%s)  pins=%d",
                         ui->chips[i]->refdes ? ui->chips[i]->refdes : "?",
                         ui->chips[i]->part ? ui->chips[i]->part : "?", ui->chips[i]->pin_count);
                return 1;
            }
        }

        /* Empty island area (including title bar) moves the frame + chips. */
        if (ui->group) {
            for (i = r01s_island_group_count(ui->group) - 1; i >= 0; i--) {
                const R01sIsland *island = r01s_island_group_at(ui->group, i);
                if (!island || !hit_island_frame(ui, island, logic_x, logic_y)) {
                    continue;
                }
                if (island_has_chip_at(ui, i, logic_x, logic_y)) {
                    continue;
                }
                ui->drag_island = i;
                ui->drag_grab_bx = board_mx - island->board_x;
                ui->drag_grab_by = board_my - island->board_y;
                snprintf(ui->status, sizeof(ui->status), "move %s",
                         island->title ? island->title : "ISLAND");
                return 1;
            }
        }
        return 1;
    }
    return 0;
}
