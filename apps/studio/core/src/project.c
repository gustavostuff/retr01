#include "retr01_studio/project.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/warps.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

static void init_screen(R01Screen *s, int col, int row) {
    int c;
    s->col = col;
    s->row = row;
    s->present = 0;
    memset(s->pixels, 0, sizeof(s->pixels));
    memset(s->tiles, 0, sizeof(s->tiles));
    for (c = 0; c < R01_TILES_PER_SCREEN; c++) {
        s->attrs[c] = r01_attr_pack(0, 0, 0, 0);
    }
}

int r01_world_screen_index(const R01World *w, int col, int row) {
    if (!w || col < 0 || row < 0 || col >= w->grid_cols || row >= w->grid_rows) {
        return -1;
    }
    return row * w->grid_cols + col;
}

void r01_world_bg0_recompute_extent(R01World *w) {
    int i, min_c = 99, min_r = 99, max_c = 0, max_r = 0, n = 0;
    if (!w) {
        return;
    }
    for (i = 0; i < w->bg0_screen_count && i < R01_BG0_SCREENS_MAX; i++) {
        if (!w->bg0_screens[i].present) {
            continue;
        }
        n++;
        if (w->bg0_screens[i].col < min_c) {
            min_c = w->bg0_screens[i].col;
        }
        if (w->bg0_screens[i].row < min_r) {
            min_r = w->bg0_screens[i].row;
        }
        if (w->bg0_screens[i].col > max_c) {
            max_c = w->bg0_screens[i].col;
        }
        if (w->bg0_screens[i].row > max_r) {
            max_r = w->bg0_screens[i].row;
        }
    }
    if (n < 1) {
        w->bg0_cols = 0;
        w->bg0_rows = 0;
        return;
    }
    w->bg0_cols = max_c - min_c + 1;
    w->bg0_rows = max_r - min_r + 1;
    if (w->bg0_cols < 1) {
        w->bg0_cols = 1;
    }
    if (w->bg0_rows < 1) {
        w->bg0_rows = 1;
    }
}

void r01_world_bg0_clear(R01World *w) {
    int i;
    if (!w) {
        return;
    }
    for (i = 0; i < R01_BG0_SCREENS_MAX; i++) {
        init_screen(&w->bg0_screens[i], 0, 0);
    }
    w->bg0_screen_count = 0;
    w->bg0_active_screen = -1;
    w->bg0_cols = 0;
    w->bg0_rows = 0;
}

int r01_world_bg0_screen_index(const R01World *w, int col, int row) {
    int i;
    if (!w || col < 0 || row < 0 || col >= R01_GRID_MAX || row >= R01_GRID_MAX) {
        return -1;
    }
    for (i = 0; i < w->bg0_screen_count && i < R01_BG0_SCREENS_MAX; i++) {
        if (w->bg0_screens[i].present && w->bg0_screens[i].col == col && w->bg0_screens[i].row == row) {
            return i;
        }
    }
    return -1;
}

R01Screen *r01_world_bg0_screen_at(R01World *w, int col, int row) {
    int i = r01_world_bg0_screen_index(w, col, row);
    if (i < 0) {
        return NULL;
    }
    return &w->bg0_screens[i];
}

int r01_world_bg0_present_count(const R01World *w) {
    int i;
    int n = 0;
    if (!w) {
        return 0;
    }
    for (i = 0; i < w->bg0_screen_count && i < R01_BG0_SCREENS_MAX; i++) {
        if (w->bg0_screens[i].present) {
            n++;
        }
    }
    return n;
}

int r01_world_bg0_create_screen(R01World *w, int col, int row) {
    int i, free_i, idx;
    if (!w || col < 0 || row < 0 || col >= R01_GRID_MAX || row >= R01_GRID_MAX) {
        return -1;
    }
    idx = r01_world_bg0_screen_index(w, col, row);
    if (idx >= 0) {
        w->bg0_active_screen = idx;
        return idx;
    }
    if (r01_world_bg0_present_count(w) >= R01_BG0_SCREENS_MAX) {
        return -1;
    }
    free_i = -1;
    for (i = 0; i < w->bg0_screen_count && i < R01_BG0_SCREENS_MAX; i++) {
        if (!w->bg0_screens[i].present) {
            free_i = i;
            break;
        }
    }
    if (free_i < 0) {
        if (w->bg0_screen_count >= R01_BG0_SCREENS_MAX) {
            return -1;
        }
        free_i = w->bg0_screen_count++;
    }
    init_screen(&w->bg0_screens[free_i], col, row);
    w->bg0_screens[free_i].present = 1;
    w->bg0_active_screen = free_i;
    r01_world_bg0_recompute_extent(w);
    return free_i;
}

int r01_world_bg0_remove_screen(R01World *w, int col, int row) {
    int idx;
    R01Screen *s;
    if (!w) {
        return -1;
    }
    idx = r01_world_bg0_screen_index(w, col, row);
    if (idx < 0) {
        return -1;
    }
    s = &w->bg0_screens[idx];
    init_screen(s, col, row);
    if (w->bg0_active_screen == idx) {
        w->bg0_active_screen = -1;
        for (idx = 0; idx < w->bg0_screen_count; idx++) {
            if (w->bg0_screens[idx].present) {
                w->bg0_active_screen = idx;
                break;
            }
        }
    }
    r01_world_bg0_recompute_extent(w);
    return 0;
}

R01Screen *r01_project_active_bg0_screen(R01Project *p) {
    R01World *w = r01_project_active_world(p);
    if (!p || !w || !w->present) {
        return NULL;
    }
    if (w->bg0_active_screen < 0 || w->bg0_active_screen >= w->bg0_screen_count) {
        return NULL;
    }
    if (!w->bg0_screens[w->bg0_active_screen].present) {
        return NULL;
    }
    return &w->bg0_screens[w->bg0_active_screen];
}

int r01_world_set_grid(R01World *w, int cols, int rows) {
    int col, row, i;
    if (!w || cols < 1 || rows < 1 || cols > R01_GRID_MAX || rows > R01_GRID_MAX) {
        return -1;
    }
    w->grid_cols = cols;
    w->grid_rows = rows;
    w->screen_count = cols * rows;
    i = 0;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            init_screen(&w->screens[i++], col, row);
        }
    }
    return 0;
}

void r01_world_ensure_full_grid(R01World *w) {
    R01Screen *old;
    int n;
    int i;
    int def_c = -1;
    int def_r = -1;
    if (!w || (w->grid_cols == R01_GRID_MAX && w->grid_rows == R01_GRID_MAX)) {
        return;
    }
    n = w->screen_count;
    if (n < 1) {
        r01_world_set_grid(w, R01_GRID_MAX, R01_GRID_MAX);
        return;
    }
    if (w->default_screen >= 0 && w->default_screen < n && w->screens[w->default_screen].present) {
        def_c = w->screens[w->default_screen].col;
        def_r = w->screens[w->default_screen].row;
    }
    old = (R01Screen *)malloc(sizeof(R01Screen) * (size_t)n);
    if (!old) {
        return;
    }
    memcpy(old, w->screens, sizeof(R01Screen) * (size_t)n);
    r01_world_set_grid(w, R01_GRID_MAX, R01_GRID_MAX);
    for (i = 0; i < n; i++) {
        R01Screen *s;
        if (!old[i].present) {
            continue;
        }
        s = r01_world_screen_at(w, old[i].col, old[i].row);
        if (s) {
            *s = old[i];
        }
    }
    free(old);
    if (def_c >= 0) {
        w->default_screen = r01_world_screen_index(w, def_c, def_r);
    } else {
        r01_world_sync_default_screen(w);
    }
}

void r01_other_screen_init(R01OtherScreen *s) {
    int c;
    if (!s) {
        return;
    }
    s->present = 0;
    memset(s->tiles, 0, sizeof(s->tiles));
    for (c = 0; c < R01_TILES_PER_SCREEN; c++) {
        s->attrs[c] = r01_attr_pack(0, 0, 0, 0);
    }
}

void r01_project_init_other_screens(R01Project *p) {
    int i;
    if (!p) {
        return;
    }
    for (i = 0; i < R01_CART_OTHER_MAX; i++) {
        r01_other_screen_init(&p->other_screens[i]);
    }
    /* Title + interstitial always exported (may be blank). */
    p->other_screens[R01_CART_OTHER_TITLE].present = 1;
    p->other_screens[R01_CART_OTHER_INTER].present = 1;
}

void r01_world_init_phase1(R01World *w) {
    int col, row, idx;
    if (!w) {
        return;
    }
    memset(w, 0, sizeof(*w));
    w->present = 1;
    w->default_bg_bank = 0;
    w->default_pal_row = 0;
    w->player_entity = -1;
    w->bg0_active_screen = -1;
    r01_world_warps_init(w);
    /* Full 16x16 map slots; default authored region is 3x3 present blank screens. */
    r01_world_set_grid(w, R01_GRID_MAX, R01_GRID_MAX);
    for (row = 0; row < R01_DEFAULT_GRID; row++) {
        for (col = 0; col < R01_DEFAULT_GRID; col++) {
            idx = r01_world_screen_index(w, col, row);
            if (idx >= 0) {
                w->screens[idx].present = 1;
            }
        }
    }
    r01_world_sync_default_screen(w);
    r01_world_bg0_clear(w);
}

void r01_world_init_empty(R01World *w) {
    if (!w) {
        return;
    }
    memset(w, 0, sizeof(*w));
    w->present = 1;
    w->default_bg_bank = 0;
    w->default_pal_row = 0;
    w->player_entity = -1;
    w->bg0_active_screen = -1;
    r01_world_warps_init(w);
    r01_world_set_grid(w, R01_GRID_MAX, R01_GRID_MAX);
    r01_world_sync_default_screen(w);
    r01_world_bg0_clear(w);
}

void r01_project_init(R01Project *p, const char *name) {
    memset(p, 0, sizeof(*p));
    if (name && name[0]) {
        strncpy(p->name, name, R01_NAME_MAX - 1);
    } else {
        strncpy(p->name, "untitled", R01_NAME_MAX - 1);
    }
    p->default_world = 0;
    p->active_world = 0;
    p->active_screen = 0;
    r01_project_init_phase1_pals(p);
    r01_project_init_other_screens(p);
    r01_world_init_phase1(&p->worlds[0]);
    r01_project_select_start_screen(p);
}

R01World *r01_project_world0(R01Project *p) {
    return p ? &p->worlds[0] : NULL;
}

const R01World *r01_project_world0_const(const R01Project *p) {
    return p ? &p->worlds[0] : NULL;
}

R01World *r01_project_active_world(R01Project *p) {
    if (!p || p->active_world < 0 || p->active_world >= R01_MAX_WORLDS) {
        return NULL;
    }
    return &p->worlds[p->active_world];
}

const R01World *r01_project_active_world_const(const R01Project *p) {
    if (!p || p->active_world < 0 || p->active_world >= R01_MAX_WORLDS) {
        return NULL;
    }
    return &p->worlds[p->active_world];
}

int r01_world_present_count(const R01World *w) {
    int i, n = 0;
    if (!w) {
        return 0;
    }
    for (i = 0; i < w->screen_count; i++) {
        if (w->screens[i].present) {
            n++;
        }
    }
    return n;
}

int r01_world_find_screen(const R01World *w, int col, int row) {
    int i;
    if (!w) {
        return -1;
    }
    i = r01_world_screen_index(w, col, row);
    if (i < 0 || i >= w->screen_count) {
        return -1;
    }
    if (!w->screens[i].present) {
        return -1;
    }
    return i;
}

R01Screen *r01_world_screen_at(R01World *w, int col, int row) {
    int i = r01_world_screen_index(w, col, row);
    if (i < 0 || i >= w->screen_count) {
        return NULL;
    }
    return &w->screens[i];
}

R01Screen *r01_project_active_screen(R01Project *p) {
    R01World *w;
    w = r01_project_active_world(p);
    if (!p || !w || !w->present || p->active_screen < 0 || p->active_screen >= w->screen_count) {
        return NULL;
    }
    if (!w->screens[p->active_screen].present) {
        return NULL;
    }
    return &w->screens[p->active_screen];
}

void r01_project_select_start_screen(R01Project *p) {
    R01World *w;
    int idx;
    int i;
    if (!p) {
        return;
    }
    w = r01_project_active_world(p);
    if (!w || !w->present || w->screen_count < 1) {
        p->active_screen = 0;
        return;
    }
    idx = r01_world_screen_index(w, R01_START_COL, R01_START_ROW);
    if (idx >= 0 && idx < w->screen_count && w->screens[idx].present) {
        p->active_screen = idx;
        return;
    }
    for (i = 0; i < w->screen_count; i++) {
        if (w->screens[i].present) {
            p->active_screen = i;
            return;
        }
    }
    p->active_screen = (idx >= 0 && idx < w->screen_count) ? idx : 0;
}

static int world_start_screen_index(const R01World *w) {
    int idx;
    int i;
    if (!w || !w->present || w->screen_count < 1) {
        return 0;
    }
    idx = r01_world_screen_index(w, R01_START_COL, R01_START_ROW);
    if (idx >= 0 && idx < w->screen_count && w->screens[idx].present) {
        return idx;
    }
    for (i = 0; i < w->screen_count; i++) {
        if (w->screens[i].present) {
            return i;
        }
    }
    return (idx >= 0 && idx < w->screen_count) ? idx : 0;
}

int r01_world_default_screen(const R01World *w) {
    if (!w || !w->present || w->screen_count < 1) {
        return 0;
    }
    if (w->default_screen >= 0 && w->default_screen < w->screen_count &&
        w->screens[w->default_screen].present) {
        return w->default_screen;
    }
    return world_start_screen_index(w);
}

void r01_world_sync_default_screen(R01World *w) {
    if (w) {
        w->default_screen = world_start_screen_index(w);
    }
}

void r01_project_begin_play(R01Project *p) {
    R01World *w;
    if (!p) {
        return;
    }
    if (p->default_world < 0 || p->default_world >= R01_MAX_WORLDS ||
        !p->worlds[p->default_world].present) {
        p->default_world = 0;
    }
    p->active_world = p->default_world;
    w = r01_project_active_world(p);
    if (!w) {
        return;
    }
    if (w->default_screen < 0 || w->default_screen >= w->screen_count ||
        !w->screens[w->default_screen].present) {
        r01_world_sync_default_screen(w);
    }
    p->active_screen = r01_world_default_screen(w);
}

int r01_project_set_active_world(R01Project *p, int world_idx) {
    if (!p || world_idx < 0 || world_idx >= R01_MAX_WORLDS) {
        return -1;
    }
    if (!p->worlds[world_idx].present) {
        r01_world_init_empty(&p->worlds[world_idx]);
    }
    p->active_world = world_idx;
    r01_project_select_start_screen(p);
    return 0;
}

int r01_world_create_screen(R01World *w, int col, int row) {
    R01Screen *s;
    if (!w || !w->present || col < 0 || row < 0 || col >= R01_GRID_MAX || row >= R01_GRID_MAX) {
        return -1;
    }
    if (w->grid_cols < R01_GRID_MAX || w->grid_rows < R01_GRID_MAX) {
        /* Expand to full map while preserving present screens. */
        R01Screen *old;
        int n = w->screen_count;
        int i;
        old = (R01Screen *)malloc(sizeof(R01Screen) * (size_t)n);
        if (!old) {
            return -1;
        }
        memcpy(old, w->screens, sizeof(R01Screen) * (size_t)n);
        r01_world_set_grid(w, R01_GRID_MAX, R01_GRID_MAX);
        for (i = 0; i < n; i++) {
            if (!old[i].present) {
                continue;
            }
            s = r01_world_screen_at(w, old[i].col, old[i].row);
            if (s) {
                *s = old[i];
            }
        }
        free(old);
    }
    s = r01_world_screen_at(w, col, row);
    if (!s) {
        return -1;
    }
    if (!s->present) {
        if (r01_world_present_count(w) >= R01_MAX_PRESENT_SCREENS) {
            return -1;
        }
        init_screen(s, col, row);
        s->present = 1;
    }
    return r01_world_screen_index(w, col, row);
}

int r01_world_remove_screen(R01World *w, int col, int row) {
    R01Screen *s = r01_world_screen_at(w, col, row);
    int idx;
    int was_default;
    if (!s || !s->present) {
        return -1;
    }
    idx = r01_world_screen_index(w, col, row);
    was_default = (idx >= 0 && idx == w->default_screen);
    init_screen(s, col, row);
    if (was_default) {
        r01_world_sync_default_screen(w);
    }
    return 0;
}

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

typedef struct {
    uint8_t r, g, b;
} Rgb;

static int rgb_eq(Rgb a, Rgb b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static int cell_fully_transparent(const uint8_t *rgba, int png_w, int col, int row) {
    int y, x;
    int ox = col * R01_SCREEN_PX_W;
    int oy = row * R01_SCREEN_PX_H;
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            const uint8_t *p = rgba + ((size_t)(oy + y) * (size_t)png_w + (size_t)(ox + x)) * 4u;
            if (p[3] != 0) {
                return 0;
            }
        }
    }
    return 1;
}

static int collect_opaque_colors(const uint8_t *rgba, size_t npx, Rgb out[4], int *out_n, char *err_buf,
                                 size_t err_cap) {
    size_t i;
    int n = 0;
    for (i = 0; i < npx; i++) {
        const uint8_t *p = rgba + i * 4u;
        Rgb c;
        int j, found;
        if (p[3] == 0) {
            continue;
        }
        c.r = p[0];
        c.g = p[1];
        c.b = p[2];
        found = 0;
        for (j = 0; j < n; j++) {
            if (rgb_eq(out[j], c)) {
                found = 1;
                break;
            }
        }
        if (found) {
            continue;
        }
        if (n >= 4) {
            set_err(err_buf, err_cap, "need <=4 colors");
            return -1;
        }
        out[n++] = c;
    }
    *out_n = n;
    return 0;
}

static int build_png_palette(const Rgb colors[4], int ncolors, uint8_t master_for_index[4],
                             uint8_t color_to_index[4], char *err_buf, size_t err_cap) {
    int i;
    int next = 1;
    master_for_index[0] = 0;
    master_for_index[1] = 0;
    master_for_index[2] = 0;
    master_for_index[3] = 0;
    for (i = 0; i < ncolors; i++) {
        uint8_t m = (uint8_t)r01_kit_nearest_master(colors[i].r, colors[i].g, colors[i].b);
        if (m == 0) {
            color_to_index[i] = 0;
        } else {
            if (next > 3) {
                set_err(err_buf, err_cap, "need <=4 colors (incl. backdrop)");
                return -1;
            }
            master_for_index[next] = m;
            color_to_index[i] = (uint8_t)next;
            next++;
        }
    }
    return 0;
}

static uint8_t map_pixel(const uint8_t *p, const Rgb colors[4], const uint8_t color_to_index[4], int ncolors) {
    Rgb c;
    int j;
    if (p[3] == 0) {
        return 0;
    }
    c.r = p[0];
    c.g = p[1];
    c.b = p[2];
    for (j = 0; j < ncolors; j++) {
        if (rgb_eq(colors[j], c)) {
            return color_to_index[j];
        }
    }
    return 0;
}

static void fill_screen_from_cell(R01Screen *s, const uint8_t *rgba, int png_w, int png_col, int png_row,
                                  const Rgb colors[4], const uint8_t color_to_index[4], int ncolors) {
    int y, x;
    int ox = png_col * R01_SCREEN_PX_W;
    int oy = png_row * R01_SCREEN_PX_H;
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            const uint8_t *p = rgba + ((size_t)(oy + y) * (size_t)png_w + (size_t)(ox + x)) * 4u;
            s->pixels[y * R01_SCREEN_PX_W + x] = map_pixel(p, colors, color_to_index, ncolors);
        }
    }
}

int r01_project_import_png(R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    R01World *w;
    FILE *fp = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_uint_32 width = 0, height = 0;
    int bit_depth = 0;
    png_bytep *row_ptrs = NULL;
    uint8_t *rgba = NULL;
    Rgb colors[4];
    int ncolors = 0;
    int cols, rows, col, row;
    uint8_t master_for_index[4];
    uint8_t color_to_index[4];
    png_uint_32 y;
    size_t rowbytes;

    if (!p || !path || !path[0]) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    w = r01_project_active_world(p);
    if (!w) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        set_err(err_buf, err_cap, "cannot open png");
        return -1;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        set_err(err_buf, err_cap, "libpng init failed");
        goto fail;
    }
    if (setjmp(png_jmpbuf(png))) {
        set_err(err_buf, err_cap, "png read error");
        goto fail;
    }

    png_init_io(png, fp);
    png_read_info(png, info);
    png_get_IHDR(png, info, &width, &height, &bit_depth, NULL, NULL, NULL, NULL);

    if ((width % (png_uint_32)R01_SCREEN_PX_W) != 0 || (height % (png_uint_32)R01_SCREEN_PX_H) != 0) {
        set_err(err_buf, err_cap, "size must be multiple of 128x120");
        goto fail;
    }
    cols = (int)(width / (png_uint_32)R01_SCREEN_PX_W);
    rows = (int)(height / (png_uint_32)R01_SCREEN_PX_H);
    if (cols > R01_GRID_MAX || rows > R01_GRID_MAX) {
        set_err(err_buf, err_cap, "png grid exceeds 16x16");
        goto fail;
    }

    if (r01_world_set_grid(w, cols, rows) != 0) {
        set_err(err_buf, err_cap, "bad grid size");
        goto fail;
    }

    png_set_expand(png);
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    png_set_gray_to_rgb(png);
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    rowbytes = png_get_rowbytes(png, info);

    row_ptrs = (png_bytep *)calloc((size_t)height, sizeof(png_bytep));
    rgba = (uint8_t *)malloc((size_t)height * rowbytes);
    if (!row_ptrs || !rgba) {
        set_err(err_buf, err_cap, "oom");
        goto fail;
    }
    for (y = 0; y < height; y++) {
        row_ptrs[y] = rgba + (size_t)y * rowbytes;
    }
    png_read_image(png, row_ptrs);
    png_read_end(png, NULL);

    if (rowbytes != (size_t)width * 4u) {
        uint8_t *tight = (uint8_t *)malloc((size_t)width * (size_t)height * 4u);
        png_uint_32 yy;
        if (!tight) {
            set_err(err_buf, err_cap, "oom");
            goto fail;
        }
        for (yy = 0; yy < height; yy++) {
            memcpy(tight + (size_t)yy * (size_t)width * 4u, row_ptrs[yy], (size_t)width * 4u);
        }
        free(rgba);
        rgba = tight;
    }

    if (collect_opaque_colors(rgba, (size_t)width * (size_t)height, colors, &ncolors, err_buf, err_cap) != 0) {
        goto fail;
    }
    if (build_png_palette(colors, ncolors, master_for_index, color_to_index, err_buf, err_cap) != 0) {
        goto fail;
    }
    r01_project_set_bg_pals_from_png(p, master_for_index);

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            R01Screen *s = r01_world_screen_at(w, col, row);
            if (!s) {
                continue;
            }
            if (cell_fully_transparent(rgba, (int)width, col, row)) {
                s->present = 0;
                memset(s->pixels, 0, sizeof(s->pixels));
                continue;
            }
            s->present = 1;
            fill_screen_from_cell(s, rgba, (int)width, col, row, colors, color_to_index, ncolors);
        }
    }

    if (r01_chr_pack_world_bank0(w) == R01_CHR_TOO_MANY_TILES) {
        set_err(err_buf, err_cap, "too many unique tiles (>256)");
        goto fail;
    }
    if (r01_world_present_count(w) > R01_MAX_PRESENT_SCREENS) {
        if (err_buf && err_cap > 0) {
            snprintf(err_buf, err_cap, "png exceeds %d present screens", R01_MAX_PRESENT_SCREENS);
        }
        goto fail;
    }

    free(rgba);
    free(row_ptrs);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return 0;

fail:
    free(rgba);
    free(row_ptrs);
    if (png) {
        png_destroy_read_struct(&png, &info, NULL);
    }
    if (fp) {
        fclose(fp);
    }
    return -1;
}

int r01_path_ensure_parent(const char *path, char *err_buf, size_t err_cap) {
    char dir[R01_PATH_MAX];
    char *slash;
    if (!path || !path[0]) {
        set_err(err_buf, err_cap, "bad path");
        return -1;
    }
    strncpy(dir, path, sizeof(dir) - 1u);
    dir[sizeof(dir) - 1u] = '\0';
    slash = strrchr(dir, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';
    if (dir[0] == '\0') {
        return 0;
    }
    if (mkdir(dir, 0755) != 0) {
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            set_err(err_buf, err_cap, "cannot create output dir");
            return -1;
        }
    }
    return 0;
}

int r01_path_mkdir_p(const char *path, char *err_buf, size_t err_cap) {
    char tmp[R01_PATH_MAX];
    size_t i;
    if (!path || !path[0]) {
        set_err(err_buf, err_cap, "bad path");
        return -1;
    }
    if (strlen(path) >= sizeof(tmp)) {
        set_err(err_buf, err_cap, "path too long");
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] != '\0') {
                if (mkdir(tmp, 0755) != 0) {
                    struct stat st;
                    if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                        set_err(err_buf, err_cap, "cannot create dir");
                        return -1;
                    }
                }
            }
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0) {
        struct stat st;
        if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
            set_err(err_buf, err_cap, "cannot create dir");
            return -1;
        }
    }
    return 0;
}
