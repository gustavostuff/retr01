#include "ui.h"

#include "retr01_sim/board_layout.h"

#include <stdio.h>
#include <string.h>

static void clamp_chip_in_island(R01sUi *ui, R01sEntity *e, int island_index);

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
    snprintf(ui->status, sizeof(ui->status), "islands A-E — drag chips, wheel/drag pan");
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
    int max_x = R01S_BOARD_W - R01S_LOGIC_W;
    int max_y = R01S_BOARD_H - R01S_LOGIC_H;
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

static void draw_chip(SDL_Renderer *r, const R01sEntity *e, int pan_x, int pan_y, int selected) {
    int x = e->board_x - pan_x;
    int y = e->board_y - pan_y;
    int i;
    int label_y;

    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int side_left;
        int py = dip_pin_y(e, num, &side_left) - pan_y;
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

static void draw_island_frame(SDL_Renderer *r, const R01sIsland *island, int pan_x, int pan_y) {
    int x = island->board_x - pan_x;
    int y = island->board_y - pan_y;
    fill_rect(r, x + 2, y + 2, island->board_w - 4, 16, 22, 48, 32);
    font_draw(r, x + 8, y + 6, island->title ? island->title : "ISLAND", 180, 220, 160);
    draw_rect(r, x, y, island->board_w, island->board_h, 60, 100, 70);
}

void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r) {
    int i, gx, gy;
    int ox = -ui->pan_x % 32;
    int oy = -ui->pan_y % 32;

    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 18, 42, 28);
    SDL_SetRenderDrawColor(r, 24, 52, 34, 255);
    for (gx = ox; gx < R01S_LOGIC_W; gx += 32) {
        SDL_RenderDrawLine(r, gx, 0, gx, R01S_LOGIC_H);
    }
    for (gy = oy; gy < R01S_LOGIC_H; gy += 32) {
        SDL_RenderDrawLine(r, 0, gy, R01S_LOGIC_W, gy);
    }

    /* Board island frames (from active group) */
    if (ui->group) {
        for (i = 0; i < r01s_island_group_count(ui->group); i++) {
            const R01sIsland *island = r01s_island_group_at(ui->group, i);
            if (island) {
                draw_island_frame(r, island, ui->pan_x, ui->pan_y);
            }
        }
    }

    for (i = 0; i < ui->chip_count; i++) {
        draw_chip(r, ui->chips[i], ui->pan_x, ui->pan_y, i == ui->selected);
    }

    /* Fixed HUD */
    fill_rect(r, 0, 0, R01S_LOGIC_W, 22, 12, 14, 16);
    font_draw(r, 8, 7, "RETR01 SIM  ISLANDS A-E", 200, 210, 220);
    font_draw(r, R01S_LOGIC_W - 500, 7, "SPC PAUSE  R RESET  . STEP  DRAG CHIP  WHEEL PAN  ESC", 120, 130, 140);

    fill_rect(r, R01S_LOGIC_W - 200, 36, 184, 110, 16, 22, 18);
    draw_rect(r, R01S_LOGIC_W - 200, 36, 184, 110, 80, 90, 70);
    font_draw(r, R01S_LOGIC_W - 192, 42, "LIVE PROBE", 200, 210, 180);
    draw_led(r, R01S_LOGIC_W - 192, 60, ui->probe_vdd, 80, 220, 100, "VDD");
    draw_led(r, R01S_LOGIC_W - 192, 78, ui->probe_phi2, 220, 200, 60, "PHI2");
    draw_led(r, R01S_LOGIC_W - 192, 96, ui->probe_resb_low, 220, 80, 80, "RESB LO");
    font_draw(r, R01S_LOGIC_W - 192, 118, "PINS GLOW = LEVEL", 120, 130, 120);

    fill_rect(r, 0, R01S_LOGIC_H - 22, R01S_LOGIC_W, 22, 12, 14, 16);
    font_draw(r, 8, R01S_LOGIC_H - 15, ui->status, 160, 170, 160);
}

static int hit_chip(const R01sEntity *e, int lx, int ly, int pan_x, int pan_y) {
    int x = e->board_x - pan_x;
    int y = e->board_y - pan_y;
    return lx >= x - 12 && lx < x + e->body_w + 12 && ly >= y - 4 && ly < y + e->body_h + 4;
}

int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y) {
    int i;
    int board_mx = logic_x + ui->pan_x;
    int board_my = logic_y + ui->pan_y;
    if (!ui || !e) {
        return 0;
    }
    if (e->type == SDL_MOUSEWHEEL) {
        ui->pan_x -= e->wheel.x * 32;
        ui->pan_y -= e->wheel.y * 32;
        r01s_ui_clamp_pan(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN &&
        (e->button.button == SDL_BUTTON_MIDDLE || e->button.button == SDL_BUTTON_RIGHT)) {
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
        ui->pan_x -= (logic_x - ui->drag_last_x);
        ui->pan_y -= (logic_y - ui->drag_last_y);
        ui->drag_last_x = logic_x;
        ui->drag_last_y = logic_y;
        r01s_ui_clamp_pan(ui);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        move_chip_drag(ui, ui->drag_chip, board_mx, board_my);
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        int step = 48;
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
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ui->drag_chip = -1;
        return ui->selected >= 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        ui->selected = -1;
        ui->drag_chip = -1;
        for (i = ui->chip_count - 1; i >= 0; i--) {
            if (hit_chip(ui->chips[i], logic_x, logic_y, ui->pan_x, ui->pan_y)) {
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
        return 1;
    }
    return 0;
}
