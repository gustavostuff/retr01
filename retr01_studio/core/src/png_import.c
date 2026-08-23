#include "retr01_studio/project.h"
#include "retr01_studio/chr_pack.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

void r01_world_clear_screens(R01World *w) {
    if (!w) {
        return;
    }
    memset(w->screens, 0, sizeof(w->screens));
    w->screen_count = 0;
}

int r01_world_set_grid(R01World *w, int cols, int rows) {
    int i;
    if (!w || cols < 1 || rows < 1 || cols > R01_GRID_SIZE || rows > R01_GRID_SIZE) {
        return -1;
    }
    w->grid_cols = cols;
    w->grid_rows = rows;
    for (i = w->screen_count - 1; i >= 0; i--) {
        R01Screen *s = &w->screens[i];
        if (s->col < 0 || s->row < 0 || s->col >= cols || s->row >= rows) {
            int j;
            for (j = i; j < w->screen_count - 1; j++) {
                w->screens[j] = w->screens[j + 1];
            }
            w->screen_count--;
            memset(&w->screens[w->screen_count], 0, sizeof(R01Screen));
        }
    }
    return 0;
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
            set_err(err_buf, err_cap, "need ≤4 colors (indexed or flat RGB)");
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

static void fill_screen_from_cell(R01Screen *s, const uint8_t *rgba, int png_w, int col, int row,
                                  const Rgb colors[4], int ncolors, int default_bank, int default_pal) {
    int y, x, cell;
    int ox = col * R01_SCREEN_PX_W;
    int oy = row * R01_SCREEN_PX_H;
    memset(s, 0, sizeof(*s));
    s->col = col;
    s->row = row;
    s->present = 1;
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            const uint8_t *p = rgba + ((size_t)(oy + y) * (size_t)png_w + (size_t)(ox + x)) * 4u;
            s->pixels[y * R01_SCREEN_PX_W + x] = map_pixel(p, colors, ncolors);
        }
    }
    for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
        s->attrs[cell] = r01_attr_pack(default_bank, default_pal, 0, 0, 0, 0);
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
    int cols, rows, col, row, present_cells;
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
    if (cols < 1 || rows < 1 || cols > R01_GRID_SIZE || rows > R01_GRID_SIZE) {
        set_err(err_buf, err_cap, "grid must be 1..8 screens per axis");
        goto fail;
    }

    /* Expand to 8-bit RGBA for a single validation path. */
    png_set_expand(png); /* palette / tRNS / low-bit gray → 8-bit */
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    png_set_gray_to_rgb(png);
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER); /* RGB → RGBA if no alpha yet */
    png_read_update_info(png, info);
    rowbytes = png_get_rowbytes(png, info);
    if (png_get_channels(png, info) != 4 || rowbytes < (size_t)width * 4u) {
        set_err(err_buf, err_cap, "png expand failed");
        goto fail;
    }

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

    /* Pack tightly to RGBA if rowbytes has padding. */
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

    present_cells = 0;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            if (!cell_fully_transparent(rgba, (int)width, col, row)) {
                present_cells++;
            }
        }
    }
    if (present_cells == 0) {
        set_err(err_buf, err_cap, "no opaque screens in png");
        goto fail;
    }
    if (present_cells > R01_MAX_SCREENS_PER_WORLD) {
        set_err(err_buf, err_cap, "too many screens (max 32)");
        goto fail;
    }

    r01_world_clear_screens(w);
    if (r01_world_set_grid(w, cols, rows) != 0) {
        set_err(err_buf, err_cap, "bad grid");
        goto fail;
    }
    w->present = 1;

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            R01Screen *s;
            if (cell_fully_transparent(rgba, (int)width, col, row)) {
                continue;
            }
            s = &w->screens[w->screen_count];
            fill_screen_from_cell(s, rgba, (int)width, col, row, colors, ncolors, w->default_bg_bank,
                                  w->default_pal_row);
            w->screen_count++;
        }
    }

    /* Auto-pack unique 8×8 patterns into BG banks 0→3. */
    {
        R01ChrPackStatus st = r01_chr_pack_world_spill(w);
        if (st != R01_CHR_OK) {
            int bi;
            r01_world_clear_screens(w);
            for (bi = 0; bi < R01_BG_BANKS; bi++) {
                memset(w->bg_banks[bi].chr, 0, R01_BANK_CHR_BYTES);
                w->bg_banks[bi].tile_count = 0;
            }
            set_err(err_buf, err_cap,
                    st == R01_CHR_TOO_MANY_TILES ? "too many unique tiles (>4 BG banks)" : "chr pack failed");
            goto fail;
        }
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
