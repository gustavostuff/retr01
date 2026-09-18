#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/play.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/export_codegen.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"
#include "r01_custom_logic_scan.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDR_SIZE 16
#define PTR_TABLE_SIZE ((int)R01_CART_PTR_TABLE_BYTES)
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

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void pack_hitbox_rel(int origin_x, int origin_y, int box_x, int box_y, int box_w, int box_h, uint8_t *hx,
                            uint8_t *hy, uint8_t *hw, uint8_t *hh) {
    int x = box_x - origin_x;
    int y = box_y - origin_y;
    int w = box_w > 0 ? box_w : R01_PLAY_PLAYER_W;
    int h = box_h > 0 ? box_h : R01_PLAY_PLAYER_H;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > 255) {
        x = 255;
    }
    if (y > 255) {
        y = 255;
    }
    if (w > 255) {
        w = 255;
    }
    if (h > 255) {
        h = 255;
    }
    if (hx) {
        *hx = (uint8_t)x;
    }
    if (hy) {
        *hy = (uint8_t)y;
    }
    if (hw) {
        *hw = (uint8_t)w;
    }
    if (hh) {
        *hh = (uint8_t)h;
    }
}

/* Byte-RLE over a 480 B screen payload (general_docs/graphics). Returns compressed length, or 0 on fail. */
static size_t rle_encode_480(const uint8_t in[R01_CART_SCREEN_PAYLOAD], uint8_t *out, size_t out_cap) {
    size_t ip = 0;
    size_t op = 0;

    if (!in || !out) {
        return 0;
    }
    while (ip < R01_CART_SCREEN_PAYLOAD) {
        size_t run = 1;
        while (ip + run < R01_CART_SCREEN_PAYLOAD && in[ip + run] == in[ip] && run < 128) {
            run++;
        }
        if (run >= 2) {
            if (op + 2 > out_cap) {
                return 0;
            }
            out[op++] = (uint8_t)(0x80u | (uint8_t)(run - 1u));
            out[op++] = in[ip];
            ip += run;
        } else {
            size_t lit = 0;
            while (ip + lit < R01_CART_SCREEN_PAYLOAD && lit < 128) {
                if (ip + lit + 1 < R01_CART_SCREEN_PAYLOAD && in[ip + lit] == in[ip + lit + 1]) {
                    size_t peek = 1;
                    while (ip + lit + peek < R01_CART_SCREEN_PAYLOAD &&
                           in[ip + lit + peek] == in[ip + lit] && peek < 128) {
                        peek++;
                    }
                    if (peek >= 2) {
                        break;
                    }
                }
                lit++;
            }
            if (lit < 1) {
                lit = 1;
            }
            if (op + 1 + lit > out_cap) {
                return 0;
            }
            out[op++] = (uint8_t)(lit - 1u);
            memcpy(out + op, in + ip, lit);
            op += lit;
            ip += lit;
        }
    }
    return op;
}

static int build_other_blob(Buf *b, const R01Project *p) {
    uint8_t hdr[R01_CART_OTHER_HDR_BYTES];
    uint8_t raw[R01_CART_SCREEN_PAYLOAD];
    uint8_t compressed[R01_CART_SCREEN_PAYLOAD + 64];
    int ids[R01_CART_OTHER_MAX];
    int count = 0;
    uint32_t payload_base;
    size_t payload_bytes = 0;
    int i;
    int di;

    if (!b || !p) {
        return -1;
    }
    for (i = 0; i < R01_CART_OTHER_MAX; i++) {
        int force = (i == R01_CART_OTHER_TITLE || i == R01_CART_OTHER_INTER);
        if (force || p->other_screens[i].present) {
            ids[count++] = i;
        }
    }
    if (count < 2 || count > R01_CART_OTHER_MAX) {
        return -1;
    }
    {
        int credits_n = 0;
        for (i = 0; i < count; i++) {
            if (ids[i] >= R01_CART_OTHER_CREDITS_FIRST) {
                credits_n++;
            }
        }
        if (credits_n < R01_CART_CREDITS_MIN || credits_n > R01_CART_CREDITS_MAX) {
            return -1;
        }
    }

    payload_base = (uint32_t)R01_CART_OTHER_HDR_BYTES + (uint32_t)count * R01_CART_OTHER_DIR_BYTES;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = (uint8_t)count;
    if (buf_append(b, hdr, sizeof(hdr)) != 0) {
        return -1;
    }
    /* Placeholder dir; patched as payloads are appended. */
    {
        uint8_t zdir[R01_CART_OTHER_MAX * R01_CART_OTHER_DIR_BYTES];
        memset(zdir, 0, sizeof(zdir));
        if (buf_append(b, zdir, (size_t)count * R01_CART_OTHER_DIR_BYTES) != 0) {
            return -1;
        }
    }

    for (di = 0; di < count; di++) {
        const R01OtherScreen *os = &p->other_screens[ids[di]];
        uint8_t *e = b->data + R01_CART_OTHER_HDR_BYTES + (size_t)di * R01_CART_OTHER_DIR_BYTES;
        size_t clen;
        uint8_t flags = 0;
        const uint8_t *pay;
        size_t pay_len;
        uint32_t rel;

        memcpy(raw, os->tiles, R01_TILES_PER_SCREEN);
        memcpy(raw + R01_TILES_PER_SCREEN, os->attrs, R01_ATTRS_PER_SCREEN);
        clen = rle_encode_480(raw, compressed, sizeof(compressed));
        if (clen > 0 && clen < R01_CART_SCREEN_PAYLOAD) {
            flags = R01_CART_OTHER_FLAG_RLE;
            pay = compressed;
            pay_len = clen;
        } else {
            pay = raw;
            pay_len = R01_CART_SCREEN_PAYLOAD;
        }
        rel = payload_base + (uint32_t)payload_bytes;
        e[R01_CART_OTHER_DIR_ID] = (uint8_t)ids[di];
        e[R01_CART_OTHER_DIR_FLAGS] = flags;
        put_u16(e + R01_CART_OTHER_DIR_LEN, (uint16_t)pay_len);
        put_u24(e + R01_CART_OTHER_DIR_OFF_PAYLOAD, rel);
        if (buf_append(b, pay, pay_len) != 0) {
            return -1;
        }
        payload_bytes += pay_len;
    }
    if (b->len > R01_CART_OTHER_BYTES_MAX) {
        return -1;
    }
    return 0;
}

static uint8_t pack_oam_attr(int bank, int pal, int flip_h, int flip_v) {
    int b = r01_is_global_spr_bank(bank) ? r01_global_spr_index(bank) : bank;
    uint8_t a = (uint8_t)((b & 3) | ((pal & 3) << 2));
    if (flip_h) {
        a |= R01_ATTR_FLIP_H;
    }
    if (flip_v) {
        a |= R01_ATTR_FLIP_V;
    }
    return a;
}

static int append_player_anim_blob(Buf *blob, const R01World *w, int player_type, int remap_b0_tile1) {
    const R01EntityType *ent;
    uint8_t magic[2] = {R01_CART_PLAYER_ANIM_MAGIC0, R01_CART_PLAYER_ANIM_MAGIC1};
    int si;

    if (!blob || !w || player_type < 0 || player_type >= w->entity_count) {
        return -1;
    }
    ent = &w->entities[player_type];
    if (ent->state_count < 1) {
        return -1;
    }
    if (buf_append(blob, magic, sizeof(magic)) != 0) {
        return -1;
    }
    {
        uint8_t sc = (uint8_t)(ent->state_count > R01_ENTITY_STATES_MAX ? R01_ENTITY_STATES_MAX : ent->state_count);
        if (buf_append(blob, &sc, 1) != 0) {
            return -1;
        }
    }
    for (si = 0; si < ent->state_count && si < R01_ENTITY_STATES_MAX; si++) {
        const R01EntityState *st = &ent->states[si];
        uint8_t drawable_count;
        int fi;
        int drawable = 0;
        for (fi = 0; fi < st->frame_count; fi++) {
            if (st->frames[fi].part_count > 0) {
                drawable++;
            }
        }
        if (drawable > 255) {
            drawable = 255;
        }
        drawable_count = (uint8_t)drawable;
        if (buf_append(blob, &drawable_count, 1) != 0) {
            return -1;
        }
        for (fi = 0; fi < st->frame_count; fi++) {
            const R01EntityFrame *fr = &st->frames[fi];
            int pi;
            uint8_t pc;
            uint8_t fh[8];
            if (fr->part_count < 1) {
                continue;
            }
            pc = (uint8_t)(fr->part_count > R01_CART_ENTITY_PARTS_MAX ? R01_CART_ENTITY_PARTS_MAX : fr->part_count);
            fh[0] = (uint8_t)fr->origin_x;
            fh[1] = (uint8_t)fr->origin_y;
            fh[2] = (uint8_t)st->hitbox_x;
            fh[3] = (uint8_t)st->hitbox_y;
            fh[4] = (uint8_t)(st->hitbox_w > 0 ? st->hitbox_w : R01_PLAY_PLAYER_W);
            fh[5] = (uint8_t)(st->hitbox_h > 0 ? st->hitbox_h : R01_PLAY_PLAYER_H);
            fh[6] = pc;
            {
                int delay = fr->delay;
                if (delay < 1) {
                    delay = 1;
                }
                if (delay > 255) {
                    delay = 255;
                }
                fh[7] = (uint8_t)delay;
            }
            if (buf_append(blob, fh, sizeof(fh)) != 0) {
                return -1;
            }
            for (pi = 0; pi < (int)pc; pi++) {
                const R01EntityPart *pt = &fr->parts[pi];
                uint8_t part[4];
                int tile = pt->tile_id;
                if (remap_b0_tile1 >= 0 && pt->tile_id == R01_SPR_PLAYER_TILE_ID &&
                    (pt->bank == 0 || pt->bank == R01_GLOBAL_SPR_BANK_BASE)) {
                    tile = remap_b0_tile1;
                }
                part[0] = (uint8_t)tile;
                part[1] = pack_oam_attr(pt->bank, pt->pal, pt->flip_h, pt->flip_v);
                part[2] = (uint8_t)(int8_t)pt->dx;
                part[3] = (uint8_t)(int8_t)pt->dy;
                if (buf_append(blob, part, sizeof(part)) != 0) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

static size_t pack_entity_def(uint8_t *out, size_t cap, const R01EntityType *ent, int remap_b0_tile1) {
    uint8_t scratch[R01_CART_ENTITY_DEF_MAX];
    uint8_t *hdr;
    int sc;
    int si;
    size_t cursor;

    if (!out || !ent || ent->state_count < 1) {
        return 0;
    }
    memset(scratch, 0, sizeof(scratch));
    hdr = scratch;
    sc = ent->state_count;
    if (sc > R01_CART_ENTITY_STATES_MAX) {
        sc = R01_CART_ENTITY_STATES_MAX;
    }
    if (sc > R01_ENTITY_STATES_MAX) {
        sc = R01_ENTITY_STATES_MAX;
    }
    hdr[0] = 0; /* flags */
    hdr[1] = (uint8_t)sc;
    hdr[2] = 0; /* default_state */
    hdr[3] = 0; /* reserved0 */
    cursor = 12; /* header + state_off[4] */

    for (si = 0; si < sc; si++) {
        const R01EntityState *st = &ent->states[si];
        int fc;
        int fi;
        size_t state_base;
        uint8_t *sh;

        fc = st->frame_count;
        if (fc < 1) {
            continue;
        }
        if (fc > R01_CART_ENTITY_FRAMES_MAX) {
            fc = R01_CART_ENTITY_FRAMES_MAX;
        }
        if (fc > R01_ENTITY_FRAMES_MAX) {
            fc = R01_ENTITY_FRAMES_MAX;
        }
        if (cursor + 10u > sizeof(scratch)) {
            break;
        }
        state_base = cursor;
        put_u16(hdr + 4 + (size_t)si * 2u, (uint16_t)state_base);
        sh = scratch + state_base;
        sh[0] = (uint8_t)fc;
        sh[1] = 0; /* reserved1 */
        cursor = state_base + 10u;

        for (fi = 0; fi < fc; fi++) {
            const R01EntityFrame *fr = &st->frames[fi];
            int ox = fr->origin_x;
            int oy = fr->origin_y;
            int pc;
            int pi;
            size_t frame_base;
            uint8_t *fh;

            pc = fr->part_count;
            if (pc < 1) {
                put_u16(sh + 2 + (size_t)fi * 2u, 0);
                continue;
            }
            if (pc > R01_CART_ENTITY_PARTS_MAX) {
                pc = R01_CART_ENTITY_PARTS_MAX;
            }
            if (cursor + 6u + (size_t)pc * 4u > sizeof(scratch)) {
                put_u16(sh + 2 + (size_t)fi * 2u, 0);
                continue;
            }
            frame_base = cursor;
            put_u16(sh + 2 + (size_t)fi * 2u, (uint16_t)(frame_base - state_base));
            fh = scratch + frame_base;
            {
                int delay = fr->delay;
                if (delay < 1) {
                    delay = R01_CART_ENTITY_FRAME_DELAY_DEFAULT;
                }
                if (delay > 255) {
                    delay = 255;
                }
                fh[0] = (uint8_t)delay;
            }
            fh[1] = (uint8_t)pc;
            {
                const R01EntityFrame *href = r01_entity_state_hitbox_origin_frame(st);
                int hox = href ? href->origin_x : ox;
                int hoy = href ? href->origin_y : oy;
                pack_hitbox_rel(hox, hoy, st->hitbox_x, st->hitbox_y, st->hitbox_w, st->hitbox_h, &fh[2],
                                &fh[3], &fh[4], &fh[5]);
            }
            for (pi = 0; pi < pc; pi++) {
                const R01EntityPart *pt = &fr->parts[pi];
                uint8_t *sp = fh + 6 + pi * 4;
                int tile = pt->tile_id;
                int rx = pt->dx - ox;
                int ry = pt->dy - oy;
                if (remap_b0_tile1 >= 0 && pt->tile_id == R01_SPR_PLAYER_TILE_ID &&
                    (pt->bank == 0 || pt->bank == R01_GLOBAL_SPR_BANK_BASE)) {
                    tile = remap_b0_tile1;
                }
                if (rx < -128) {
                    rx = -128;
                }
                if (rx > 127) {
                    rx = 127;
                }
                if (ry < -128) {
                    ry = -128;
                }
                if (ry > 127) {
                    ry = 127;
                }
                sp[0] = (uint8_t)tile;
                sp[1] = (uint8_t)(int8_t)rx;
                sp[2] = (uint8_t)(int8_t)ry;
                sp[3] = pack_oam_attr(pt->bank, pt->pal, pt->flip_h, pt->flip_v);
            }
            cursor = frame_base + 6u + (size_t)pc * 4u;
        }
    }

    if (cursor < 12u || cursor > cap || cursor > sizeof(scratch)) {
        return 0;
    }
    memcpy(out, scratch, cursor);
    return cursor;
}

static int build_entity_catalog(Buf *catalog, const R01World *w, int type_n, int remap_b0_tile1) {
    size_t dir_bytes;
    int ti;

    if (!catalog || !w || type_n < 0) {
        return -1;
    }
    catalog->data = NULL;
    catalog->len = 0;
    catalog->cap = 0;
    if (type_n == 0) {
        return 0;
    }
    dir_bytes = (size_t)type_n * 2u;
    if (buf_pad(catalog, dir_bytes, 0) != 0) {
        return -1;
    }
    for (ti = 0; ti < type_n; ti++) {
        uint8_t def[R01_CART_ENTITY_DEF_MAX];
        size_t n = pack_entity_def(def, sizeof(def), &w->entities[ti], remap_b0_tile1);
        size_t off = catalog->len;
        if (n == 0) {
            put_u16(catalog->data + (size_t)ti * 2u, 0);
            continue;
        }
        if (off > 0xFFFFu || buf_append(catalog, def, n) != 0) {
            free(catalog->data);
            catalog->data = NULL;
            catalog->len = 0;
            catalog->cap = 0;
            return -1;
        }
        put_u16(catalog->data + (size_t)ti * 2u, (uint16_t)off);
    }
    return 0;
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
    fprintf(f, "; retr01 Phase 1 -- boot streams palette + start MAP, then VBlank pad poll.\n");
    fprintf(f, "; Gameplay: Studio play.c / emu cart runtime (marker R01P @ $80F0).\n");
    fprintf(f, "; Play table @ $8100: present[32], spawn_cell @ $8120, coll_dir @ $8122.\n");
    fprintf(f, "; play_pos_ok @ $8500 (PRG+$0500): solid shadow probe via ($20),Y.\n");
    fprintf(f, ".setcpu \"65C02\"\n");
    fprintf(f, "WORLD     = $7F30\n");
    fprintf(f, "SCROLL_X  = $7F02\n");
    fprintf(f, "SCROLL_Y  = $7F03\n");
    fprintf(f, "PPUCTRL   = $7F00\n");
    fprintf(f, "PPUSTATUS = $7F01\n");
    fprintf(f, "PPUCTRL_BOOT = $07\n");
    fprintf(f, "PAD0      = $7F60\n");
    fprintf(f, ".segment \"CODE\"\n.org $8000\n");
    fprintf(f, "reset:\n        sei\n        cld\n        ldx #$ff\n        txs\n");
    fprintf(f, "        lda #0\n        sta WORLD\n        sta SCROLL_X\n        sta SCROLL_Y\n");
    fprintf(f, "        lda #PPUCTRL_BOOT\n        sta PPUCTRL\n");
    fprintf(f, "; palette + MAP stream patched at export -- see prg_phase1.c\n");
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

static int tile_nonzero(const uint8_t tile[R01_TILE_BYTES]) {
    int i;
    for (i = 0; i < R01_TILE_BYTES; i++) {
        if (tile[i]) {
            return 1;
        }
    }
    return 0;
}

static int spr0_tile_referenced(const R01World *w, int tile_id) {
    int ti, si, fi, pi;
    if (!w) {
        return 0;
    }
    for (ti = 0; ti < w->entity_count; ti++) {
        const R01EntityType *ent = &w->entities[ti];
        for (si = 0; si < ent->state_count; si++) {
            const R01EntityState *st = &ent->states[si];
            for (fi = 0; fi < st->frame_count; fi++) {
                const R01EntityFrame *fr = &st->frames[fi];
                for (pi = 0; pi < fr->part_count; pi++) {
                    const R01EntityPart *pt = &fr->parts[pi];
                    if (pt->tile_id != tile_id) {
                        continue;
                    }
                    if (pt->bank == 0 || pt->bank == R01_GLOBAL_SPR_BANK_BASE) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* Copy SPR0/player tile 1 aside before the player stub overwrites it. Returns new id or -1. */
static int relocate_spr0_tile1(uint8_t bank[R01_CHR_BANK_BYTES], const R01World *w) {
    int dest;
    const uint8_t *src = bank + (size_t)R01_SPR_PLAYER_TILE_ID * R01_TILE_BYTES;
    if (!spr0_tile_referenced(w, R01_SPR_PLAYER_TILE_ID) && !tile_nonzero(src)) {
        return -1;
    }
    /* Find a free slot after the stub id; do not clobber merged player-bank art. */
    for (dest = R01_SPR_PLAYER_TILE_ID + 1; dest < R01_TILES_PER_BANK; dest++) {
        if (!tile_nonzero(bank + (size_t)dest * R01_TILE_BYTES) && !spr0_tile_referenced(w, dest)) {
            break;
        }
    }
    if (dest >= R01_TILES_PER_BANK) {
        return -1;
    }
    memcpy(bank + (size_t)dest * R01_TILE_BYTES, src, R01_TILE_BYTES);
    return dest;
}

static void merge_other_spr_into_world_spr(uint8_t bank[R01_CHR_BANK_BYTES], const R01Project *p, int spr_bank) {
    int tid;
    if (!bank || !p || spr_bank < 0 || spr_bank >= R01_SPR_BANKS) {
        return;
    }
    for (tid = 0; tid < p->other_spr_banks[spr_bank].tile_count && tid < R01_TILES_PER_BANK; tid++) {
        const uint8_t *src = p->other_spr_banks[spr_bank].chr + (size_t)tid * R01_TILE_BYTES;
        int b, nonzero = 0;
        for (b = 0; b < R01_TILE_BYTES; b++) {
            if (src[b]) {
                nonzero = 1;
                break;
            }
        }
        if (nonzero) {
            /* Include tile 1: relocate + stub run after merge on SPR0. */
            memcpy(bank + (size_t)tid * R01_TILE_BYTES, src, R01_TILE_BYTES);
        }
    }
}

static uint8_t cart_pack_world_flags(const char *custom_logic_path) {
    int wx = 0;
    int wy = 0;
    int clip = 0;
    int mode = 0;
    uint8_t flags = 0;
    if (custom_logic_path && r01_custom_logic_scan_bg0_wrap(custom_logic_path, &wx, &wy) == 0) {
        if (wx) {
            flags |= R01_CART_WHDR_FLAG_BG0_WRAP_X;
        }
        if (wy) {
            flags |= R01_CART_WHDR_FLAG_BG0_WRAP_Y;
        }
    }
    if (custom_logic_path && r01_custom_logic_scan_bg0_clip_bg1(custom_logic_path, &clip) == 0 && clip) {
        flags |= R01_CART_WHDR_FLAG_BG0_CLIP_BG1;
    }
    if (custom_logic_path && r01_custom_logic_scan_game_mode(custom_logic_path, &mode) == 0 &&
        mode == R01_GAME_MODE_PLATFORMER) {
        flags |= R01_CART_WHDR_FLAG_PLATFORMER;
    }
    return flags;
}

static void cart_pack_platformer_prg(uint8_t prg[R01_PRG_BYTES], const char *custom_logic_path, const R01World *w) {
    int grav = 0;
    int jump = 0;
    int meter = 0;
    int crouch = -1;
    int pe;
    if (!prg) {
        return;
    }
    prg[R01_PRG_PLAT_GRAVITY_OFF] = 0;
    prg[R01_PRG_PLAT_JUMP_OFF] = 0;
    prg[R01_PRG_PLAT_METER_OFF] = 0;
    prg[R01_PRG_PLAT_CROUCH_OFF] = 0xFFu;
    if (custom_logic_path) {
        if (r01_custom_logic_scan_plat_gravity(custom_logic_path, &grav) == 0 && grav > 0) {
            R01PlayPhysics ph;
            r01_play_physics_init(&ph);
            r01_play_physics_set_gravity(&ph, grav);
            prg[R01_PRG_PLAT_GRAVITY_OFF] = (uint8_t)ph.gravity;
        }
        if (r01_custom_logic_scan_plat_jump(custom_logic_path, &jump) == 0 && jump > 0) {
            R01PlayPhysics ph;
            r01_play_physics_init(&ph);
            r01_play_physics_set_jump(&ph, jump);
            prg[R01_PRG_PLAT_JUMP_OFF] = (uint8_t)ph.jump;
        }
        if (r01_custom_logic_scan_plat_meter(custom_logic_path, &meter) == 0 && meter > 0) {
            R01PlayPhysics ph;
            r01_play_physics_init(&ph);
            r01_play_physics_set_meter(&ph, meter);
            prg[R01_PRG_PLAT_METER_OFF] = (uint8_t)ph.meter;
        }
        if (r01_custom_logic_scan_plat_crouch(custom_logic_path, &crouch) != 0) {
            crouch = -1;
        }
    }
    if (crouch < 0 && w) {
        pe = r01_world_player_entity(w);
        if (pe >= 0 && pe < w->entity_count) {
            crouch = r01_entity_crouch_state_index(&w->entities[pe]);
        }
    }
    if (crouch >= 0 && crouch < R01_ENTITY_STATES_MAX) {
        prg[R01_PRG_PLAT_CROUCH_OFF] = (uint8_t)crouch;
    }
}

static int cart_pack_cam_deadzone(uint8_t *out_x, uint8_t *out_y, const char *custom_logic_path) {
    int dx = R01_PLAY_CAM_DEADZONE_X_DEFAULT;
    int dy = R01_PLAY_CAM_DEADZONE_Y_DEFAULT;
    if (custom_logic_path && r01_custom_logic_scan_deadzone(custom_logic_path, &dx, &dy) == 0) {
        /* scanned */
    }
    if (dx < 0) {
        dx = 0;
    }
    if (dy < 0) {
        dy = 0;
    }
    if (dx > 255) {
        dx = 255;
    }
    if (dy > 255) {
        dy = 255;
    }
    if (out_x) {
        *out_x = (uint8_t)dx;
    }
    if (out_y) {
        *out_y = (uint8_t)dy;
    }
    return 0;
}

static int resolve_custom_logic_path(const char *cart_or_stem_path, char *out, size_t out_cap) {
    const char *slash;
    size_t dir_len;
    if (!out || out_cap < 20) {
        return -1;
    }
    if (!cart_or_stem_path || !cart_or_stem_path[0]) {
        snprintf(out, out_cap, "output/C/custom_logic.c");
        return 0;
    }
    slash = strrchr(cart_or_stem_path, '/');
    if (!slash) {
        slash = strrchr(cart_or_stem_path, '\\');
    }
    if (!slash) {
        snprintf(out, out_cap, "C/custom_logic.c");
        return 0;
    }
    dir_len = (size_t)(slash - cart_or_stem_path);
    if (dir_len + strlen("/C/custom_logic.c") + 1 > out_cap) {
        return -1;
    }
    memcpy(out, cart_or_stem_path, dir_len);
    snprintf(out + dir_len, out_cap - dir_len, "/C/custom_logic.c");
    return 0;
}

static int build_world_blob(Buf *blob, const R01Project *p, const R01World *w, const char *custom_logic_path) {
    uint8_t hdr[WORLD_HDR_SIZE];
    uint8_t dir[R01_MAX_PRESENT_SCREENS * SCREEN_DIR_ENT];
    uint8_t bg0_dir[R01_BG0_SCREENS_MAX * SCREEN_DIR_ENT];
    Buf catalog = {0};
    size_t off_chr, off_sdir, off_spay, off_bg0_dir, off_bg0_pay, off_types, off_insts;
    int si, bi, present_n = 0;
    int bg0_n = 0;
    int type_n, inst_n;
    int remap_b0_tile1 = -1;
    uint32_t payload_base;
    uint32_t bg0_payload_base;

    memset(hdr, 0, sizeof(hdr));
    memset(dir, 0, sizeof(dir));
    memset(bg0_dir, 0, sizeof(bg0_dir));
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            present_n++;
        }
    }
    for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
        if (w->bg0_screens[si].present) {
            bg0_n++;
        }
    }
    if (present_n > R01_MAX_PRESENT_SCREENS) {
        return -1;
    }
    if (present_n > 255) {
        present_n = 255;
    }
    if (bg0_n > R01_BG0_SCREENS_MAX) {
        bg0_n = R01_BG0_SCREENS_MAX;
    }
    type_n = w->entity_count;
    if (type_n > 255) {
        type_n = 255;
    }
    if (type_n > R01_MAX_ENTITY_TYPES) {
        type_n = R01_MAX_ENTITY_TYPES;
    }
    /* Placements ship in PRG ($81C0+), not the cart world blob (docs). */
    inst_n = 0;
    {
        uint8_t spr0[R01_CHR_BANK_BYTES];
        size_t n = (size_t)w->spr_banks[0].tile_count * R01_TILE_BYTES;
        memset(spr0, 0, sizeof(spr0));
        if (n > sizeof(spr0)) {
            n = sizeof(spr0);
        }
        memcpy(spr0, w->spr_banks[0].chr, n);
        merge_other_spr_into_world_spr(spr0, p, 0);
        remap_b0_tile1 = relocate_spr0_tile1(spr0, w);
    }
    if (build_entity_catalog(&catalog, w, type_n, remap_b0_tile1) != 0) {
        return -1;
    }
    off_chr = WORLD_HDR_SIZE;
    off_sdir = off_chr + (size_t)R01_BG_BANKS * R01_CHR_BANK_BYTES + (size_t)R01_SPR_BANKS * R01_CHR_BANK_BYTES;
    off_spay = off_sdir + (size_t)present_n * SCREEN_DIR_ENT;
    payload_base = (uint32_t)off_spay;
    off_bg0_dir = off_spay + (size_t)present_n * SCREEN_PAYLOAD;
    off_bg0_pay = off_bg0_dir + (size_t)bg0_n * SCREEN_DIR_ENT;
    bg0_payload_base = (uint32_t)off_bg0_pay;
    off_types = off_bg0_pay + (size_t)bg0_n * SCREEN_PAYLOAD;
    off_insts = off_types + catalog.len;

    {
        int ds = r01_world_default_screen(w);
        const R01Screen *spawn = &w->screens[ds];
        put_u8(hdr + 0, R01_CELL_PACK(spawn->col, spawn->row));
        put_u8(hdr + 1, 0);
    }
    put_u8(hdr + 2, (uint8_t)(w->default_bg_bank & 3));
    /* hdr[3]: BG0 present extent (cols | rows<<4). 0 when no BG0. */
    if (bg0_n > 0) {
        int min_c = 99, min_r = 99, max_c = 0, max_r = 0;
        int pc, pr;
        for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
            if (!w->bg0_screens[si].present) {
                continue;
            }
            if (w->bg0_screens[si].col < min_c) {
                min_c = w->bg0_screens[si].col;
            }
            if (w->bg0_screens[si].row < min_r) {
                min_r = w->bg0_screens[si].row;
            }
            if (w->bg0_screens[si].col > max_c) {
                max_c = w->bg0_screens[si].col;
            }
            if (w->bg0_screens[si].row > max_r) {
                max_r = w->bg0_screens[si].row;
            }
        }
        pc = max_c - min_c + 1;
        pr = max_r - min_r + 1;
        if (pc < 1) {
            pc = 1;
        }
        if (pr < 1) {
            pr = 1;
        }
        put_u8(hdr + 3, (uint8_t)((pc & 0x0f) | ((pr & 0x0f) << 4)));
    } else {
        put_u8(hdr + 3, 0);
    }
    put_u8(hdr + 4, (uint8_t)(w->default_pal_row & 7));
    put_u8(hdr + 5, (uint8_t)present_n);
    put_u8(hdr + 6, (uint8_t)bg0_n); /* BG0 present count (hdr was parallax_count) */
    put_u24(hdr + 8, (uint32_t)off_chr);
    put_u24(hdr + 11, (uint32_t)off_sdir);
    put_u24(hdr + 14, bg0_n > 0 ? (uint32_t)off_bg0_dir : 0u);
    put_u8(hdr + R01_CART_WHDR_TYPE_COUNT, (uint8_t)type_n);
    put_u8(hdr + R01_CART_WHDR_INST_COUNT, (uint8_t)inst_n);
    put_u24(hdr + R01_CART_WHDR_OFF_TYPES, (uint32_t)off_types);
    put_u24(hdr + R01_CART_WHDR_OFF_INSTS, (uint32_t)off_insts);
    {
        int pe = r01_world_player_entity(w);
        put_u8(hdr + R01_CART_WHDR_PLAYER_ENTITY, R01_CART_PLAYER_ENTITY_NONE);
        put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_X, 0);
        put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_Y, 0);
        put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_W, (uint8_t)R01_PLAY_PLAYER_W);
        put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_H, (uint8_t)R01_PLAY_PLAYER_H);
        if (pe >= 0 && pe < type_n && w->entities[pe].state_count > 0 &&
            w->entities[pe].states[0].frame_count > 0) {
            const R01EntityState *st = &w->entities[pe].states[0];
            const R01EntityFrame *fr = r01_entity_state_hitbox_origin_frame(st);
            uint8_t hx, hy, hw, hh;
            if (!fr) {
                fr = &st->frames[0];
            }
            pack_hitbox_rel(fr->origin_x, fr->origin_y, st->hitbox_x, st->hitbox_y, st->hitbox_w, st->hitbox_h,
                            &hx, &hy, &hw, &hh);
            put_u8(hdr + R01_CART_WHDR_PLAYER_ENTITY, (uint8_t)pe);
            put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_X, hx);
            put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_Y, hy);
            put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_W, hw);
            put_u8(hdr + R01_CART_WHDR_PLAYER_HIT_H, hh);
        }
    }
    {
        uint8_t dz_x;
        uint8_t dz_y;
        cart_pack_cam_deadzone(&dz_x, &dz_y, custom_logic_path);
        put_u8(hdr + R01_CART_WHDR_CAM_DEADZONE_X, dz_x);
        put_u8(hdr + R01_CART_WHDR_CAM_DEADZONE_Y, dz_y);
    }
    put_u8(hdr + R01_CART_WHDR_FLAGS, cart_pack_world_flags(custom_logic_path));

    if (buf_append(blob, hdr, WORLD_HDR_SIZE) != 0) {
        free(catalog.data);
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
            free(catalog.data);
            return -1;
        }
    }
    for (bi = 0; bi < R01_SPR_BANKS; bi++) {
        uint8_t bank[R01_CHR_BANK_BYTES];
        size_t n = (size_t)w->spr_banks[bi].tile_count * R01_TILE_BYTES;
        memset(bank, 0, sizeof(bank));
        if (n > sizeof(bank)) {
            n = sizeof(bank);
        }
        memcpy(bank, w->spr_banks[bi].chr, n);
        merge_other_spr_into_world_spr(bank, p, bi);
        if (bi == 0) {
            /* Remap already computed for the entity catalog; apply the same bank edit. */
            (void)relocate_spr0_tile1(bank, w);
            fill_solid_tile(bank + (size_t)R01_SPR_PLAYER_TILE_ID * R01_TILE_BYTES, 1);
        }
        if (buf_append(blob, bank, sizeof(bank)) != 0) {
            free(catalog.data);
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
            put_u8(e + 0, R01_CELL_PACK(s->col, s->row));
            put_u8(e + 1, 0);
            put_u8(e + 2, 0);
            put_u8(e + 3, 0);
            put_u24(e + 4, payload_base + (uint32_t)di * SCREEN_PAYLOAD);
            put_u24(e + 7, 0);
            di++;
        }
        if (buf_append(blob, dir, (size_t)present_n * SCREEN_DIR_ENT) != 0) {
            free(catalog.data);
            return -1;
        }
        for (si = 0; si < w->screen_count; si++) {
            const R01Screen *s = &w->screens[si];
            uint8_t attrs[R01_ATTRS_PER_SCREEN];
            int cell;
            if (!s->present) {
                continue;
            }
            memcpy(attrs, s->attrs, sizeof(attrs));
            for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
                if (s->tiles[cell] == 0) {
                    attrs[cell] = 0;
                }
            }
            if (buf_append(blob, s->tiles, R01_TILES_PER_SCREEN) != 0 ||
                buf_append(blob, attrs, R01_ATTRS_PER_SCREEN) != 0) {
                free(catalog.data);
                return -1;
            }
        }
    }
    if (bg0_n > 0) {
        int di = 0;
        int min_c = 99, min_r = 99;
        /* Cart BG0 coords are origin-relative to the present bbox (Host Play samples from 0). */
        for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
            if (!w->bg0_screens[si].present) {
                continue;
            }
            if (w->bg0_screens[si].col < min_c) {
                min_c = w->bg0_screens[si].col;
            }
            if (w->bg0_screens[si].row < min_r) {
                min_r = w->bg0_screens[si].row;
            }
        }
        for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
            const R01Screen *s = &w->bg0_screens[si];
            uint8_t *e;
            if (!s->present) {
                continue;
            }
            e = bg0_dir + (size_t)di * SCREEN_DIR_ENT;
            put_u8(e + 0, R01_CELL_PACK(s->col - min_c, s->row - min_r));
            put_u8(e + 1, 0);
            put_u8(e + 2, 0);
            put_u8(e + 3, 0);
            put_u24(e + 4, bg0_payload_base + (uint32_t)di * SCREEN_PAYLOAD);
            put_u24(e + 7, 0);
            di++;
        }
        if (buf_append(blob, bg0_dir, (size_t)bg0_n * SCREEN_DIR_ENT) != 0) {
            free(catalog.data);
            return -1;
        }
        for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
            const R01Screen *s = &w->bg0_screens[si];
            if (!s->present) {
                continue;
            }
            if (buf_append(blob, s->tiles, R01_TILES_PER_SCREEN) != 0 ||
                buf_append(blob, s->attrs, R01_ATTRS_PER_SCREEN) != 0) {
                free(catalog.data);
                return -1;
            }
        }
    }
    {
        if (catalog.len > 0 && buf_append(blob, catalog.data, catalog.len) != 0) {
            free(catalog.data);
            return -1;
        }
        free(catalog.data);
        catalog.data = NULL;
        catalog.len = 0;
    }
    /* Instance table omitted from cart; see r01_prg_fill_phase1 PLAY_INST_*. */
    {
        int pe = r01_world_player_entity(w);
        if (pe >= 0 && pe < type_n && append_player_anim_blob(blob, w, pe, remap_b0_tile1) == 0) {
            blob->data[R01_CART_WHDR_FLAGS] |= R01_CART_WHDR_FLAG_PLAYER_ANIM;
        }
    }
    return 0;
}

static uint32_t cart_off_map_screen0(const R01World *w, uint32_t world_base) {
    uint32_t off_chr = WORLD_HDR_SIZE;
    uint32_t off_sdir =
        off_chr + (uint32_t)R01_BG_BANKS * R01_CHR_BANK_BYTES + (uint32_t)R01_SPR_BANKS * R01_CHR_BANK_BYTES;
    uint32_t payload_base;
    int present_n = 0;
    int di = 0;
    int si;

    if (!w) {
        return 0;
    }
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            present_n++;
        }
    }
    payload_base = off_sdir + (uint32_t)present_n * SCREEN_DIR_ENT;
    {
        int ds = r01_world_default_screen(w);
        int dc = w->screens[ds].col;
        int dr = w->screens[ds].row;
        for (si = 0; si < w->screen_count; si++) {
            const R01Screen *s = &w->screens[si];
            if (!s->present) {
                continue;
            }
            if (s->col == dc && s->row == dr) {
                return world_base + payload_base + (uint32_t)di * SCREEN_PAYLOAD;
            }
            di++;
        }
    }
    di = 0;
    for (si = 0; si < w->screen_count; si++) {
        const R01Screen *s = &w->screens[si];
        if (!s->present) {
            continue;
        }
        return world_base + payload_base + (uint32_t)di * SCREEN_PAYLOAD;
    }
    return 0;
}

static int r01_cart_build(const R01Project *p, const char *cart_path, uint8_t **out, size_t *out_len, char *err_buf,
                          size_t err_cap) {
    Buf cart = {0};
    R01Project *work;
    uint8_t hdr[HDR_SIZE];
    uint8_t ptrs[PTR_TABLE_SIZE];
    uint8_t wtable[WORLD_TABLE_SIZE];
    uint8_t prg[R01_PRG_BYTES];
    size_t ptr_bytes = (size_t)PTR_TABLE_SIZE;
    uint32_t off_prg, off_pal_bg, off_pal_spr, off_other, off_other_chr, off_wtable, world_base;
    size_t other_len;
    R01PrgCartLayout prg_layout;
    Buf world_blob = {0};
    Buf other_blob = {0};
    uint8_t other_chr[R01_CART_OTHER_CHR_BYTES];
    int bi;

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
    {
        char custom_logic_path[R01_PATH_MAX];
        resolve_custom_logic_path(cart_path, custom_logic_path, sizeof(custom_logic_path));
        if (build_world_blob(&world_blob, work, &work->worlds[0], custom_logic_path) != 0) {
        free(work);
        free(world_blob.data);
        if (err_buf && err_cap > 0) {
            snprintf(err_buf, err_cap, "world blob failed (>%d present screens?)", R01_MAX_PRESENT_SCREENS);
        }
        return -1;
        }
    }
    if (build_other_blob(&other_blob, work) != 0) {
        free(work);
        free(world_blob.data);
        free(other_blob.data);
        set_err(err_buf, err_cap, "other screens blob failed");
        return -1;
    }
    other_len = other_blob.len;

    memset(other_chr, 0, sizeof(other_chr));
    for (bi = 0; bi < R01_BG_BANKS; bi++) {
        size_t n = (size_t)work->other_bg_banks[bi].tile_count * R01_TILE_BYTES;
        if (n > R01_CHR_BANK_BYTES) {
            n = R01_CHR_BANK_BYTES;
        }
        memcpy(other_chr + (size_t)bi * R01_CHR_BANK_BYTES, work->other_bg_banks[bi].chr, n);
    }
    for (bi = 0; bi < R01_SPR_BANKS; bi++) {
        size_t n = (size_t)work->other_spr_banks[bi].tile_count * R01_TILE_BYTES;
        if (n > R01_CHR_BANK_BYTES) {
            n = R01_CHR_BANK_BYTES;
        }
        memcpy(other_chr + (4u + (size_t)bi) * R01_CHR_BANK_BYTES, work->other_spr_banks[bi].chr, n);
    }

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "retr01", 6);
    hdr[6] = R01_CART_FORMAT_VER;
    hdr[7] = 1;

    off_pal_bg = HDR_SIZE + (uint32_t)ptr_bytes;
    off_pal_spr = off_pal_bg + R01_PAL_PLANE_BYTES;
    off_prg = off_pal_spr + R01_PAL_PLANE_BYTES;
    off_other_chr = off_prg + R01_PRG_BYTES;
    off_other = off_other_chr + R01_CART_OTHER_CHR_BYTES;
    off_wtable = off_other + (uint32_t)other_len;
    world_base = off_wtable + WORLD_TABLE_SIZE;

    memset(&prg_layout, 0, sizeof(prg_layout));
    prg_layout.off_pal_bg = off_pal_bg;
    prg_layout.len_pal_bg = R01_PAL_PLANE_BYTES;
    prg_layout.off_pal_spr = off_pal_spr;
    prg_layout.len_pal_spr = R01_PAL_PLANE_BYTES;
    prg_layout.default_pal_row = (uint8_t)(work->worlds[0].default_pal_row & 7u);
    prg_layout.off_map_screen0 = cart_off_map_screen0(&work->worlds[0], world_base);
    r01_prg_fill_phase1(prg, &work->worlds[0], &prg_layout);
    {
        char custom_logic_path[R01_PATH_MAX];
        resolve_custom_logic_path(cart_path, custom_logic_path, sizeof(custom_logic_path));
        cart_pack_platformer_prg(prg, custom_logic_path, &work->worlds[0]);
    }

    memset(ptrs, 0, sizeof(ptrs));
    put_u24(ptrs + 0, off_prg);
    put_u24(ptrs + 3, R01_PRG_BYTES);
    put_u24(ptrs + 6, off_pal_bg);
    put_u24(ptrs + 9, R01_PAL_PLANE_BYTES);
    put_u24(ptrs + 12, off_pal_spr);
    put_u24(ptrs + 15, R01_PAL_PLANE_BYTES);
    put_u24(ptrs + 18, off_wtable);
    put_u24(ptrs + 21, WORLD_TABLE_SIZE);
    put_u24(ptrs + 24, off_other);
    put_u24(ptrs + 27, (uint32_t)other_len);
    put_u24(ptrs + 30, off_other_chr);
    put_u24(ptrs + 33, R01_CART_OTHER_CHR_BYTES);

    if (buf_append(&cart, hdr, HDR_SIZE) != 0 || buf_append(&cart, ptrs, ptr_bytes) != 0 ||
        append_pal_plane(&cart, work->global_pal_bg) != 0 ||
        append_pal_plane(&cart, work->global_pal_spr) != 0 || buf_append(&cart, prg, R01_PRG_BYTES) != 0 ||
        buf_append(&cart, other_chr, R01_CART_OTHER_CHR_BYTES) != 0 ||
        buf_append(&cart, other_blob.data, other_len) != 0) {
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
    free(other_blob.data);
    free(work);
    *out = cart.data;
    *out_len = cart.len;
    return 0;

oom:
    free(work);
    free(world_blob.data);
    free(other_blob.data);
    free(cart.data);
    set_err(err_buf, err_cap, "oom");
    return -1;
}

int r01_cart_write(const R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    uint8_t *img = NULL;
    size_t len = 0;
    FILE *f;
    if (r01_cart_build(p, path, &img, &len, err_buf, err_cap) != 0) {
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
    if (r01_cart_build(p, path, &img, &len, err_buf, err_cap) != 0) {
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
    if (r01_export_codegen(p, path_stem, err_buf, err_cap) != 0) {
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
    snprintf(path, sizeof(path), "%s_flash.bin", path_stem);
    if (r01_cart_write_flash(p, path, err_buf, err_cap) != 0) {
        return -1;
    }
    return 0;
}
