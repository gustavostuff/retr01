#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

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
    a->worlds[0].screens[0].parallax = 1;
    a->worlds[0].screens[0].attrs[5] =
        r01_attr_pack(2, 1, 1, 0, 1, 0);
    a->active_screen = 0;
    a->generate_bank = 2;
    expect_true(r01_chr_pack_world_bank(&a->worlds[0], 2) == R01_CHR_OK, "pack before save");
    expect_true(r01_project_save_json(a, path, err, sizeof(err)) == 0, "save");
    expect_true(r01_project_load_json(b, path, err, sizeof(err)) == 0, "load");
    expect_true(strcmp(b->name, "roundtrip") == 0, "name");
    expect_true(b->generate_bank == 2, "generate_bank");
    expect_true(b->global_pal_bg[0].idx[0] == 12 && b->global_pal_bg[0].idx[3] == 55, "global pal");
    expect_true(b->worlds[0].default_bg_bank == 2, "default_bg_bank");
    expect_true(b->worlds[0].default_pal_row == 1, "default_pal_row");
    expect_true(b->worlds[0].screen_count == 1, "screen_count");
    expect_true(b->worlds[0].screens[0].col == 2 && b->worlds[0].screens[0].row == 3, "screen pos");
    expect_true(b->worlds[0].screens[0].parallax == 1, "parallax");
    expect_true(r01_screen_get_pixel(&b->worlds[0].screens[0], 10, 20) == 3, "pixel");
    expect_true(b->worlds[0].screens[0].attrs[5] == a->worlds[0].screens[0].attrs[5], "attrs");
    expect_true(b->worlds[0].bg_banks[2].tile_count == a->worlds[0].bg_banks[2].tile_count, "chr count");
    remove(path);
    free(a);
    free(b);
}

int main(void) {
    g_fails = 0;
    test_grid_caps();
    test_chr_pack();
    test_chr_flip_dedupe();
    test_chr_anim_strip();
    test_chr_overflow();
    test_palette_quantize();
    test_roundtrip();
    if (g_fails) {
        fprintf(stderr, "%d test(s) failed\n", g_fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
