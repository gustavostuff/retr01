#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void screen_sel_set(UiState *ui, int x0, int y0, int x1, int y1) {
    if (!ui) {
        return;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= R01_SCREEN_TILES_X) {
        x1 = R01_SCREEN_TILES_X - 1;
    }
    if (y1 >= R01_SCREEN_TILES_Y) {
        y1 = R01_SCREEN_TILES_Y - 1;
    }
    ui->sel_x0 = x0;
    ui->sel_y0 = y0;
    ui->sel_x1 = x1;
    ui->sel_y1 = y1;
}

void screen_sel_clear(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->sel_x0 = -1;
    ui->sel_y0 = -1;
    ui->sel_x1 = -1;
    ui->sel_y1 = -1;
    ui->sel_drag = 0;
}

int screen_sel_valid(const UiState *ui) {
    return ui && ui->sel_x0 >= 0 && ui->sel_y0 >= 0;
}

void screen_sel_bounds(const UiState *ui, int *min_x, int *min_y, int *max_x, int *max_y) {
    if (!screen_sel_valid(ui)) {
        *min_x = *min_y = *max_x = *max_y = -1;
        return;
    }
    *min_x = ui->sel_x0 < ui->sel_x1 ? ui->sel_x0 : ui->sel_x1;
    *max_x = ui->sel_x0 > ui->sel_x1 ? ui->sel_x0 : ui->sel_x1;
    *min_y = ui->sel_y0 < ui->sel_y1 ? ui->sel_y0 : ui->sel_y1;
    *max_y = ui->sel_y0 > ui->sel_y1 ? ui->sel_y0 : ui->sel_y1;
}

int screen_sel_is_multi(const UiState *ui) {
    int min_x, min_y, max_x, max_y;
    if (!screen_sel_valid(ui)) {
        return 0;
    }
    screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
    return (max_x - min_x + 1) * (max_y - min_y + 1) > 1;
}
