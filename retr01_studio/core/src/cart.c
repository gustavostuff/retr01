#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDR_SIZE 16
#define PTR_TABLE_SIZE 24
#define WORLD_SLOT_SIZE 8
#define WORLD_TABLE_SIZE (R01_MAX_WORLDS * WORLD_SLOT_SIZE)
#define WORLD_HDR_SIZE 32
#define SCREEN_DIR_ENT 12
#define SCREEN_PAYLOAD 480

typedef struct Buf {
    uint8_t *data;
    size_t len;
    size_t cap;
} Buf;

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

static int buf_reserve(Buf *b, size_t need) {
    uint8_t *n;
    size_t cap = b->cap ? b->cap : 4096;
    if (need <= b->cap) {
        return 0;
    }
    while (cap < need) {
        cap *= 2;
    }
    n = (uint8_t *)realloc(b->data, cap);
    if (!n) {
        return -1;
    }
    b->data = n;
    b->cap = cap;
    return 0;
}

static int buf_append(Buf *b, const void *src, size_t n) {
    if (buf_reserve(b, b->len + n) != 0) {
        return -1;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 0;
}

static int buf_pad(Buf *b, size_t to_len, uint8_t fill) {
    if (to_len < b->len || buf_reserve(b, to_len) != 0) {
        return -1;
    }
    memset(b->data + b->len, fill, to_len - b->len);
    b->len = to_len;
    return 0;
}

static void put_u8(uint8_t *p, uint8_t v) {
    p[0] = v;
}

static void put_u24(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
}

void r01_prom_fill(uint8_t out64[R01_MASTER_COLORS]) {
    int i;
    for (i = 0; i < R01_MASTER_COLORS; i++) {
        uint8_t r, g, b;
        r01_kit_rgb(i, &r, &g, &b);
        out64[i] = r01_quantize_r3g3b2(r, g, b);
    }
}

int r01_prom_write(const char *path, char *err_buf, size_t err_cap) {
    uint8_t prom[R01_MASTER_COLORS];
    FILE *f;
    if (!path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    r01_prom_fill(prom);
    f = fopen(path, "wb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write prom");
        return -1;
    }
    if (fwrite(prom, 1, sizeof(prom), f) != sizeof(prom)) {
        fclose(f);
        set_err(err_buf, err_cap, "prom write failed");
        return -1;
    }
    fclose(f);
    return 0;
}

int r01_prg_write_asm(const R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    FILE *f;
    const R01World *w;
    if (!path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    f = fopen(path, "wb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write asm");
        return -1;
    }
    w = p ? &p->worlds[0] : NULL;
    fprintf(f, "; retr01 Phase 1 — Smooth + Eagle View (Studio Play SoT)\n");
    fprintf(f, "; Gameplay: Studio play.c / emu cart runtime (marker R01P @ $80F0).\n");
    fprintf(f, "; Play table @ $8100: present[8] bitmask, spawn_col, spawn_row.\n");
    fprintf(f, ".setcpu \"65C02\"\n");
    fprintf(f, "WORLD     = $FE30\n");
    fprintf(f, "SCROLL_X  = $FE02\n");
    fprintf(f, "SCROLL_Y  = $FE03\n");
    fprintf(f, "PPUCTRL   = $FE00\n");
    fprintf(f, "PPUSTATUS = $FE01\n");
    fprintf(f, "PAD0      = $FE60\n");
    fprintf(f, ".segment \"CODE\"\n.org $8000\n");
    fprintf(f, "reset:\n        sei\n        cld\n        ldx #$ff\n        txs\n");
    fprintf(f, "        lda #0\n        sta WORLD\n        sta SCROLL_X\n        sta SCROLL_Y\n");
    fprintf(f, "        lda #1\n        sta PPUCTRL\n");
    fprintf(f, "main:\n        lda PPUSTATUS\n        and #$80\n        beq main\n");
    fprintf(f, "        lda PAD0\n        sta $00FE\n        jmp main\n");
    fprintf(f, ".segment \"PLAY\"\n.org $8100\n");
    fprintf(f, "; present mask + spawn filled by exporter\n");
    if (w) {
        int i, n = 0;
        for (i = 0; i < w->screen_count; i++) {
            if (w->screens[i].present) {
                n++;
            }
        }
        fprintf(f, "; %d present screens in cart MAP\n", n);
    }
    fprintf(f, ".segment \"VECTORS\"\n.org $FFFA\n");
    fprintf(f, "        .word main\n        .word reset\n        .word main\n");
    fclose(f);
    return 0;
}

static int append_pal_plane(Buf *b, R01PalRow plane[R01_PAL_ROWS][R01_PALS_PER_ROW]) {
    int row, pal, c, o = 0;
    uint8_t tmp[R01_PAL_PLANE_BYTES];
    for (row = 0; row < R01_PAL_ROWS; row++) {
        for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
            for (c = 0; c < R01_PAL_COLORS; c++) {
                tmp[o++] = plane[row][pal].idx[c] & 63u;
            }
        }
    }
    return buf_append(b, tmp, sizeof(tmp));
}

static void fill_solid_tile(uint8_t tile[R01_TILE_BYTES], uint8_t color) {
    int row;
    uint8_t p0 = (color & 1u) ? 0xFFu : 0;
    uint8_t p1 = (color & 2u) ? 0xFFu : 0;
    memset(tile, 0, R01_TILE_BYTES);
    for (row = 0; row < 8; row++) {
        tile[row] = p0;
        tile[row + 8] = p1;
    }
}

static int build_world_blob(Buf *blob, const R01World *w) {
    uint8_t hdr[WORLD_HDR_SIZE];
    uint8_t dir[R01_MAX_SCREENS * SCREEN_DIR_ENT];
    size_t off_chr, off_sdir, off_spay;
    int si, bi, present_n = 0;
    uint32_t payload_base;

    memset(hdr, 0, sizeof(hdr));
    memset(dir, 0, sizeof(dir));
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            present_n++;
        }
    }
    if (present_n > 255) {
        present_n = 255;
    }

    off_chr = WORLD_HDR_SIZE;
    off_sdir = off_chr + (size_t)R01_BG_BANKS * R01_CHR_BANK_BYTES + (size_t)R01_SPR_BANKS * R01_CHR_BANK_BYTES;
    off_spay = off_sdir + (size_t)present_n * SCREEN_DIR_ENT;
    payload_base = (uint32_t)off_spay;

    put_u8(hdr + 0, (uint8_t)R01_START_COL);
    put_u8(hdr + 1, (uint8_t)R01_START_ROW);
    put_u8(hdr + 2, (uint8_t)(w->default_bg_bank & 3));
    put_u8(hdr + 3, 0); /* default_spr_bank */
    put_u8(hdr + 4, (uint8_t)(w->default_pal_row & 7));
    put_u8(hdr + 5, (uint8_t)present_n);
    put_u8(hdr + 6, 0);
    put_u24(hdr + 8, (uint32_t)off_chr);
    put_u24(hdr + 11, (uint32_t)off_sdir);
    put_u24(hdr + 14, 0);
    /* hdr+17..31 reserved */

    if (buf_append(blob, hdr, WORLD_HDR_SIZE) != 0) {
        return -1;
    }
    for (bi = 0; bi < R01_BG_BANKS; bi++) {
        uint8_t bank[R01_CHR_BANK_BYTES];
        size_t n = (size_t)w->bg_banks[bi].tile_count * R01_TILE_BYTES;
        memset(bank, 0, sizeof(bank));
        if (n > sizeof(bank)) {
            n = sizeof(bank);
        }
        memcpy(bank, w->bg_banks[bi].chr, n);
        if (buf_append(blob, bank, sizeof(bank)) != 0) {
            return -1;
        }
    }
    for (bi = 0; bi < R01_SPR_BANKS; bi++) {
        uint8_t bank[R01_CHR_BANK_BYTES];
        memset(bank, 0, sizeof(bank));
        if (bi == 0) {
            /* Tile 1 = solid color-1 player (matches Studio Play sprite pal idx 1). */
            fill_solid_tile(bank + R01_TILE_BYTES, 1);
        }
        if (buf_append(blob, bank, sizeof(bank)) != 0) {
            return -1;
        }
    }
    {
        int di = 0;
        for (si = 0; si < w->screen_count; si++) {
            const R01Screen *s = &w->screens[si];
            uint8_t *e;
            if (!s->present) {
                continue;
            }
            e = dir + (size_t)di * SCREEN_DIR_ENT;
            put_u8(e + 0, (uint8_t)s->col);
            put_u8(e + 1, (uint8_t)s->row);
            put_u8(e + 2, 0);
            put_u8(e + 3, 0);
            put_u24(e + 4, payload_base + (uint32_t)di * SCREEN_PAYLOAD);
            put_u24(e + 7, 0);
            di++;
        }
        if (buf_append(blob, dir, (size_t)present_n * SCREEN_DIR_ENT) != 0) {
            return -1;
        }
        for (si = 0; si < w->screen_count; si++) {
            const R01Screen *s = &w->screens[si];
            if (!s->present) {
                continue;
            }
            if (buf_append(blob, s->tiles, R01_TILES_PER_SCREEN) != 0 ||
                buf_append(blob, s->attrs, R01_ATTRS_PER_SCREEN) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int r01_cart_build(const R01Project *p, uint8_t **out, size_t *out_len, char *err_buf, size_t err_cap) {
    Buf cart = {0};
    R01Project *work;
    uint8_t hdr[HDR_SIZE];
    uint8_t ptrs[PTR_TABLE_SIZE];
    uint8_t wtable[WORLD_TABLE_SIZE];
    uint8_t prg[R01_PRG_BYTES];
    uint32_t off_prg, off_pal_bg, off_pal_spr, off_wtable;
    Buf world_blob = {0};

    if (!p || !out || !out_len) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    *out = NULL;
    *out_len = 0;
    work = (R01Project *)malloc(sizeof(R01Project));
    if (!work) {
        set_err(err_buf, err_cap, "oom");
        return -1;
    }
    memcpy(work, p, sizeof(*work));
    if (r01_chr_pack_world_bank0(&work->worlds[0]) != R01_CHR_OK) {
        free(work);
        set_err(err_buf, err_cap, "chr pack failed");
        return -1;
    }
    if (build_world_blob(&world_blob, &work->worlds[0]) != 0) {
        free(work);
        free(world_blob.data);
        set_err(err_buf, err_cap, "world blob failed");
        return -1;
    }

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "retr01", 6);
    hdr[6] = R01_CART_FORMAT_VER;
    hdr[7] = 1;
    r01_prg_fill_phase1(prg, &work->worlds[0]);

    off_pal_bg = HDR_SIZE + PTR_TABLE_SIZE;
    off_pal_spr = off_pal_bg + R01_PAL_PLANE_BYTES;
    off_prg = off_pal_spr + R01_PAL_PLANE_BYTES;
    off_wtable = off_prg + R01_PRG_BYTES;

    memset(ptrs, 0, sizeof(ptrs));
    put_u24(ptrs + 0, off_prg);
    put_u24(ptrs + 3, R01_PRG_BYTES);
    put_u24(ptrs + 6, off_pal_bg);
    put_u24(ptrs + 9, R01_PAL_PLANE_BYTES);
    put_u24(ptrs + 12, off_pal_spr);
    put_u24(ptrs + 15, R01_PAL_PLANE_BYTES);
    put_u24(ptrs + 18, off_wtable);
    put_u24(ptrs + 21, WORLD_TABLE_SIZE);

    if (buf_append(&cart, hdr, HDR_SIZE) != 0 || buf_append(&cart, ptrs, PTR_TABLE_SIZE) != 0 ||
        append_pal_plane(&cart, work->global_pal_bg) != 0 ||
        append_pal_plane(&cart, work->global_pal_spr) != 0 || buf_append(&cart, prg, R01_PRG_BYTES) != 0) {
        goto oom;
    }

    memset(wtable, 0, sizeof(wtable));
    put_u8(wtable + 0, 1);
    put_u24(wtable + 2, off_wtable + WORLD_TABLE_SIZE);
    put_u24(wtable + 5, (uint32_t)world_blob.len);

    if (buf_append(&cart, wtable, WORLD_TABLE_SIZE) != 0 || buf_append(&cart, world_blob.data, world_blob.len) != 0) {
        goto oom;
    }
    free(world_blob.data);
    free(work);
    *out = cart.data;
    *out_len = cart.len;
    return 0;

oom:
    free(work);
    free(world_blob.data);
    free(cart.data);
    set_err(err_buf, err_cap, "oom");
    return -1;
}

int r01_cart_write(const R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    uint8_t *img = NULL;
    size_t len = 0;
    FILE *f;
    if (r01_cart_build(p, &img, &len, err_buf, err_cap) != 0) {
        return -1;
    }
    f = fopen(path, "wb");
    if (!f) {
        free(img);
        set_err(err_buf, err_cap, "cannot write cart");
        return -1;
    }
    if (fwrite(img, 1, len, f) != len) {
        fclose(f);
        free(img);
        set_err(err_buf, err_cap, "cart write failed");
        return -1;
    }
    fclose(f);
    free(img);
    return 0;
}

int r01_cart_write_flash(const R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    uint8_t *img = NULL;
    size_t len = 0;
    Buf flash = {0};
    FILE *f;
    if (r01_cart_build(p, &img, &len, err_buf, err_cap) != 0) {
        return -1;
    }
    if (len > R01_CART_FLASH_BYTES) {
        free(img);
        set_err(err_buf, err_cap, "cart too large");
        return -1;
    }
    if (buf_append(&flash, img, len) != 0 || buf_pad(&flash, R01_CART_FLASH_BYTES, 0xFF) != 0) {
        free(img);
        free(flash.data);
        set_err(err_buf, err_cap, "oom");
        return -1;
    }
    free(img);
    f = fopen(path, "wb");
    if (!f) {
        free(flash.data);
        set_err(err_buf, err_cap, "cannot write flash");
        return -1;
    }
    if (fwrite(flash.data, 1, flash.len, f) != flash.len) {
        fclose(f);
        free(flash.data);
        set_err(err_buf, err_cap, "flash write failed");
        return -1;
    }
    fclose(f);
    free(flash.data);
    return 0;
}

int r01_export_bundle(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    if (!p || !path_stem) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    snprintf(path, sizeof(path), "%s.retr01", path_stem);
    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s.retr01", path_stem);
    if (r01_cart_write(p, path, err_buf, err_cap) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s_prom.bin", path_stem);
    if (r01_prom_write(path, err_buf, err_cap) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s_boot.s", path_stem);
    if (r01_prg_write_asm(p, path, err_buf, err_cap) != 0) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s_flash.bin", path_stem);
    if (r01_cart_write_flash(p, path, err_buf, err_cap) != 0) {
        return -1;
    }
    return 0;
}
