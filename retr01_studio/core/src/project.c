#include "retr01_studio/project.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/chr_pack.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void init_screen(R01Screen *s, int col, int row) {
    int c;
    s->col = col;
    s->row = row;
    s->present = 1;
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

void r01_world_init_phase1(R01World *w) {
    if (!w) {
        return;
    }
    memset(w, 0, sizeof(*w));
    w->present = 1;
    w->default_bg_bank = 0;
    w->default_pal_row = 0;
    r01_world_set_grid(w, R01_DEFAULT_GRID, R01_DEFAULT_GRID);
}

void r01_project_init(R01Project *p, const char *name) {
    memset(p, 0, sizeof(*p));
    if (name && name[0]) {
        strncpy(p->name, name, R01_NAME_MAX - 1);
    } else {
        strncpy(p->name, "untitled", R01_NAME_MAX - 1);
    }
    p->active_screen = R01_START_ROW * R01_DEFAULT_GRID + R01_START_COL;
    r01_project_init_phase1_pals(p);
    r01_world_init_phase1(&p->worlds[0]);
}

R01World *r01_project_world0(R01Project *p) {
    return p ? &p->worlds[0] : NULL;
}

const R01World *r01_project_world0_const(const R01Project *p) {
    return p ? &p->worlds[0] : NULL;
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
    w = r01_project_world0(p);
    if (!p || !w || p->active_screen < 0 || p->active_screen >= w->screen_count) {
        return NULL;
    }
    if (!w->screens[p->active_screen].present) {
        return NULL;
    }
    return &w->screens[p->active_screen];
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
            set_err(err_buf, err_cap, "need ≤4 colors");
            return -1;
        }
        out[n++] = c;
    }
    *out_n = n;
    return 0;
}

static uint8_t map_pixel(const uint8_t *p, const Rgb colors[4], int ncolors) {
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
            return (uint8_t)j;
        }
    }
    return 0;
}

static void fill_screen_from_cell(R01Screen *s, const uint8_t *rgba, int png_w, int png_col, int png_row,
                                  const Rgb colors[4], int ncolors) {
    int y, x;
    int ox = png_col * R01_SCREEN_PX_W;
    int oy = png_row * R01_SCREEN_PX_H;
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            const uint8_t *p = rgba + ((size_t)(oy + y) * (size_t)png_w + (size_t)(ox + x)) * 4u;
            s->pixels[y * R01_SCREEN_PX_W + x] = map_pixel(p, colors, ncolors);
        }
    }
}

int r01_world_import_png(R01World *w, const char *path, char *err_buf, size_t err_cap) {
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
    png_uint_32 y;
    size_t rowbytes;

    if (!w || !path || !path[0]) {
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
        set_err(err_buf, err_cap, "size must be multiple of 128×120");
        goto fail;
    }
    cols = (int)(width / (png_uint_32)R01_SCREEN_PX_W);
    rows = (int)(height / (png_uint_32)R01_SCREEN_PX_H);
    if (cols > R01_GRID_MAX || rows > R01_GRID_MAX) {
        set_err(err_buf, err_cap, "png grid exceeds 8×8");
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

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            R01Screen *s;
            if (cell_fully_transparent(rgba, (int)width, col, row)) {
                continue;
            }
            s = r01_world_screen_at(w, col, row);
            if (!s) {
                continue;
            }
            fill_screen_from_cell(s, rgba, (int)width, col, row, colors, ncolors);
        }
    }

    if (r01_chr_pack_world_bank0(w) == R01_CHR_TOO_MANY_TILES) {
        set_err(err_buf, err_cap, "too many unique tiles (>256)");
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
