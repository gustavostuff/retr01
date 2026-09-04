#include "retr01_emu/cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDR_SIZE 16
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
    if (len < HDR_SIZE + R01E_CART_PTR_TABLE_BYTES) {
        set_err(err, err_cap, "cart too small");
        return -1;
    }
    if (memcmp(img, R01E_CART_MAGIC, 6) != 0) {
        set_err(err, err_cap, "bad magic");
        return -1;
    }
    out->format_ver = img[6];
    if (out->format_ver != R01E_CART_FORMAT_VER) {
        set_err(err, err_cap, "unsupported cart format_ver");
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
    out->len_world_table = get_u24(ptrs + 21);
    out->off_other = get_u24(ptrs + 24);
    out->len_other = get_u24(ptrs + 27);
    /* Legacy ASCII credits pointers ignored (reserved 0). */
    out->off_credits = 0;
    out->len_credits = 0;
    (void)get_u24(ptrs + 30);
    (void)get_u24(ptrs + 33);
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
    out->start_col = (uint8_t)R01E_CELL_COL(hdr[0]);
    out->start_row = (uint8_t)R01E_CELL_ROW(hdr[0]);
    out->default_bg_bank = hdr[2] & 3u;
    out->default_pal_row = hdr[4] & 7u;
    out->screen_count = hdr[5];
    if (out->screen_count > R01E_MAX_PRESENT_SCREENS) {
        return -1;
    }
    out->bg0_count = hdr[6];
    if (out->bg0_count > R01E_PARALLAX_MAX) {
        return -1;
    }
    /* hdr[3]: was default_spr_bank (always 0). Now BG0 cols|rows<<4 when BG0 present. */
    out->bg0_cols = (uint8_t)(hdr[3] & 0x0fu);
    out->bg0_rows = (uint8_t)((hdr[3] >> 4) & 0x0fu);
    out->off_chr = get_u24(hdr + 8);
    out->off_screen_dir = get_u24(hdr + 11);
    out->off_bg0_dir = get_u24(hdr + 14);
    out->entity_type_count = hdr[R01E_CART_WHDR_TYPE_COUNT];
    out->entity_inst_count = hdr[R01E_CART_WHDR_INST_COUNT];
    out->off_entity_types = get_u24(hdr + R01E_CART_WHDR_OFF_TYPES);
    out->off_entity_insts = get_u24(hdr + R01E_CART_WHDR_OFF_INSTS);
    out->player_entity = hdr[R01E_CART_WHDR_PLAYER_ENTITY];
    out->player_hit_x = hdr[R01E_CART_WHDR_PLAYER_HIT_X];
    out->player_hit_y = hdr[R01E_CART_WHDR_PLAYER_HIT_Y];
    out->player_hit_w = hdr[R01E_CART_WHDR_PLAYER_HIT_W];
    out->player_hit_h = hdr[R01E_CART_WHDR_PLAYER_HIT_H];
    out->world_flags = hdr[R01E_CART_WHDR_FLAGS];
    out->cam_deadzone_x = hdr[R01E_CART_WHDR_CAM_DEADZONE_X];
    out->cam_deadzone_y = hdr[R01E_CART_WHDR_CAM_DEADZONE_Y];
    out->off_player_anim = out->off_entity_insts + (uint32_t)out->entity_inst_count * R01E_CART_INSTANCE_SIZE;
    out->has_player_anim =
        (out->world_flags & R01E_CART_WHDR_FLAG_PLAYER_ANIM) != 0 &&
        out->player_entity != R01E_CART_PLAYER_ENTITY_NONE;
    if (out->player_hit_w < 1) {
        out->player_hit_w = 8;
    }
    if (out->player_hit_h < 1) {
        out->player_hit_h = 8;
    }
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
        if (R01E_CELL_COL(e[0]) == col && R01E_CELL_ROW(e[0]) == row) {
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
        if (R01E_CELL_COL(e[0]) == col && R01E_CELL_ROW(e[0]) == row) {
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

const uint8_t *r01e_cart_other_raw(const R01eCart *c, int id, size_t *out_len, int *out_flags) {
    const uint8_t *blob;
    uint8_t count;
    int i;

    if (out_len) {
        *out_len = 0;
    }
    if (out_flags) {
        *out_flags = 0;
    }
    if (!c || c->format_ver != R01E_CART_FORMAT_VER || c->len_other == 0 || id < 0 || id >= R01E_CART_OTHER_MAX) {
        return NULL;
    }
    blob = r01e_cart_ptr(c, c->off_other, c->len_other);
    if (!blob) {
        return NULL;
    }
    count = blob[0];
    if (count == 0 || count > R01E_CART_OTHER_MAX) {
        return NULL;
    }
    for (i = 0; i < (int)count; i++) {
        const uint8_t *e = blob + R01E_CART_OTHER_HDR_BYTES + (size_t)i * R01E_CART_OTHER_DIR_BYTES;
        if ((int)e[R01E_CART_OTHER_DIR_ID] == id) {
            uint16_t plen = (uint16_t)e[R01E_CART_OTHER_DIR_LEN] | ((uint16_t)e[R01E_CART_OTHER_DIR_LEN + 1] << 8);
            uint32_t rel = get_u24(e + R01E_CART_OTHER_DIR_OFF);
            if (plen == 0) {
                plen = (uint16_t)R01E_SCREEN_PAYLOAD; /* legacy raw without len */
            }
            if (out_len) {
                *out_len = plen;
            }
            if (out_flags) {
                *out_flags = (int)e[R01E_CART_OTHER_DIR_FLAGS];
            }
            return r01e_cart_ptr(c, c->off_other + rel, plen);
        }
    }
    return NULL;
}

static int rle_decode_480(const uint8_t *in, size_t in_len, uint8_t out[R01E_SCREEN_PAYLOAD]) {
    size_t ip = 0;
    size_t op = 0;
    if (!in || !out) {
        return -1;
    }
    while (op < R01E_SCREEN_PAYLOAD) {
        uint8_t cmd;
        size_t n;
        if (ip >= in_len) {
            return -1;
        }
        cmd = in[ip++];
        if (cmd & 0x80u) {
            n = (size_t)(cmd & 0x7Fu) + 1u;
            if (ip >= in_len || op + n > R01E_SCREEN_PAYLOAD) {
                return -1;
            }
            memset(out + op, in[ip++], n);
            op += n;
        } else {
            n = (size_t)cmd + 1u;
            if (ip + n > in_len || op + n > R01E_SCREEN_PAYLOAD) {
                return -1;
            }
            memcpy(out + op, in + ip, n);
            ip += n;
            op += n;
        }
    }
    return (op == R01E_SCREEN_PAYLOAD) ? 0 : -1;
}

int r01e_cart_other_decode(const R01eCart *c, int id, uint8_t out[R01E_SCREEN_PAYLOAD]) {
    size_t len = 0;
    int flags = 0;
    const uint8_t *raw = r01e_cart_other_raw(c, id, &len, &flags);
    if (!raw || !out || len == 0) {
        return -1;
    }
    if (flags & R01E_CART_OTHER_FLAG_RLE) {
        return rle_decode_480(raw, len, out);
    }
    if (len < R01E_SCREEN_PAYLOAD) {
        return -1;
    }
    memcpy(out, raw, R01E_SCREEN_PAYLOAD);
    return 0;
}

const uint8_t *r01e_cart_other_payload(const R01eCart *c, int id) {
    size_t len = 0;
    int flags = 0;
    const uint8_t *raw = r01e_cart_other_raw(c, id, &len, &flags);
    if (!raw || (flags & R01E_CART_OTHER_FLAG_RLE) || len < R01E_SCREEN_PAYLOAD) {
        return NULL;
    }
    return raw;
}

int r01e_cart_aabb_ok(const R01eCart *c, int world, int px, int py, int bw, int bh) {
    int x1, y1, c0, c1, r0, r1, col, row;
    if (!c || px < 0 || py < 0 || bw < 1 || bh < 1) {
        return 0;
    }
    x1 = px + bw - 1;
    y1 = py + bh - 1;
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

int r01e_cart_player_aabb_ok(const R01eCart *c, int world, int px, int py) {
    return r01e_cart_aabb_ok(c, world, px, py, 8, 8);
}
