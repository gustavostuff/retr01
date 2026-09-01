#include "ui_internal.h"

#include "app.h"
#include "retr01_sim/board.h"

#include <string.h>

#ifndef R01S_CART_PATH_A
#define R01S_CART_PATH_A "output/test.retr01"
#endif
#ifndef R01S_CART_PATH_B
#define R01S_CART_PATH_B "output/test_2.retr01"
#endif

enum {
    R01S_UI_CONTROL_POWER = 0,
    R01S_UI_CONTROL_RESET,
    R01S_UI_CONTROL_CART,
    R01S_UI_CONTROL_CART_A,
    R01S_UI_CONTROL_CART_B,
    R01S_UI_CONTROL_FLASH,
    R01S_UI_CONTROL_COUNT,
};

#define R01S_UI_CONTROL_PAD 6
#define R01S_UI_CONTROL_BTN_H 18
#define R01S_UI_CONTROL_GAP 2

static int control_strip_label_w(void) {
    static const char *labels[R01S_UI_CONTROL_COUNT] = {
        "Power", "Reset", "Insert Cart", "test", "test 2", "Flash Cart"};
    int i;
    int max_w = 0;
    for (i = 0; i < R01S_UI_CONTROL_COUNT; i++) {
        int w = font_text_width(labels[i]);
        if (w > max_w) {
            max_w = w;
        }
    }
    return max_w;
}

static int control_strip_w(void) {
    return R01S_UI_CONTROL_PAD * 2 + control_strip_label_w();
}

static int control_strip_h(void) {
    return R01S_UI_CONTROL_PAD * 2 + R01S_UI_CONTROL_COUNT * R01S_UI_CONTROL_BTN_H +
           (R01S_UI_CONTROL_COUNT - 1) * R01S_UI_CONTROL_GAP;
}

static void control_strip_screen_origin(const R01sUi *ui, int *sx, int *sy) {
    if (!ui) {
        if (sx) {
            *sx = R01S_UI_VIEW_X + R01S_UI_CONTROL_STRIP_DEFAULT_X;
        }
        if (sy) {
            *sy = R01S_UI_VIEW_Y + R01S_UI_CONTROL_STRIP_DEFAULT_Y;
        }
        return;
    }
    if (sx) {
        *sx = R01S_UI_VIEW_X + ui->control_strip_x;
    }
    if (sy) {
        *sy = R01S_UI_VIEW_Y + ui->control_strip_y;
    }
}

static void control_strip_bounds(const R01sUi *ui, SDL_Rect *rc) {
    int sx;
    int sy;

    if (!rc) {
        return;
    }
    control_strip_screen_origin(ui, &sx, &sy);
    rc->x = sx;
    rc->y = sy;
    rc->w = control_strip_w();
    rc->h = control_strip_h();
}

static void control_strip_clamp(R01sUi *ui) {
    int max_x;
    int max_y;

    if (!ui) {
        return;
    }
    max_x = R01S_UI_VIEW_W - control_strip_w();
    max_y = R01S_UI_VIEW_H - control_strip_h();
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (ui->control_strip_x < 0) {
        ui->control_strip_x = 0;
    }
    if (ui->control_strip_y < 0) {
        ui->control_strip_y = 0;
    }
    if (ui->control_strip_x > max_x) {
        ui->control_strip_x = max_x;
    }
    if (ui->control_strip_y > max_y) {
        ui->control_strip_y = max_y;
    }
}

static void control_btn_rect(const R01sUi *ui, int index, SDL_Rect *rc) {
    int sx;
    int sy;
    int y;

    control_strip_screen_origin(ui, &sx, &sy);
    y = sy + R01S_UI_CONTROL_PAD + index * (R01S_UI_CONTROL_BTN_H + R01S_UI_CONTROL_GAP);
    rc->x = sx + R01S_UI_CONTROL_PAD;
    rc->y = y;
    rc->w = control_strip_w() - R01S_UI_CONTROL_PAD * 2;
    rc->h = R01S_UI_CONTROL_BTN_H;
}

static int control_strip_hit(const R01sUi *ui, int lx, int ly) {
    int i;
    if (!ui || !ui_control_strip_contains(ui, lx, ly)) {
        return -1;
    }
    for (i = 0; i < R01S_UI_CONTROL_COUNT; i++) {
        SDL_Rect btn;
        control_btn_rect(ui, i, &btn);
        if (lx >= btn.x && lx < btn.x + btn.w && ly >= btn.y && ly < btn.y + btn.h) {
            return i;
        }
    }
    return -1;
}

static const char *control_cart_label(const R01sBoard *board) {
    return board && r01s_board_cart_inserted(board) ? "Remove Cart" : "Insert Cart";
}

static int control_cart_picker_disabled(const R01sBoard *board) {
    return board && (r01s_board_cart_inserted(board) || r01s_board_flash_active(board));
}

static int control_cart_path_selected(const R01sBoard *board, const char *path) {
    const char *active;

    if (!board || !path) {
        return 0;
    }
    active = r01s_board_cart_path(board);
    return active && strcmp(active, path) == 0;
}

int ui_control_strip_contains(const R01sUi *ui, int lx, int ly) {
    SDL_Rect rc;
    if (!ui) {
        return 0;
    }
    control_strip_bounds(ui, &rc);
    return lx >= rc.x && lx < rc.x + rc.w && ly >= rc.y && ly < rc.y + rc.h;
}

void ui_control_strip_clamp(R01sUi *ui) {
    control_strip_clamp(ui);
}

void ui_control_strip_draw(SDL_Renderer *r, R01sUi *ui) {
    R01sBoard *board;
    int sx;
    int sy;
    int sw;
    int sh;
    int i;
    int powered;
    int flashing;

    if (!r || !ui) {
        return;
    }
    board = r01s_board_from_group(ui->group);
    powered = ui->group && ui->group->powered;
    flashing = board && r01s_board_flash_active(board);
    control_strip_clamp(ui);
    control_strip_screen_origin(ui, &sx, &sy);
    sw = control_strip_w();
    sh = control_strip_h();
    fill_rect_a(r, sx, sy, sw, sh, 20, 24, 18, 200);
    draw_rect(r, sx, sy, sw, sh, 70, 90, 75);

    for (i = 0; i < R01S_UI_CONTROL_COUNT; i++) {
        SDL_Rect btn;
        const char *label;
        int selected = 0;
        int disabled = 0;

        control_btn_rect(ui, i, &btn);
        switch (i) {
        case R01S_UI_CONTROL_POWER:
            label = "Power";
            selected = powered;
            break;
        case R01S_UI_CONTROL_RESET:
            label = "Reset";
            disabled = !powered;
            break;
        case R01S_UI_CONTROL_CART:
            label = control_cart_label(board);
            break;
        case R01S_UI_CONTROL_CART_A:
            label = "test";
            disabled = control_cart_picker_disabled(board);
            selected = control_cart_path_selected(board, R01S_CART_PATH_A);
            break;
        case R01S_UI_CONTROL_CART_B:
            label = "test 2";
            disabled = control_cart_picker_disabled(board);
            selected = control_cart_path_selected(board, R01S_CART_PATH_B);
            break;
        case R01S_UI_CONTROL_FLASH:
            label = "Flash Cart";
            disabled = flashing;
            break;
        default:
            label = "?";
            break;
        }
        if (disabled) {
            fill_rect(r, btn.x, btn.y, btn.w, btn.h, 28, 30, 32);
            draw_rect(r, btn.x, btn.y, btn.w, btn.h, 60, 65, 70);
            font_draw(r, btn.x + (btn.w - font_text_width(label)) / 2,
                      btn.y + (btn.h - font_line_h()) / 2, label, 100, 105, 110);
        } else {
            draw_segment_btn(r, &btn, selected, label);
        }
    }
}

int ui_control_strip_activate(R01sUi *ui, int lx, int ly) {
    R01sBoard *board;
    int hit;

    if (!ui || !ui->group) {
        return 0;
    }
    board = r01s_board_from_group(ui->group);
    if (!board) {
        return 0;
    }
    hit = control_strip_hit(ui, lx, ly);
    if (hit < 0) {
        return 0;
    }
    switch (hit) {
    case R01S_UI_CONTROL_POWER:
        if (ui->app) {
            r01s_app_toggle_power(ui->app);
        } else {
            r01s_board_toggle_power(board, ui->group);
            snprintf(ui->status, sizeof(ui->status), "power %s", ui->group->powered ? "on" : "off");
        }
        return 1;
    case R01S_UI_CONTROL_RESET:
        if (ui->app) {
            r01s_app_console_reset(ui->app);
        } else if (ui->group->powered) {
            r01s_board_console_reset(board, ui->group);
            snprintf(ui->status, sizeof(ui->status), "console reset");
        }
        return 1;
    case R01S_UI_CONTROL_CART:
        r01s_board_toggle_cart(board);
        snprintf(ui->status, sizeof(ui->status), "%s", control_cart_label(board));
        return 1;
    case R01S_UI_CONTROL_CART_A:
        if (control_cart_picker_disabled(board)) {
            return 1;
        }
        if (r01s_board_select_cart(board, R01S_CART_PATH_A) == 0) {
            snprintf(ui->status, sizeof(ui->status), "cart: test");
        } else {
            snprintf(ui->status, sizeof(ui->status), "cart switch failed");
        }
        return 1;
    case R01S_UI_CONTROL_CART_B:
        if (control_cart_picker_disabled(board)) {
            return 1;
        }
        if (r01s_board_select_cart(board, R01S_CART_PATH_B) == 0) {
            snprintf(ui->status, sizeof(ui->status), "cart: test 2");
        } else {
            snprintf(ui->status, sizeof(ui->status), "cart switch failed");
        }
        return 1;
    case R01S_UI_CONTROL_FLASH:
        if (r01s_board_flash_active(board)) {
            return 1;
        }
        if (r01s_board_start_flash(board, NULL) == 0) {
            snprintf(ui->status, sizeof(ui->status), "flashing cart");
        } else {
            snprintf(ui->status, sizeof(ui->status), "flash failed");
        }
        return 1;
    default:
        return 0;
    }
}
