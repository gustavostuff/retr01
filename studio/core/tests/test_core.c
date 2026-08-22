#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"
#include "retr01_studio/spr_pack.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fails++;
    }
}

static void test_grid_caps(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    int i, rc;
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    for (i = 0; i < R01_MAX_SCREENS_PER_WORLD; i++) {
        rc = r01_world_toggle_screen(w, i % R01_GRID_SIZE, i / R01_GRID_SIZE);
        expect_true(rc == 0, "add screen under cap");
    }
    expect_true(w->screen_count == R01_MAX_SCREENS_PER_WORLD, "screen_count at cap");
    {
        int col = -1, row = -1, c, r;
        for (r = 0; r < R01_GRID_SIZE && col < 0; r++) {
            for (c = 0; c < R01_GRID_SIZE; c++) {
                if (r01_world_find_screen(w, c, r) < 0) {
                    col = c;
                    row = r;
                    break;
                }
            }
        }
        if (col >= 0) {
            rc = r01_world_toggle_screen(w, col, row);
            expect_true(rc == -1, "reject over cap");
        }
    }
    rc = r01_world_toggle_screen(w, w->screens[0].col, w->screens[0].row);
    expect_true(rc == 0 && w->screen_count == R01_MAX_SCREENS_PER_WORLD - 1, "remove screen");
    free(w);
}

static void test_chr_pack(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    R01ChrPackStatus st;
    int y, x, k;
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    r01_world_toggle_screen(w, 0, 0);
    r01_screen_clear_pixels(&w->screens[0], 0);
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            r01_screen_plot(&w->screens[0], x, y, 1);
        }
    }
    for (y = 0; y < 8; y++) {
        for (x = 8; x < 16; x++) {
            r01_screen_plot(&w->screens[0], x, y, 2);
        }
    }
    st = r01_chr_pack_world_bank(w, 0);
    expect_true(st == R01_CHR_OK, "pack ok");
    expect_true(w->bg_banks[0].tile_count == 3, "empty + two painted tiles");
    expect_true(w->screens[0].tiles[0] != w->screens[0].tiles[1], "indices differ");
    expect_true((w->screens[0].attrs[0] & R01_ATTR_BANK_MASK) == 0, "bank stamped");
    {
        uint8_t t[16], pix[64];
        r01_tile_from_pixels(w->screens[0].pixels, 0, 0, t);
        r01_tile_to_pixels(t, pix);
        for (k = 0; k < 64; k++) {
            expect_true(pix[k] == 1, "decode solid tile color");
            if (pix[k] != 1) {
                break;
            }
        }
    }
    free(w);
}

static void test_chr_flip_dedupe(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    int y;
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    r01_world_toggle_screen(w, 0, 0);
    r01_screen_clear_pixels(&w->screens[0], 0);
    /* asymmetric tile at (0,0) */
    for (y = 0; y < 8; y++) {
        r01_screen_plot(&w->screens[0], 0, y, 3);
        r01_screen_plot(&w->screens[0], 1, y, 1);
    }
    /* horizontal mirror at (1,0) */
    for (y = 0; y < 8; y++) {
        r01_screen_plot(&w->screens[0], 8 + 7, y, 3);
        r01_screen_plot(&w->screens[0], 8 + 6, y, 1);
    }
    expect_true(r01_chr_pack_world_bank(w, 0) == R01_CHR_OK, "flip pack ok");
    expect_true(w->bg_banks[0].tile_count == 2, "flip deduped to empty+1"); /* empty + one unique */
    expect_true(w->screens[0].tiles[0] == w->screens[0].tiles[1], "same tile index");
    expect_true(r01_attr_flip_h(w->screens[0].attrs[1]), "FLIP_H stamped");
    free(w);
}

static void test_chr_anim_strip(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    r01_world_toggle_screen(w, 0, 0);
    r01_screen_clear_pixels(&w->screens[0], 1);
    w->screens[0].attrs[0] = R01_ATTR_ANIM;
    expect_true(r01_chr_pack_world_bank(w, 1) == R01_CHR_OK, "anim pack ok");
    expect_true((w->screens[0].tiles[0] & 3) == 0, "anim base 4-aligned");
    expect_true(w->bg_banks[1].tile_count >= 4, "anim reserved 4 slots");
    expect_true(r01_attr_anim(w->screens[0].attrs[0]), "anim preserved");
    expect_true(r01_attr_bank(w->screens[0].attrs[0]) == 1, "anim bank");
    free(w);
}

static void test_chr_overflow(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    int tx, ty;
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    r01_world_toggle_screen(w, 0, 0);
    r01_screen_clear_pixels(&w->screens[0], 0);
    for (ty = 0; ty < R01_SCREEN_TILES_Y; ty++) {
        for (tx = 0; tx < R01_SCREEN_TILES_X; tx++) {
            int id = ty * R01_SCREEN_TILES_X + tx;
            int base = (ty * 8) * R01_SCREEN_PX_W + (tx * 8);
            w->screens[0].pixels[base + 0] = (uint8_t)(id % 4);
            w->screens[0].pixels[base + 1] = (uint8_t)((id / 4) % 4);
            w->screens[0].pixels[base + 2] = (uint8_t)((id / 16) % 4);
            w->screens[0].pixels[base + 3] = (uint8_t)((id / 64) % 4);
        }
    }
    expect_true(r01_chr_pack_world_bank(w, 1) == R01_CHR_OK, "240 unique fits");
    expect_true(w->bg_banks[1].tile_count == 240, "240 tiles packed");

    r01_world_toggle_screen(w, 1, 0);
    r01_screen_clear_pixels(&w->screens[1], 0);
    for (ty = 0; ty < 2; ty++) {
        for (tx = 0; tx < 10; tx++) {
            int raw = 240 + ty * 10 + tx;
            int id = raw > 255 ? 255 : raw;
            int base = (ty * 8) * R01_SCREEN_PX_W + (tx * 8);
            w->screens[1].pixels[base + 0] = (uint8_t)(id % 4);
            w->screens[1].pixels[base + 1] = (uint8_t)((id / 4) % 4);
            w->screens[1].pixels[base + 2] = (uint8_t)((id / 16) % 4);
            w->screens[1].pixels[base + 3] = (uint8_t)((id / 64) % 4);
            if (raw > 255) {
                w->screens[1].pixels[base + 4] = 3;
            }
        }
    }
    expect_true(r01_chr_pack_world_bank(w, 2) == R01_CHR_TOO_MANY_TILES, "overflow detected");
    free(w);
}

static void test_palette_quantize(void) {
    uint8_t packed, r, g, b;
    packed = r01_quantize_r3g3b2(255, 255, 255);
    expect_true(packed == 0xFF, "white pack");
    r01_kit_rgb(48, &r, &g, &b);
    expect_true(r == 0xFF && g == 0xFF && b == 0xFF, "kit white");
}

static void test_roundtrip(void) {
    R01Project *a = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *b = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];
    const char *path = "studio_test_roundtrip.json";
    expect_true(a && b, "alloc projects");
    r01_project_init(a, "roundtrip");
    a->worlds[0].present = 1;
    a->worlds[0].default_bg_bank = 2;
    a->worlds[0].default_pal_row = 1;
    a->global_pal_bg[0].idx[0] = 12;
    a->global_pal_bg[0].idx[3] = 55;
    r01_world_toggle_screen(&a->worlds[0], 2, 3);
    r01_screen_plot(&a->worlds[0].screens[0], 10, 20, 3);
    a->worlds[0].screens[0].attrs[5] =
        r01_attr_pack(2, 1, 1, 0, 1, 0);
    r01_world_toggle_plane(&a->worlds[0], 0);
    r01_tilemap_plot(a->worlds[0].planes[0].pixels, 4, 4, 2);
    r01_spr_tile_plot(&a->worlds[0], 0, 3, 2, 2, 1);
    r01_screen_oam_add(&a->worlds[0].screens[0], 40, 50, 3, r01_oam_pack(0, 1, 0, 0, 0, 0));
    a->active_screen = 0;
    a->active_plane = -1;
    a->generate_bank = 2;
    a->constraints.scroll_mode = R01_SCROLL_DEADZONE;
    a->constraints.deadzone_x = 30;
    a->constraints.anim_rate = 7;
    a->constraints.transition = R01_XITION_FADE;
    a->has_cart_save = 1;
    a->worlds[0].use_constraints = 1;
    a->worlds[0].constraints.scroll_mode = R01_SCROLL_INSTANT;
    a->worlds[0].constraints.player_meta = 0;
    a->worlds[0].grid_cols = 4;
    a->worlds[0].grid_rows = 3;
    expect_true(r01_chr_pack_world_bank(&a->worlds[0], 2) == R01_CHR_OK, "pack before save");
    expect_true(r01_project_save_json(a, path, err, sizeof(err)) == 0, "save");
    expect_true(r01_project_load_json(b, path, err, sizeof(err)) == 0, "load");
    expect_true(strcmp(b->name, "roundtrip") == 0, "name");
    expect_true(b->generate_bank == 2, "generate_bank");
    expect_true(b->constraints.scroll_mode == R01_SCROLL_DEADZONE, "proj scroll");
    expect_true(b->constraints.deadzone_x == 30 && b->constraints.anim_rate == 7, "proj constraints");
    expect_true(b->constraints.transition == R01_XITION_FADE, "proj xition");
    expect_true(b->has_cart_save == 1, "has_cart_save");
    expect_true(b->worlds[0].use_constraints == 1, "world use_constraints");
    expect_true(b->worlds[0].constraints.scroll_mode == R01_SCROLL_INSTANT, "world scroll");
    expect_true(b->worlds[0].constraints.player_meta == 0, "world player_meta");
    expect_true(b->worlds[0].grid_cols == 4 && b->worlds[0].grid_rows == 3, "grid size");
    expect_true(b->global_pal_bg[0].idx[0] == 12 && b->global_pal_bg[0].idx[3] == 55, "global pal");
    expect_true(b->worlds[0].default_bg_bank == 2, "default_bg_bank");
    expect_true(b->worlds[0].default_pal_row == 1, "default_pal_row");
    expect_true(b->worlds[0].screen_count == 1, "screen_count");
    expect_true(b->worlds[0].screens[0].col == 2 && b->worlds[0].screens[0].row == 3, "screen pos");
    expect_true(r01_screen_get_pixel(&b->worlds[0].screens[0], 10, 20) == 3, "pixel");
    expect_true(b->worlds[0].screens[0].attrs[5] == a->worlds[0].screens[0].attrs[5], "attrs");
    expect_true(b->worlds[0].planes[0].present == 1, "plane present");
    expect_true(b->worlds[0].planes[0].slot == 0, "plane slot");
    expect_true(r01_tilemap_get_pixel(b->worlds[0].planes[0].pixels, 4, 4) == 2, "plane pixel");
    expect_true(b->worlds[0].screens[0].oam_count == 1, "oam count");
    expect_true(b->worlds[0].screens[0].oam[0].x == 40 && b->worlds[0].screens[0].oam[0].tile == 3, "oam");
    expect_true(b->worlds[0].spr_banks[0].tile_count >= 4, "spr tiles");
    expect_true(b->worlds[0].bg_banks[2].tile_count == a->worlds[0].bg_banks[2].tile_count, "chr count");
    remove(path);
    free(a);
    free(b);
}

static void test_plane_pack(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    r01_world_toggle_plane(w, 1);
    r01_tilemap_clear_pixels(w->planes[1].pixels, 0);
    r01_tilemap_plot(w->planes[1].pixels, 0, 0, 3);
    expect_true(r01_chr_pack_world_bank(w, 0) == R01_CHR_OK, "plane-only pack");
    expect_true(w->bg_banks[0].tile_count >= 1, "plane contributed tiles");
    expect_true(w->planes[1].present == 1 && w->planes[1].slot == 1, "slot preserved");
    free(w);
}

static void test_select_bg_bank(void) {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    expect_true(p != NULL, "alloc project");
    r01_project_init(p, "banks");
    expect_true(r01_project_select_bg_bank(p, 2) == 2, "select bank 2");
    expect_true(p->generate_bank == 2, "generate_bank is 2");
    expect_true(r01_project_select_bg_bank(p, -1) == -1, "reject negative");
    expect_true(r01_project_select_bg_bank(p, 4) == -1, "reject out of range");
    expect_true(p->generate_bank == 2, "unchanged after reject");

    p->worlds[0].present = 1;
    r01_world_toggle_screen(&p->worlds[0], 0, 0);
    r01_screen_clear_pixels(&p->worlds[0].screens[0], 0);
    r01_screen_plot(&p->worlds[0].screens[0], 0, 0, 3);
    expect_true(r01_chr_pack_world_bank(&p->worlds[0], p->generate_bank) == R01_CHR_OK, "pack selected");
    expect_true(p->worlds[0].bg_banks[2].tile_count > 0, "tiles in bank 2");
    expect_true(p->worlds[0].bg_banks[0].tile_count == 0, "bank 0 still empty");

    expect_true(r01_project_select_bg_bank(p, 1) == 1, "switch to bank 1");
    expect_true(p->generate_bank == 1, "generate_bank follows select");
    free(p);
}

static void test_spr_oam_meta(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    int idx, mid;
    expect_true(w != NULL, "alloc world");
    w->present = 1;
    r01_world_toggle_screen(w, 0, 0);
    r01_spr_tile_plot(w, 1, 0, 0, 0, 3);
    r01_spr_tile_plot(w, 1, 0, 1, 0, 2);
    r01_spr_tile_plot(w, 1, 1, 0, 0, 1);
    expect_true(w->spr_banks[1].tile_count >= 2, "tiles ensured");
    idx = r01_screen_oam_add(&w->screens[0], 10, 20, 0, r01_oam_pack(1, 0, 0, 0, 0, 0));
    expect_true(idx == 0, "oam add");
    expect_true(r01_screen_oam_hit(&w->screens[0], 12, 22) == 0, "oam hit");
    mid = r01_meta_create_from_oam(w, &w->screens[0], &idx, 1);
    expect_true(mid == 0 && w->meta_count == 1, "meta create");
    expect_true(r01_spr_pack_world_bank(w, 1) == R01_CHR_OK, "spr pack");
    expect_true(w->spr_banks[1].tile_count >= 1, "spr packed");
    free(w);
}

static void test_play_scroll(void) {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01PlayState pl;
    const R01Constraints *c;
    expect_true(p != NULL, "alloc project");
    r01_project_init(p, "play");
    p->worlds[0].present = 1;
    r01_world_toggle_screen(&p->worlds[0], 0, 0);
    r01_world_toggle_screen(&p->worlds[0], 1, 0);
    r01_screen_clear_pixels(&p->worlds[0].screens[0], 1);
    r01_screen_clear_pixels(&p->worlds[0].screens[1], 2);
    p->active_screen = 0;
    p->constraints.scroll_mode = R01_SCROLL_PIXEL;
    r01_play_start(&pl, p);
    expect_true(pl.active == 1, "play active");
    expect_true(pl.player_x == R01_SCREEN_PX_W / 2, "start x");
    expect_true(pl.cam_x == 0, "start cam");

    /* walk into second screen */
    {
        int i;
        for (i = 0; i < R01_SCREEN_PX_W; i++) {
            r01_play_tick(&pl, p, 1, 0);
        }
    }
    expect_true(pl.player_x / R01_SCREEN_PX_W == 1, "crossed into col 1");
    expect_true(pl.cam_x > 0, "pixel cam followed");

    /* block empty neighbor */
    {
        int blocked = pl.player_x;
        r01_play_tick(&pl, p, 0, -1); /* up into empty */
        expect_true(pl.player_y == R01_SCREEN_PX_H / 2 || pl.player_y >= 0, "y ok");
        (void)blocked;
        r01_play_tick(&pl, p, 0, -R01_SCREEN_PX_H);
        expect_true(r01_world_find_screen(&p->worlds[0], pl.player_x / R01_SCREEN_PX_W,
                                         pl.player_y / R01_SCREEN_PX_H) >= 0,
                    "stayed on present screen");
    }

    p->constraints.scroll_mode = R01_SCROLL_INSTANT;
    r01_play_start(&pl, p);
    {
        int i;
        for (i = 0; i < R01_SCREEN_PX_W; i++) {
            r01_play_tick(&pl, p, 1, 0);
        }
    }
    expect_true(pl.cam_x == R01_SCREEN_PX_W, "instant cam snaps");

    p->constraints.scroll_mode = R01_SCROLL_DEADZONE;
    p->constraints.deadzone_x = 40;
    r01_play_start(&pl, p);
    expect_true(pl.cam_x == 0, "dz start cam");
    r01_play_tick(&pl, p, 1, 0);
    expect_true(pl.cam_x == 0, "dz hold while inside");

    p->worlds[0].use_constraints = 1;
    p->worlds[0].constraints.scroll_mode = R01_SCROLL_HYBRID;
    c = r01_project_constraints(p);
    expect_true(c == &p->worlds[0].constraints, "world override");
    expect_true(c->scroll_mode == R01_SCROLL_HYBRID, "hybrid");
    free(p);
}

static void test_cart_export(void) {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    uint8_t *img = NULL;
    size_t len = 0;
    char err[128];
    int wc = 0;
    uint32_t poff = 0, plen = 0;
    uint8_t prom[64];
    expect_true(p != NULL, "alloc");
    r01_project_init(p, "cart");
    p->has_cart_save = 1;
    p->worlds[0].present = 1;
    r01_world_toggle_screen(&p->worlds[0], 0, 0);
    r01_world_toggle_screen(&p->worlds[0], 1, 0);
    r01_screen_clear_pixels(&p->worlds[0].screens[0], 1);
    r01_screen_plot(&p->worlds[0].screens[0], 0, 0, 3);
    r01_screen_clear_pixels(&p->worlds[0].screens[1], 2);
    expect_true(r01_cart_build(p, &img, &len, err, sizeof(err)) == 0, "build");
    expect_true(img != NULL && len > 32768, "size");
    expect_true(r01_cart_peek_header(img, len, &wc, &poff, &plen) == 0, "peek");
    expect_true(wc == 1, "world_count");
    expect_true(plen == R01_PRG_BYTES, "prg len");
    expect_true(memcmp(img, "RETR01", 6) == 0, "magic");
    expect_true((img[8] & R01_CART_FLAG_I2C_SAVE) != 0, "i2c flag");
    expect_true(img[poff + 0x7FFC] == 0x00 && img[poff + 0x7FFD] == 0x80, "reset vector");
    expect_true(r01_cart_write(p, "studio_test_cart.retr01", err, sizeof(err)) == 0, "write");
    expect_true(r01_prom_write("studio_test_prom.bin", err, sizeof(err)) == 0, "prom");
    expect_true(r01_prg_write_asm(p, "studio_test_boot.s", err, sizeof(err)) == 0, "asm");
    r01_prom_fill(prom);
    expect_true(prom[0] == r01_quantize_r3g3b2(0, 0, 0), "prom black");
    expect_true(prom[48] == 0xFF, "prom white");
    remove("studio_test_cart.retr01");
    remove("studio_test_prom.bin");
    remove("studio_test_boot.s");
    free(img);
    free(p);
}

static int write_test_indexed_png(const char *path, int cols, int rows, int transparent_cell_col,
                                  int transparent_cell_row) {
    FILE *fp;
    png_structp png;
    png_infop info;
    png_color pal[4];
    png_byte trans[4];
    int w = cols * R01_SCREEN_PX_W;
    int h = rows * R01_SCREEN_PX_H;
    png_bytep *rows_ptr;
    int y, x;
    uint8_t *buf;

    fp = fopen(path, "wb");
    if (!fp) {
        return -1;
    }
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        fclose(fp);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, (png_uint_32)w, (png_uint_32)h, 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    pal[0].red = 0;
    pal[0].green = 0;
    pal[0].blue = 0;
    pal[1].red = 255;
    pal[1].green = 0;
    pal[1].blue = 0;
    pal[2].red = 0;
    pal[2].green = 255;
    pal[2].blue = 0;
    pal[3].red = 0;
    pal[3].green = 0;
    pal[3].blue = 255;
    png_set_PLTE(png, info, pal, 4);
    trans[0] = 0; /* index 0 fully transparent for hole cell fill */
    trans[1] = 255;
    trans[2] = 255;
    trans[3] = 255;
    png_set_tRNS(png, info, trans, 4, NULL);
    png_write_info(png, info);

    buf = (uint8_t *)malloc((size_t)w * (size_t)h);
    rows_ptr = (png_bytep *)malloc((size_t)h * sizeof(png_bytep));
    if (!buf || !rows_ptr) {
        free(buf);
        free(rows_ptr);
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }
    for (y = 0; y < h; y++) {
        rows_ptr[y] = buf + (size_t)y * (size_t)w;
        for (x = 0; x < w; x++) {
            int c = x / R01_SCREEN_PX_W;
            int r = y / R01_SCREEN_PX_H;
            if (c == transparent_cell_col && r == transparent_cell_row) {
                buf[y * w + x] = 0; /* transparent */
            } else {
                buf[y * w + x] = (uint8_t)(1 + ((c + r) & 2)); /* opaque 1 or 3 */
            }
        }
    }
    png_write_image(png, rows_ptr);
    png_write_end(png, NULL);
    free(buf);
    free(rows_ptr);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 0;
}

static void test_png_import(void) {
    R01World *w = (R01World *)calloc(1, sizeof(R01World));
    char err[128];
    const char *path = "studio_test_import.png";
    expect_true(w != NULL, "alloc");
    w->present = 1;
    w->grid_cols = R01_GRID_SIZE;
    w->grid_rows = R01_GRID_SIZE;
    expect_true(write_test_indexed_png(path, 3, 2, 1, 0) == 0, "write png");
    /* 3x2 with hole at (1,0) -> 5 screens */
    expect_true(r01_world_import_png(w, path, err, sizeof(err)) == 0, "import");
    expect_true(w->grid_cols == 3 && w->grid_rows == 2, "grid sized to png");
    expect_true(w->screen_count == 5, "skip transparent cell");
    expect_true(r01_world_find_screen(w, 1, 0) < 0, "hole empty");
    expect_true(r01_world_find_screen(w, 0, 0) >= 0, "cell 0,0 present");
    expect_true(r01_world_find_screen(w, 2, 1) >= 0, "cell 2,1 present");
    {
        int si = r01_world_find_screen(w, 0, 0);
        expect_true(si >= 0 && r01_screen_get_pixel(&w->screens[si], 0, 0) == 1, "pixel color");
    }
    /* reject non-multiple size via bad path already covered; reject >4 colors would need another file */
    remove(path);
    free(w);
}

int main(void) {
    g_fails = 0;
    test_grid_caps();
    test_chr_pack();
    test_chr_flip_dedupe();
    test_chr_anim_strip();
    test_chr_overflow();
    test_palette_quantize();
    test_plane_pack();
    test_select_bg_bank();
    test_spr_oam_meta();
    test_play_scroll();
    test_cart_export();
    test_png_import();
    test_roundtrip();
    if (g_fails) {
        fprintf(stderr, "%d test(s) failed\n", g_fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
