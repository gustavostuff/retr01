#include "retr01_ui/widgets.h"

#include <string.h>

static int panel_track_w(const UiPanel *p, int col) {
    int w;
    if (!p || col < 0 || col >= p->cols) {
        return 0;
    }
    w = p->col_w[col];
    return w > 0 ? w : p->cell_w;
}

static int panel_track_h(const UiPanel *p, int row) {
    int h;
    if (!p || row < 0 || row >= p->rows) {
        return 0;
    }
    h = p->row_h[row];
    return h > 0 ? h : p->cell_h;
}

static int panel_col_origin(const UiPanel *p, int col) {
    int i, x = 0;
    for (i = 0; i < col && i < p->cols; i++) {
        x += panel_track_w(p, i);
    }
    return x;
}

static int panel_row_origin(const UiPanel *p, int row) {
    int i, y = 0;
    for (i = 0; i < row && i < p->rows; i++) {
        y += panel_track_h(p, i);
    }
    return y;
}

static int panel_span_w(const UiPanel *p, int col, int colspan) {
    int i, w = 0;
    int end = col + colspan;
    if (end > p->cols) {
        end = p->cols;
    }
    for (i = col; i < end; i++) {
        w += panel_track_w(p, i);
    }
    return w;
}

static int panel_span_h(const UiPanel *p, int row, int rowspan) {
    int i, h = 0;
    int end = row + rowspan;
    if (end > p->rows) {
        end = p->rows;
    }
    for (i = row; i < end; i++) {
        h += panel_track_h(p, i);
    }
    return h;
}

void ui_panel_init(UiPanel *p, int cols, int rows, int cell_w, int cell_h) {
    if (!p) {
        return;
    }
    memset(p, 0, sizeof(*p));
    if (cols < 1) {
        cols = 1;
    }
    if (rows < 1) {
        rows = 1;
    }
    if (cols > UI_PANEL_TRACKS_MAX) {
        cols = UI_PANEL_TRACKS_MAX;
    }
    if (rows > UI_PANEL_TRACKS_MAX) {
        rows = UI_PANEL_TRACKS_MAX;
    }
    p->cols = cols;
    p->rows = rows;
    p->cell_w = cell_w >= UI_PANEL_CELL_MIN ? cell_w : UI_PANEL_CELL_MIN;
    p->cell_h = cell_h >= UI_PANEL_CELL_MIN ? cell_h : UI_PANEL_CELL_MIN;
}

void ui_panel_set_cells(UiPanel *p, const UiPanelCell *cells, int count) {
    if (!p) {
        return;
    }
    p->cells = cells;
    p->cell_count = count > 0 ? count : 0;
    if (p->cell_count > UI_PANEL_CELLS_MAX) {
        p->cell_count = UI_PANEL_CELLS_MAX;
    }
}

void ui_panel_set_col_w(UiPanel *p, int col, int w) {
    if (!p || col < 0 || col >= p->cols || col >= UI_PANEL_TRACKS_MAX) {
        return;
    }
    p->col_w[col] = w;
}

void ui_panel_set_row_h(UiPanel *p, int row, int h) {
    if (!p || row < 0 || row >= p->rows || row >= UI_PANEL_TRACKS_MAX) {
        return;
    }
    p->row_h[row] = h;
}

void ui_panel_layout(UiPanel *p, int x, int y) {
    int i;
    if (!p) {
        return;
    }
    p->x = x;
    p->y = y;
    p->total_w = panel_span_w(p, 0, p->cols);
    p->total_h = panel_span_h(p, 0, p->rows);

    for (i = 0; i < UI_PANEL_CELLS_MAX; i++) {
        p->out_x[i] = 0;
        p->out_y[i] = 0;
        p->out_w[i] = 0;
        p->out_h[i] = 0;
    }

    if (!p->cells || p->cell_count < 1) {
        return;
    }

    for (i = 0; i < p->cell_count; i++) {
        const UiPanelCell *c = &p->cells[i];
        int cs, rs;
        if (c->col < 0 || c->row < 0 || c->col >= p->cols || c->row >= p->rows) {
            continue;
        }
        cs = c->colspan >= 1 ? c->colspan : 1;
        rs = c->rowspan >= 1 ? c->rowspan : 1;
        if (c->col + cs > p->cols || c->row + rs > p->rows) {
            continue;
        }
        p->out_x[i] = x + panel_col_origin(p, c->col);
        p->out_y[i] = y + panel_row_origin(p, c->row);
        p->out_w[i] = panel_span_w(p, c->col, cs);
        p->out_h[i] = panel_span_h(p, c->row, rs);
    }
}

int ui_panel_cell(const UiPanel *p, int id, int *x, int *y, int *w, int *h) {
    int i;
    if (!p || !p->cells || id < 0) {
        return 0;
    }
    for (i = 0; i < p->cell_count; i++) {
        if (p->cells[i].id != id) {
            continue;
        }
        if (x) {
            *x = p->out_x[i];
        }
        if (y) {
            *y = p->out_y[i];
        }
        if (w) {
            *w = p->out_w[i];
        }
        if (h) {
            *h = p->out_h[i];
        }
        return 1;
    }
    return 0;
}

void ui_panel_debug_draw(SDL_Renderer *r, const UiPanel *p) {
    int x, y, step;
    int pw, ph;
    if (!UI_PANEL_DEBUG_GRID || !r || !p) {
        return;
    }
    pw = p->total_w;
    ph = p->total_h;
    if (pw < 1 || ph < 1) {
        return;
    }
    step = UI_UNIT > 0 ? UI_UNIT : 8;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    /* Full panel chess at ~20% opacity so gaps and widgets stay visible. */
    for (y = 0; y < ph; y += step) {
        for (x = 0; x < pw; x += step) {
            int tw = step;
            int th = step;
            int parity = ((x / step) + (y / step)) & 1;
            if (x + tw > pw) {
                tw = pw - x;
            }
            if (y + th > ph) {
                th = ph - y;
            }
            if (parity) {
                fill_rect_alpha(r, p->x + x, p->y + y, tw, th, 255, 105, 180, 51);
            } else {
                fill_rect_alpha(r, p->x + x, p->y + y, tw, th, 255, 20, 147, 51);
            }
        }
    }
}
