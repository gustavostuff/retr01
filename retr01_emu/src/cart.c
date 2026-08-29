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
    if (out->world_count > R01E_MAX_WORLDS) {
        r01e_cart_free(out);
        set_err(err, err_cap, "world_count too large");
        return -1;
    }
    out->flags = img[8];
    ptrs = img + HDR_SIZE;
    out->off_prg = get_u24(ptrs + 0);
    out->len_prg = get_u24(ptrs + 3);
    out->off_pal_bg = get_u24(ptrs + 6);
    out->len_pal_bg = get_u24(ptrs + 9);
    out->off_pal_spr = get_u24(ptrs + 12);
    out->len_pal_spr = get_u24(ptrs + 15);
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
    out->default_pal_row = hdr[4] & 7u;
    out->screen_count = hdr[5];
    if (out->screen_count > R01E_MAX_PRESENT_SCREENS) {
        return -1;
    }
    out->parallax_count = hdr[6];
    out->off_chr = get_u24(hdr + 8);
    out->off_screen_dir = get_u24(hdr + 11);
    out->off_parallax_dir = get_u24(hdr + 14);
    out->entity_type_count = hdr[R01E_CART_WHDR_TYPE_COUNT];
    out->entity_inst_count = hdr[R01E_CART_WHDR_INST_COUNT];
    out->off_entity_types = get_u24(hdr + R01E_CART_WHDR_OFF_TYPES);
    out->off_entity_insts = get_u24(hdr + R01E_CART_WHDR_OFF_INSTS);
    return 0;
}

int r01e_cart_has_screen(const R01eCart *c, int world, int col, int row) {
    R01eWorldView wv;
    const uint8_t *dir;
    int si;

    if (r01e_cart_world(c, world, &wv) != 0) {
        return 0;
    }
    dir = r01e_cart_ptr(c, wv.base + wv.off_screen_dir, (size_t)wv.screen_count * 12u);
    if (!dir) {
        return 0;
    }
    for (si = 0; si < wv.screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        if ((int)e[0] == col && (int)e[1] == row) {
            return 1;
        }
    }
    return 0;
}

static int cart_screen_payload(const R01eCart *c, int world, int col, int row, uint32_t *out_off) {
    R01eWorldView wv;
    const uint8_t *dir;
    int si;

    if (r01e_cart_world(c, world, &wv) != 0) {
        return 0;
    }
    dir = r01e_cart_ptr(c, wv.base + wv.off_screen_dir, (size_t)wv.screen_count * 12u);
    if (!dir) {
        return 0;
    }
    for (si = 0; si < wv.screen_count; si++) {
        const uint8_t *e = dir + (size_t)si * 12u;
        if ((int)e[0] == col && (int)e[1] == row) {
            if (out_off) {
                *out_off = wv.base + get_u24(e + 4);
            }
            return 1;
        }
    }
    return 0;
}

int r01e_cart_attr_at(const R01eCart *c, int world, int wx, int wy, uint8_t *out_attr) {
    int col, row, lx, ly, tx, ty, cell;
    uint32_t pay;
    const uint8_t *attrs;

    if (!c || wx < 0 || wy < 0) {
        return -1;
    }
    col = wx / R01E_SCREEN_PX_W;
    row = wy / R01E_SCREEN_PX_H;
    if (!cart_screen_payload(c, world, col, row, &pay)) {
        return -1;
    }
    lx = wx % R01E_SCREEN_PX_W;
    ly = wy % R01E_SCREEN_PX_H;
    tx = lx / 8;
    ty = ly / 8;
    cell = ty * R01E_SCREEN_TILES_X + tx;
    attrs = r01e_cart_ptr(c, pay + R01E_TILES_PER_SCREEN + (uint32_t)cell, 1);
    if (!attrs) {
        return -1;
    }
    if (out_attr) {
        *out_attr = attrs[0];
    }
    return 0;
}

int r01e_cart_solid_at(const R01eCart *c, int world, int wx, int wy) {
    uint8_t attr;
    if (r01e_cart_attr_at(c, world, wx, wy, &attr) != 0) {
        return 0;
    }
    return (attr & R01E_ATTR_SOLID) != 0;
}

int r01e_cart_player_aabb_ok(const R01eCart *c, int world, int px, int py) {
    int x1, y1, c0, c1, r0, r1, col, row;
    if (!c || px < 0 || py < 0) {
        return 0;
    }
    x1 = px + 7;
    y1 = py + 7;
    c0 = px / R01E_SCREEN_PX_W;
    c1 = x1 / R01E_SCREEN_PX_W;
    r0 = py / R01E_SCREEN_PX_H;
    r1 = y1 / R01E_SCREEN_PX_H;
    for (col = c0; col <= c1; col++) {
        for (row = r0; row <= r1; row++) {
            if (!r01e_cart_has_screen(c, world, col, row)) {
                return 0;
            }
        }
    }
    if (r01e_cart_solid_at(c, world, px, py) || r01e_cart_solid_at(c, world, x1, py) ||
        r01e_cart_solid_at(c, world, px, y1) || r01e_cart_solid_at(c, world, x1, y1)) {
        return 0;
    }
    return 1;
}
