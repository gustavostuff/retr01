#include "ui.h"

#include <stdio.h>
#include <string.h>

/* Minimal 5x7 digits/letters for labels (subset). */
static const uint8_t FONT[36][7] = {
    /* 0-9 */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    /* A-Z */
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
        if (*text == ' ') {
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
    snprintf(ui->status, sizeof(ui->status), "retr01_sim scaffold — stub DIP on board");
    return 0;
}

void r01s_ui_shutdown(R01sUi *ui) {
    if (ui) {
        memset(ui, 0, sizeof(*ui));
    }
}

int r01s_ui_add_chip(R01sUi *ui, R01sEntity *chip) {
    if (!ui || !chip || ui->chip_count >= R01S_BOARD_MAX_CHIPS) {
        return -1;
    }
    ui->chips[ui->chip_count++] = chip;
    return 0;
}

static void draw_chip(SDL_Renderer *r, const R01sEntity *e, int pan_x, int pan_y, int selected) {
    int x = e->board_x - pan_x;
    int y = e->board_y - pan_y;
    int rows = (e->pin_count + 1) / 2;
    int i;
    int pitch = rows > 1 ? (e->body_h - 8) / (rows - 1) : 0;

    /* Pin stubs — left 1..N/2, right N..N/2+1 (DIP). */
    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int side_left = num <= rows;
        int idx = side_left ? (num - 1) : (e->pin_count - num);
        int py = y + 4 + idx * pitch;
        int px0, px1;
        Uint8 pr = 180, pg = 160, pb = 60;
        if (e->pins[i].dir == R01S_PIN_PWR) {
            pr = 200;
            pg = 80;
            pb = 80;
        }
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
    /* Pin-1 notch */
    fill_rect(r, x + e->body_w / 2 - 4, y - 2, 8, 4, 20, 22, 20);

    if (e->refdes) {
        font_draw(r, x + 4, y + 8, e->refdes, 220, 220, 200);
    }
    if (e->part) {
        font_draw(r, x + 4, y + 20, e->part, 160, 180, 140);
    }
}

void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r) {
    int i;
    /* PCB-ish field */
    fill_rect(r, 0, 0, R01S_LOGIC_W, R01S_LOGIC_H, 18, 42, 28);
    /* faint grid */
    SDL_SetRenderDrawColor(r, 24, 52, 34, 255);
    for (i = 0; i < R01S_LOGIC_W; i += 32) {
        SDL_RenderDrawLine(r, i, 0, i, R01S_LOGIC_H);
    }
    for (i = 0; i < R01S_LOGIC_H; i += 32) {
        SDL_RenderDrawLine(r, 0, i, R01S_LOGIC_W, i);
    }

    fill_rect(r, 0, 0, R01S_LOGIC_W, 18, 12, 14, 16);
    font_draw(r, 8, 5, "RETR01 SIM  BOARD VIEW", 200, 210, 220);
    font_draw(r, R01S_LOGIC_W - 200, 5, "ESC QUIT  CLICK SELECT", 120, 130, 140);

    for (i = 0; i < ui->chip_count; i++) {
        draw_chip(r, ui->chips[i], ui->pan_x, ui->pan_y, i == ui->selected);
    }

    fill_rect(r, 0, R01S_LOGIC_H - 16, R01S_LOGIC_W, 16, 12, 14, 16);
    font_draw(r, 8, R01S_LOGIC_H - 12, ui->status, 160, 170, 160);
}

static int hit_chip(const R01sEntity *e, int lx, int ly, int pan_x, int pan_y) {
    int x = e->board_x - pan_x;
    int y = e->board_y - pan_y;
    return lx >= x - 12 && lx < x + e->body_w + 12 && ly >= y - 4 && ly < y + e->body_h + 4;
}

int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y) {
    int i;
    if (!ui || !e) {
        return 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        ui->selected = -1;
        for (i = ui->chip_count - 1; i >= 0; i--) {
            if (hit_chip(ui->chips[i], logic_x, logic_y, ui->pan_x, ui->pan_y)) {
                ui->selected = i;
                snprintf(ui->status, sizeof(ui->status), "selected %s (%s)  pins=%d",
                         ui->chips[i]->refdes ? ui->chips[i]->refdes : "?",
                         ui->chips[i]->part ? ui->chips[i]->part : "?", ui->chips[i]->pin_count);
                return 1;
            }
        }
        snprintf(ui->status, sizeof(ui->status), "board click %d,%d", logic_x, logic_y);
        return 1;
    }
    return 0;
}
