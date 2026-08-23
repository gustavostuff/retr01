#include "retr01_emu/cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDR_SIZE 16
#define PTR_TABLE_SIZE 24
#define WORLD_SLOT_SIZE 8
#define WORLD_HDR_SIZE 32

static void set_err(char *err, size_t err_cap, const char *msg) {
    if (err && err_cap > 0) {
        snprintf(err, err_cap, "%s", msg ? msg : "error");
    }
}

static uint32_t get_u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

void r01e_cart_free(R01eCart *c) {
    if (!c) {
        return;
    }
    free(c->data);
    memset(c, 0, sizeof(*c));
}

const uint8_t *r01e_cart_ptr(const R01eCart *c, uint32_t abs_off, size_t need) {
    if (!c || !c->data) {
        return NULL;
    }
    if ((size_t)abs_off + need > c->len) {
        return NULL;
    }
    return c->data + abs_off;
}

uint8_t r01e_cart_read(const R01eCart *c, uint32_t abs_off) {
    const uint8_t *p = r01e_cart_ptr(c, abs_off, 1);
    return p ? *p : 0xFF;
}

const uint8_t *r01e_cart_prg(const R01eCart *c) {
    if (!c || c->len_prg == 0) {
        return NULL;
    }
    return r01e_cart_ptr(c, c->off_prg, c->len_prg);
}

int r01e_cart_load_mem(R01eCart *out, const uint8_t *img, size_t len, char *err, size_t err_cap) {
    const uint8_t *ptrs;
    uint8_t *copy;
    if (!out || !img) {
        set_err(err, err_cap, "bad args");
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (len < HDR_SIZE + PTR_TABLE_SIZE) {
        set_err(err, err_cap, "cart too small");
        return -1;
    }
    if (memcmp(img, R01E_CART_MAGIC, 6) != 0) {
        set_err(err, err_cap, "bad magic");
        return -1;
    }
    copy = (uint8_t *)malloc(len);
    if (!copy) {
        set_err(err, err_cap, "oom");
        return -1;
    }
    memcpy(copy, img, len);
    out->data = copy;
    out->len = len;
    out->format_ver = img[6];
    out->world_count = img[7];
    out->flags = img[8];
    ptrs = img + HDR_SIZE;
    out->off_prg = get_u24(ptrs + 0);
    out->len_prg = get_u24(ptrs + 3);
    out->off_pal_bg = get_u24(ptrs + 6);
    out->off_pal_spr = get_u24(ptrs + 12);
    out->off_world_table = get_u24(ptrs + 18);
    if (out->len_prg == 0 || !r01e_cart_ptr(out, out->off_prg, out->len_prg > R01E_PRG_BYTES ? R01E_PRG_BYTES : out->len_prg)) {
        r01e_cart_free(out);
        set_err(err, err_cap, "bad PRG pointer");
        return -1;
    }
    return 0;
}

int r01e_cart_load_path(R01eCart *out, const char *path, char *err, size_t err_cap) {
    FILE *f;
    long sz;
    uint8_t *buf;
    size_t n;
    int rc;
    if (!path) {
        set_err(err, err_cap, "bad path");
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        set_err(err, err_cap, "cannot open cart");
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        set_err(err, err_cap, "cart seek failed");
        return -1;
    }
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        set_err(err, err_cap, "oom");
        return -1;
    }
    n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        set_err(err, err_cap, "cart read failed");
        return -1;
    }
    rc = r01e_cart_load_mem(out, buf, n, err, err_cap);
    free(buf);
    return rc;
}

int r01e_cart_world(const R01eCart *c, int index, R01eWorldView *out) {
    const uint8_t *slot;
    const uint8_t *hdr;
    uint32_t base, wlen;
    if (!c || !out || index < 0 || index >= R01E_MAX_WORLDS) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    slot = r01e_cart_ptr(c, c->off_world_table + (uint32_t)index * WORLD_SLOT_SIZE, WORLD_SLOT_SIZE);
    if (!slot || slot[0] == 0) {
        return -1;
    }
    base = get_u24(slot + 2);
    wlen = get_u24(slot + 5);
    hdr = r01e_cart_ptr(c, base, WORLD_HDR_SIZE);
    if (!hdr) {
        return -1;
    }
    out->present = 1;
    out->base = base;
    out->len = wlen;
    out->start_col = hdr[0];
    out->start_row = hdr[1];
    out->default_bg_bank = hdr[2] & 3u;
    out->default_pal_row = hdr[4] & 3u;
    out->screen_count = hdr[5];
    out->parallax_count = hdr[6];
    out->off_chr = get_u24(hdr + 8);
    out->off_screen_dir = get_u24(hdr + 11);
    out->off_parallax_dir = get_u24(hdr + 14);
    out->off_wpal_bg = get_u24(hdr + 17);
    out->off_wpal_spr = get_u24(hdr + 20);
    return 0;
}
