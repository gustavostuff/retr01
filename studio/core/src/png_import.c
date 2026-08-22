#include "retr01_studio/project.h"

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
    /* Drop screens outside the new atlas. */
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

static int cell_fully_transparent(const uint8_t *idx, const uint8_t *alpha, int png_w, int col, int row) {
    int y, x;
    int ox = col * R01_SCREEN_PX_W;
    int oy = row * R01_SCREEN_PX_H;
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t pi = idx[(oy + y) * png_w + (ox + x)];
            if (alpha[pi] != 0) {
                return 0;
            }
        }
    }
    return 1;
}

static void fill_screen_from_cell(R01Screen *s, const uint8_t *idx, int png_w, int col, int row, int default_bank,
                                  int default_pal) {
    int y, x, cell;
    int ox = col * R01_SCREEN_PX_W;
    int oy = row * R01_SCREEN_PX_H;
    memset(s, 0, sizeof(*s));
    s->col = col;
    s->row = row;
    s->present = 1;
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t pi = idx[(oy + y) * png_w + (ox + x)];
            s->pixels[y * R01_SCREEN_PX_W + x] = (uint8_t)(pi & 3u);
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
    int bit_depth = 0, color_type = 0;
    png_colorp palette = NULL;
    int num_palette = 0;
    png_bytep trans = NULL;
    int num_trans = 0;
    png_bytep *row_ptrs = NULL;
    uint8_t *idx = NULL;
    uint8_t alpha[256];
    int cols, rows, col, row, present_cells;
    png_uint_32 y;

    if (!w || !path) {
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
    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    if (color_type != PNG_COLOR_TYPE_PALETTE) {
        set_err(err_buf, err_cap, "png must be indexed (palette) color");
        goto fail;
    }
    if (bit_depth > 8) {
        set_err(err_buf, err_cap, "png bit depth must be <= 8");
        goto fail;
    }
    if ((width % (png_uint_32)R01_SCREEN_PX_W) != 0 || (height % (png_uint_32)R01_SCREEN_PX_H) != 0) {
        set_err(err_buf, err_cap, "size must be multiple of 128x120");
        goto fail;
    }
    cols = (int)(width / (png_uint_32)R01_SCREEN_PX_W);
    rows = (int)(height / (png_uint_32)R01_SCREEN_PX_H);
    if (cols < 1 || rows < 1 || cols > R01_GRID_SIZE || rows > R01_GRID_SIZE) {
        set_err(err_buf, err_cap, "grid must be 1..8 screens on each axis");
        goto fail;
    }

    if (!png_get_PLTE(png, info, &palette, &num_palette) || num_palette < 1) {
        set_err(err_buf, err_cap, "png missing palette");
        goto fail;
    }
    if (num_palette > 4) {
        set_err(err_buf, err_cap, "png must use at most 4 palette colors");
        goto fail;
    }

    memset(alpha, 255, sizeof(alpha));
    if (png_get_tRNS(png, info, &trans, &num_trans, NULL) && trans && num_trans > 0) {
        int i;
        for (i = 0; i < num_trans && i < 256; i++) {
            alpha[i] = trans[i];
        }
    }

    if (bit_depth < 8) {
        png_set_packing(png);
    }
    png_read_update_info(png, info);

    row_ptrs = (png_bytep *)calloc((size_t)height, sizeof(png_bytep));
    idx = (uint8_t *)malloc((size_t)width * (size_t)height);
    if (!row_ptrs || !idx) {
        set_err(err_buf, err_cap, "oom");
        goto fail;
    }
    for (y = 0; y < height; y++) {
        row_ptrs[y] = idx + (size_t)y * (size_t)width;
    }
    png_read_image(png, row_ptrs);
    png_read_end(png, NULL);

    /* Remap any index >= num_palette (shouldn't happen) and clamp to 0..3. */
    {
        size_t n = (size_t)width * (size_t)height;
        size_t i;
        for (i = 0; i < n; i++) {
            if (idx[i] >= (uint8_t)num_palette) {
                idx[i] = 0;
            }
            if (idx[i] > 3) {
                idx[i] = 3;
            }
        }
    }

    present_cells = 0;
    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            if (!cell_fully_transparent(idx, alpha, (int)width, col, row)) {
                present_cells++;
            }
        }
    }
    if (present_cells > R01_MAX_SCREENS_PER_WORLD) {
        set_err(err_buf, err_cap, "too many non-transparent screens (max 32)");
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
            if (cell_fully_transparent(idx, alpha, (int)width, col, row)) {
                continue;
            }
            s = &w->screens[w->screen_count];
            fill_screen_from_cell(s, idx, (int)width, col, row, w->default_bg_bank, w->default_pal_row);
            w->screen_count++;
        }
    }

    free(idx);
    free(row_ptrs);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return 0;

fail:
    free(idx);
    free(row_ptrs);
    if (png) {
        png_destroy_read_struct(&png, &info, NULL);
    }
    if (fp) {
        fclose(fp);
    }
    return -1;
}
