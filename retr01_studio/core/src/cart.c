#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

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

static size_t other_blob_bytes(void) {
    return (size_t)R01_CART_OTHER_HDR_BYTES + (size_t)R01_CART_OTHER_MAX * (size_t)R01_CART_OTHER_DIR_BYTES +
           (size_t)R01_CART_OTHER_MAX * (size_t)R01_CART_SCREEN_PAYLOAD;
}

static int build_other_blob(Buf *b, const R01Project *p) {
    uint8_t hdr[R01_CART_OTHER_HDR_BYTES];
    uint8_t dir[R01_CART_OTHER_MAX * R01_CART_OTHER_DIR_BYTES];
    uint32_t payload_base = (uint32_t)R01_CART_OTHER_HDR_BYTES + (uint32_t)R01_CART_OTHER_MAX * R01_CART_OTHER_DIR_BYTES;
    int i;

    if (!b || !p) {
        return -1;
    }
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = (uint8_t)R01_CART_OTHER_MAX;
    memset(dir, 0, sizeof(dir));
    for (i = 0; i < R01_CART_OTHER_MAX; i++) {
        uint8_t *e = dir + (size_t)i * R01_CART_OTHER_DIR_BYTES;
        e[0] = (uint8_t)i;
        e[1] = 0;
        put_u24(e + 4, payload_base + (uint32_t)i * R01_CART_SCREEN_PAYLOAD);
    }
    if (buf_append(b, hdr, sizeof(hdr)) != 0 || buf_append(b, dir, sizeof(dir)) != 0) {
        return -1;
    }
    for (i = 0; i < R01_CART_OTHER_MAX; i++) {
        const R01OtherScreen *os = &p->other_screens[i];
        if (buf_append(b, os->tiles, R01_TILES_PER_SCREEN) != 0 ||
            buf_append(b, os->attrs, R01_ATTRS_PER_SCREEN) != 0) {
            return -1;
        }
    }
    return 0;
}

static int build_credits_blob(Buf *b, const char *text, size_t *out_len) {
    size_t n;

    if (!b || !out_len) {
        return -1;
    }
    *out_len = 0;
    if (!text || text[0] == '\0') {
        return 0;
    }
    n = strlen(text);
    if (n > R01_CART_CREDITS_MAX) {
        return -1;
    }
    if (n > 0 && buf_append(b, text, n) != 0) {
        return -1;
    }
    *out_len = n;
    return 0;
}

static uint8_t pack_oam_attr(int bank, int pal, int flip_h, int flip_v) {
    uint8_t a = (uint8_t)((bank & 3) | ((pal & 3) << 2));
    if (flip_h) {
        a |= R01_ATTR_FLIP_H;
    }
    if (flip_v) {
        a |= R01_ATTR_FLIP_V;
    }
    return a;
}

static void pack_entity_type_rec(uint8_t out[R01_CART_ENTITY_TYPE_SIZE], const R01EntityType *ent,
                                 int remap_b0_tile1) {
    const R01EntityState *st;
    const R01EntityFrame *fr;
    int pi;
    memset(out, 0, R01_CART_ENTITY_TYPE_SIZE);
    if (!ent || ent->state_count < 1 || ent->states[0].frame_count < 1) {
        return;
    }
    st = &ent->states[0];
    fr = &st->frames[0];
    out[0] = (uint8_t)st->origin_x;
    out[1] = (uint8_t)st->origin_y;
    out[2] = (uint8_t)(fr->part_count > R01_CART_ENTITY_PARTS_MAX ? R01_CART_ENTITY_PARTS_MAX : fr->part_count);
    out[3] = 0;
    for (pi = 0; pi < (int)out[2]; pi++) {
        const R01EntityPart *pt = &fr->parts[pi];
        uint8_t *slot = out + 4 + pi * 4;
        int tile = pt->tile_id;
        if (remap_b0_tile1 >= 0 && pt->bank == 0 && pt->tile_id == R01_SPR_PLAYER_TILE_ID) {
            tile = remap_b0_tile1;
        }
        slot[0] = (uint8_t)tile;
        slot[1] = pack_oam_attr(pt->bank, pt->pal, pt->flip_h, pt->flip_v);
        slot[2] = (uint8_t)(int8_t)pt->dx;
        slot[3] = (uint8_t)(int8_t)pt->dy;
    }
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
    fprintf(f, "; Play table @ $8100: present[8], spawn, coll_dir @ $810A.\n");
    fprintf(f, "; play_pos_ok @ $8500 (PRG+$0500): solid shadow probe via ($20),Y.\n");
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
    int ti, pi;
    if (!w) {
        return 0;
    }
    for (ti = 0; ti < w->entity_count; ti++) {
        const R01EntityType *ent = &w->entities[ti];
        const R01EntityFrame *fr;
        if (ent->state_count < 1 || ent->states[0].frame_count < 1) {
            continue;
        }
        fr = &ent->states[0].frames[0];
        for (pi = 0; pi < fr->part_count; pi++) {
            if (fr->parts[pi].bank == 0 && fr->parts[pi].tile_id == tile_id) {
                return 1;
            }
        }
    }
    return 0;
}

/* Copy bank-0 tile 1 aside before the player stub overwrites it. Returns new id or -1. */
static int relocate_spr0_tile1(uint8_t bank[R01_CHR_BANK_BYTES], const R01World *w) {
    int dest;
    const uint8_t *src = bank + (size_t)R01_SPR_PLAYER_TILE_ID * R01_TILE_BYTES;
    if (!spr0_tile_referenced(w, R01_SPR_PLAYER_TILE_ID) && !tile_nonzero(src)) {
        return -1;
    }
    dest = w->spr_banks[0].tile_count;
    if (dest <= R01_SPR_PLAYER_TILE_ID) {
        dest = R01_SPR_PLAYER_TILE_ID + 1;
    }
    if (dest >= R01_TILES_PER_BANK) {
        for (dest = R01_SPR_PLAYER_TILE_ID + 1; dest < R01_TILES_PER_BANK; dest++) {
            if (!spr0_tile_referenced(w, dest) &&
                !tile_nonzero(bank + (size_t)dest * R01_TILE_BYTES)) {
                break;
            }
        }
        if (dest >= R01_TILES_PER_BANK) {
            return -1;
        }
    }
    memcpy(bank + (size_t)dest * R01_TILE_BYTES, src, R01_TILE_BYTES);
    return dest;
}

static int build_world_blob(Buf *blob, const R01World *w) {
    uint8_t hdr[WORLD_HDR_SIZE];
    uint8_t dir[R01_MAX_SCREENS * SCREEN_DIR_ENT];
    size_t off_chr, off_sdir, off_spay, off_types, off_insts;
    int si, bi, present_n = 0;
    int type_n, inst_n;
    int remap_b0_tile1 = -1;
    uint32_t payload_base;

    memset(hdr, 0, sizeof(hdr));
    memset(dir, 0, sizeof(dir));
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            present_n++;
        }
    }
    if (present_n > R01_MAX_PRESENT_SCREENS) {
        return -1;
    }
    if (present_n > 255) {
        present_n = 255;
    }
    type_n = w->entity_count;
    if (type_n > 255) {
        type_n = 255;
    }
    if (type_n > R01_MAX_ENTITY_TYPES) {
        type_n = R01_MAX_ENTITY_TYPES;
    }
    inst_n = w->instance_count;
    if (inst_n > 255) {
        inst_n = 255;
    }
    if (inst_n > R01_MAX_ENTITY_INSTANCES) {
        inst_n = R01_MAX_ENTITY_INSTANCES;
    }

    off_chr = WORLD_HDR_SIZE;
    off_sdir = off_chr + (size_t)R01_BG_BANKS * R01_CHR_BANK_BYTES + (size_t)R01_SPR_BANKS * R01_CHR_BANK_BYTES;
    off_spay = off_sdir + (size_t)present_n * SCREEN_DIR_ENT;
    payload_base = (uint32_t)off_spay;
    off_types = off_spay + (size_t)present_n * SCREEN_PAYLOAD;
    off_insts = off_types + (size_t)type_n * R01_CART_ENTITY_TYPE_SIZE;

    {
        int ds = r01_world_default_screen(w);
        const R01Screen *spawn = &w->screens[ds];
        put_u8(hdr + 0, (uint8_t)spawn->col);
        put_u8(hdr + 1, (uint8_t)spawn->row);
    }
    put_u8(hdr + 2, (uint8_t)(w->default_bg_bank & 3));
    put_u8(hdr + 3, 0); /* default_spr_bank */
    put_u8(hdr + 4, (uint8_t)(w->default_pal_row & 7));
    put_u8(hdr + 5, (uint8_t)present_n);
    put_u8(hdr + 6, 0);
    put_u24(hdr + 8, (uint32_t)off_chr);
    put_u24(hdr + 11, (uint32_t)off_sdir);
    put_u24(hdr + 14, 0);
    put_u8(hdr + R01_CART_WHDR_TYPE_COUNT, (uint8_t)type_n);
    put_u8(hdr + R01_CART_WHDR_INST_COUNT, (uint8_t)inst_n);
    put_u24(hdr + R01_CART_WHDR_OFF_TYPES, (uint32_t)off_types);
    put_u24(hdr + R01_CART_WHDR_OFF_INSTS, (uint32_t)off_insts);

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
        size_t n = (size_t)w->spr_banks[bi].tile_count * R01_TILE_BYTES;
        memset(bank, 0, sizeof(bank));
        if (n > sizeof(bank)) {
            n = sizeof(bank);
        }
        memcpy(bank, w->spr_banks[bi].chr, n);
        if (bi == 0) {
            /* Tile 1 reserved: solid color-1 player (Studio/emu/sim OAM slot 0).
             * Relocate any user art that still sits there (pre-reservation projects). */
            remap_b0_tile1 = relocate_spr0_tile1(bank, w);
            fill_solid_tile(bank + (size_t)R01_SPR_PLAYER_TILE_ID * R01_TILE_BYTES, 1);
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
    {
        int ti;
        for (ti = 0; ti < type_n; ti++) {
            uint8_t rec[R01_CART_ENTITY_TYPE_SIZE];
            pack_entity_type_rec(rec, &w->entities[ti], remap_b0_tile1);
            if (buf_append(blob, rec, sizeof(rec)) != 0) {
                return -1;
            }
        }
    }
    {
        int ii;
        for (ii = 0; ii < inst_n; ii++) {
            uint8_t rec[R01_CART_INSTANCE_SIZE];
            const R01EntityInstance *inst = &w->instances[ii];
            memset(rec, 0, sizeof(rec));
            rec[0] = (uint8_t)inst->type_id;
            rec[1] = 0;
            put_u16(rec + 2, (uint16_t)inst->world_x);
            put_u16(rec + 4, (uint16_t)inst->world_y);
            if (buf_append(blob, rec, sizeof(rec)) != 0) {
                return -1;
            }
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

static int r01_cart_build(const R01Project *p, uint8_t **out, size_t *out_len, char *err_buf, size_t err_cap) {
    Buf cart = {0};
    R01Project *work;
    uint8_t hdr[HDR_SIZE];
    uint8_t ptrs[PTR_TABLE_SIZE];
    uint8_t wtable[WORLD_TABLE_SIZE];
    uint8_t prg[R01_PRG_BYTES];
    size_t ptr_bytes = (size_t)PTR_TABLE_SIZE;
    uint32_t off_prg, off_pal_bg, off_pal_spr, off_other, off_credits, off_wtable, world_base;
    size_t other_len = other_blob_bytes();
    size_t credits_len = 0;
    R01PrgCartLayout prg_layout;
    Buf world_blob = {0};
    Buf other_blob = {0};
    Buf credits_blob = {0};

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
    if (build_world_blob(&world_blob, &work->worlds[0]) != 0) {
        free(work);
        free(world_blob.data);
        if (err_buf && err_cap > 0) {
            snprintf(err_buf, err_cap, "world blob failed (>%d present screens?)", R01_MAX_PRESENT_SCREENS);
        }
        return -1;
    }
    if (build_other_blob(&other_blob, work) != 0) {
        free(work);
        free(world_blob.data);
        free(other_blob.data);
        set_err(err_buf, err_cap, "other screens blob failed");
        return -1;
    }
    if (build_credits_blob(&credits_blob, work->credits, &credits_len) != 0) {
        free(work);
        free(world_blob.data);
        free(other_blob.data);
        free(credits_blob.data);
        set_err(err_buf, err_cap, "credits text exceeds 1024 bytes");
        return -1;
    }

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "retr01", 6);
    hdr[6] = R01_CART_FORMAT_VER;
    hdr[7] = 1;

    off_pal_bg = HDR_SIZE + (uint32_t)ptr_bytes;
    off_pal_spr = off_pal_bg + R01_PAL_PLANE_BYTES;
    off_prg = off_pal_spr + R01_PAL_PLANE_BYTES;
    off_other = off_prg + R01_PRG_BYTES;
    off_credits = off_other + (uint32_t)other_len;
    off_wtable = off_credits + (uint32_t)credits_len;
    world_base = off_wtable + WORLD_TABLE_SIZE;

    memset(&prg_layout, 0, sizeof(prg_layout));
    prg_layout.off_pal_bg = off_pal_bg;
    prg_layout.len_pal_bg = R01_PAL_PLANE_BYTES;
    prg_layout.off_pal_spr = off_pal_spr;
    prg_layout.len_pal_spr = R01_PAL_PLANE_BYTES;
    prg_layout.default_pal_row = (uint8_t)(work->worlds[0].default_pal_row & 7u);
    prg_layout.off_map_screen0 = cart_off_map_screen0(&work->worlds[0], world_base);
    r01_prg_fill_phase1(prg, &work->worlds[0], &prg_layout);

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
    put_u24(ptrs + 30, credits_len > 0 ? off_credits : 0u);
    put_u24(ptrs + 33, (uint32_t)credits_len);

    if (buf_append(&cart, hdr, HDR_SIZE) != 0 || buf_append(&cart, ptrs, ptr_bytes) != 0 ||
        append_pal_plane(&cart, work->global_pal_bg) != 0 ||
        append_pal_plane(&cart, work->global_pal_spr) != 0 || buf_append(&cart, prg, R01_PRG_BYTES) != 0 ||
        buf_append(&cart, other_blob.data, other_len) != 0 ||
        (credits_len > 0 && buf_append(&cart, credits_blob.data, credits_len) != 0)) {
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
    free(credits_blob.data);
    free(work);
    *out = cart.data;
    *out_len = cart.len;
    return 0;

oom:
    free(work);
    free(world_blob.data);
    free(other_blob.data);
    free(credits_blob.data);
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
