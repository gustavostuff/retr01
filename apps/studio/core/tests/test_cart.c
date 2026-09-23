#include "test_harness.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CART_HDR_SIZE 16u
#define CART_PTR_SIZE R01_CART_PTR_TABLE_BYTES
#define CART_PAL_PLANE_BYTES 128u
#define CART_PRG_OFF (CART_HDR_SIZE + CART_PTR_SIZE + 2u * CART_PAL_PLANE_BYTES)
#define PRG_PLAY_SPAWN_CELL 0x0120u
#define PRG_PLAY_INST_COUNT 0x01C0u
#define PRG_PLAY_INST_TABLE 0x01C1u
#define WORLD_SLOT_SIZE 8u
#define WORLD_HDR_SIZE 32u

static uint32_t rd_u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Mirror cart RLE decode for test asserts. */
static int test_rle_decode_480(const uint8_t *in, size_t in_len, uint8_t out[480]) {
    size_t ip = 0, op = 0;
    while (op < 480) {
        uint8_t cmd;
        size_t n;
        if (ip >= in_len) {
            return -1;
        }
        cmd = in[ip++];
        if (cmd & 0x80u) {
            n = (size_t)(cmd & 0x7Fu) + 1u;
            if (ip >= in_len || op + n > 480) {
                return -1;
            }
            memset(out + op, in[ip++], n);
            op += n;
        } else {
            n = (size_t)cmd + 1u;
            if (ip + n > in_len || op + n > 480) {
                return -1;
            }
            memcpy(out + op, in + ip, n);
            ip += n;
            op += n;
        }
    }
    return 0;
}

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;
    uint8_t tile[R01_TILE_BYTES];
    char err[128];
    int bank, id, cat, type_id, inst;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "cart");
    w = &p->worlds[0];
    w->default_screen = 2;

    bank = 0;
    id = r01_chr_alloc_spr_tile(p, bank);
    EXPECT(id == 0, "spr tile 0");
    memset(tile, 0, sizeof(tile));
    tile[0] = 0xA5;
    tile[8] = 0x5A;
    EXPECT(r01_chr_write_spr_tile(p, bank, id, tile) == 0, "write spr0");
    /* Authoring is contiguous: next slot is tile 1 (cart export relocates stub conflict). */
    id = r01_chr_alloc_spr_tile(p, bank);
    EXPECT(id == 1, "spr tile 1 contiguous");
    memset(tile, 0, sizeof(tile));
    tile[0] = 0x3C;
    tile[8] = 0xC3;
    EXPECT(r01_chr_write_spr_tile(p, bank, id, tile) == 0, "write spr1");
    cat = r01_world_sprite_add(p, bank, id, 2);
    EXPECT(cat == 0, "catalog");
    type_id = r01_world_entity_from_sprite(p, cat);
    EXPECT(type_id == 0, "entity type");
    p->entities[0].states[0].frames[0].delay = 24;
    p->entities[0].states[0].frames[0].origin_y = 3;
    p->entities[0].states[0].hitbox_x = 1;
    p->entities[0].states[0].hitbox_y = 2;
    p->entities[0].states[0].frames[0].parts[0].dx = 4;
    p->entities[0].states[0].frames[0].parts[0].dy = 5;
    r01_world_set_player_entity(p, 0);
    inst = r01_world_place_entity(w, type_id, 40, 50);
    EXPECT(inst == 0, "instance");
    w->instances[0].flip_h = 1;

    p->other_screens[R01_CART_OTHER_TITLE].tiles[0] = 0x42;
    /* Credits page (id 2): mostly blank -> RLE in other screens. */
    p->other_screens[R01_CART_OTHER_CREDITS_FIRST].present = 1;
    p->other_screens[R01_CART_OTHER_CREDITS_FIRST].tiles[0] = 0x11;

    /* Second entity shares no stub conflict; uses tile 0. */
    {
        R01EntityPart *pt;
        type_id = r01_world_entity_add(p);
        EXPECT(type_id == 1, "second entity");
        pt = &p->entities[type_id].states[0].frames[0].parts[0];
        memset(pt, 0, sizeof(*pt));
        pt->bank = 0;
        pt->tile_id = 0;
        p->entities[type_id].states[0].frames[0].part_count = 1;
        EXPECT(r01_world_place_entity(w, type_id, 10, 10) >= 0, "second inst");
    }

    EXPECT(r01_project_set_active_world(p, 1) == 0, "world 1");
    EXPECT(r01_world_create_screen(&p->worlds[1], 0, 0) >= 0, "world 1 screen");
    EXPECT(r01_world_place_entity(&p->worlds[1], 0, 16, 24) >= 0, "world 1 inst");

    EXPECT(r01_cart_write(p, "test_cart.retr01", err, sizeof(err)) == 0, "cart write");
    {
        FILE *f = fopen("test_cart.retr01", "rb");
        uint8_t *img = NULL;
        long flen = 0;
        char magic[6];
        uint8_t prg_spawn[2];
        long prg_off = (long)CART_PRG_OFF;

        EXPECT(f != NULL, "open cart");
        if (f) {
            EXPECT(fread(magic, 1, 6, f) == 6, "read cart magic");
            EXPECT(memcmp(magic, "retr01", 6) == 0, "cart magic");
            EXPECT(fseek(f, 6, SEEK_SET) == 0, "seek format_ver");
            {
                uint8_t fmt = 0;
                EXPECT(fread(&fmt, 1, 1, f) == 1, "read format_ver");
                EXPECT(fmt == R01_CART_FORMAT_VER, "cart format_ver");
            }
            EXPECT(fseek(f, prg_off + (long)PRG_PLAY_SPAWN_CELL, SEEK_SET) == 0, "seek prg spawn");
            EXPECT(fread(prg_spawn, 1, 1, f) == 1, "read prg spawn");
            EXPECT(prg_spawn[0] == R01_CELL_PACK(2, 0), "prg spawn cell matches default screen");
            {
                uint8_t sei = 0;
                uint8_t inst_n = 0;
                uint8_t cbyte = 0;
                EXPECT(fseek(f, prg_off, SEEK_SET) == 0, "seek PRG");
                EXPECT(fread(&sei, 1, 1, f) == 1, "read SEI");
                EXPECT(sei == 0x78, "llvm-mos PRG reset SEI");
                EXPECT(fseek(f, prg_off + (long)PRG_PLAY_INST_COUNT, SEEK_SET) == 0, "seek inst count");
                EXPECT(fread(&inst_n, 1, 1, f) == 1, "read inst count");
                EXPECT(inst_n >= 1, "spawn table at $81C0");
                EXPECT(fseek(f, prg_off + (long)R01_PRG_C_OFF, SEEK_SET) == 0, "seek C");
                EXPECT(fread(&cbyte, 1, 1, f) == 1, "read C");
                EXPECT(cbyte != 0, "C code at $C400");
            }

            EXPECT(fseek(f, 0, SEEK_END) == 0, "seek end");
            flen = ftell(f);
            EXPECT(flen > 0, "cart size");
            img = (uint8_t *)malloc((size_t)flen);
            EXPECT(img != NULL, "cart buf");
            if (img) {
                uint8_t ptrs[CART_PTR_SIZE];
                uint8_t slot[8];
                uint8_t hdr[WORLD_HDR_SIZE];
                uint32_t off_prg, off_wtable, world_base, off_chr, off_types, off_insts;
                uint8_t type_n, inst_n;
                uint8_t irec[R01_CART_INSTANCE_SIZE];

                EXPECT(fseek(f, 0, SEEK_SET) == 0, "rewind");
                EXPECT(fread(img, 1, (size_t)flen, f) == (size_t)flen, "read cart");
                memcpy(ptrs, img + CART_HDR_SIZE, CART_PTR_SIZE);
                off_prg = rd_u24(ptrs + 0);
                off_wtable = rd_u24(ptrs + 36);
                EXPECT(off_prg == CART_PRG_OFF, "off_prg");
                EXPECT(rd_u24(ptrs + 3) == R01_PRG_BYTES, "len_prg 32KB");
                EXPECT(rd_u24(ptrs + 18) == CART_PRG_OFF + R01_PRG_BYTES, "off_chr");
                EXPECT(rd_u24(ptrs + 21) == R01_CART_GLOBAL_CHR_BYTES, "len_chr");
                EXPECT(rd_u24(ptrs + 27) > 0, "len_entities");
                EXPECT(rd_u24(ptrs + 33) > 0, "len_other");
                EXPECT(img[rd_u24(ptrs + 30)] == 3, "other_count title+inter+credits");
                EXPECT(img[rd_u24(ptrs + 30) + 4u] == 0, "other dir0 id title");
                {
                    uint32_t off_other = rd_u24(ptrs + 30);
                    const uint8_t *e0 = img + off_other + 4u;
                    uint16_t plen = rd_u16(e0 + 2);
                    uint32_t title_rel = rd_u24(e0 + 4);
                    uint8_t decoded[480];
                    EXPECT(plen > 0 && plen <= 480, "title payload len");
                    if (e0[1] & R01_CART_OTHER_FLAG_RLE) {
                        EXPECT(test_rle_decode_480(img + off_other + title_rel, plen, decoded) == 0,
                               "title RLE decode");
                        EXPECT(decoded[0] == 0x42, "title tile0 after decode");
                    } else {
                        EXPECT(img[off_other + title_rel] == 0x42, "title screen tile byte");
                    }
                }
                memcpy(slot, img + off_wtable, 8);
                EXPECT(slot[0] != 0, "world0 present");
                world_base = rd_u24(slot + 2);
                memcpy(slot, img + off_wtable + WORLD_SLOT_SIZE, 8);
                EXPECT(slot[0] != 0, "world1 present");
                EXPECT(img[7] == 2, "cart world count 2");
                {
                    uint16_t w0play = rd_u16(img + (size_t)prg_off + 0x0500u);
                    uint16_t w1play = rd_u16(img + (size_t)prg_off + 0x0502u);
                    uint8_t w1n;
                    EXPECT(w0play == 0x8100u, "world 0 play ptr");
                    EXPECT(w1play == 0x8800u, "world 1 play ptr");
                    w1n = img[(size_t)prg_off + (size_t)(w1play - 0x8000u) + 0xC0u];
                    EXPECT(w1n >= 1, "world 1 instances in PRG");
                }
                memcpy(slot, img + off_wtable, 8);
                world_base = rd_u24(slot + 2);
                memcpy(hdr, img + world_base, WORLD_HDR_SIZE);
                type_n = hdr[R01_CART_WHDR_TYPE_COUNT];
                inst_n = hdr[R01_CART_WHDR_INST_COUNT];
                off_types = rd_u24(hdr + R01_CART_WHDR_OFF_TYPES);
                off_insts = rd_u24(hdr + R01_CART_WHDR_OFF_INSTS);
                EXPECT(type_n == 2, "type count");
                EXPECT(inst_n == 0, "inst count zero in cart (placements in PRG)");
                EXPECT(hdr[R01_CART_WHDR_PLAYER_ENTITY] == 0, "player entity packed");
                /* Hitbox is i8 origin-relative (origin 4,3 hit 1,2 -> -3,-1). */
                EXPECT(hdr[R01_CART_WHDR_PLAYER_HIT_X] == (uint8_t)(int8_t)-3 &&
                           hdr[R01_CART_WHDR_PLAYER_HIT_Y] == (uint8_t)(int8_t)-1,
                       "player hitbox xy draw-origin");
                EXPECT(hdr[R01_CART_WHDR_PLAYER_HIT_W] == R01_ENTITY_HITBOX_W &&
                           hdr[R01_CART_WHDR_PLAYER_HIT_H] == R01_ENTITY_HITBOX_H,
                       "player hitbox wh");

                off_chr = rd_u24(hdr + 8);
                EXPECT(off_chr == 0, "world blob has no CHR");
                /* Entity catalog is cart-global. */
                {
                    uint32_t off_ents = rd_u24(ptrs + 24);
                    uint16_t d0 = rd_u16(img + off_ents);
                    uint16_t d1 = rd_u16(img + off_ents + 2);
                    const uint8_t *def0;
                    const uint8_t *def1;
                    const uint8_t *st;
                    const uint8_t *fr;
                    EXPECT(d0 == 4, "dir0 after 2-entry directory");
                    EXPECT(d1 > d0, "dir1 after def0");
                    def0 = img + off_ents + d0;
                    EXPECT(def0[1] == 1, "def0 state count");
                    st = def0 + rd_u16(def0 + 4);
                    EXPECT(st[0] == 1, "def0 frame count");
                    fr = st + rd_u16(st + 2);
                    EXPECT(fr[0] == 24, "def0 frame delay from entity");
                    EXPECT(fr[1] == 1, "def0 sprite count");
                    EXPECT(fr[2] == (uint8_t)(int8_t)-3 && fr[3] == (uint8_t)(int8_t)-1,
                           "def0 frame hitbox xy");
                    EXPECT(fr[4] == R01_ENTITY_HITBOX_W && fr[5] == R01_ENTITY_HITBOX_H, "def0 frame hitbox wh");
                    def1 = img + off_ents + d1;
                    EXPECT(def1[1] == 1, "def1 state count");
                    st = def1 + rd_u16(def1 + 4);
                    fr = st + rd_u16(st + 2);
                    EXPECT(fr[1] == 1, "def1 sprite count");
                    EXPECT(world_base + off_insts <= (uint32_t)flen, "off_insts in cart");
                    EXPECT((hdr[R01_CART_WHDR_FLAGS] & R01_CART_WHDR_FLAG_PLAYER_ANIM) == 0, "no PA blob");
                }
                {
                    uint8_t prg_inst_n = img[off_prg + PRG_PLAY_INST_COUNT];
                    EXPECT(prg_inst_n == 2, "prg inst count");
                    memcpy(irec, img + off_prg + PRG_PLAY_INST_TABLE, R01_CART_INSTANCE_SIZE);
                    EXPECT(irec[0] == 0, "prg inst type");
                    EXPECT(irec[1] == 1, "prg inst flip_h flag");
                    EXPECT(rd_u16(irec + 2) == 40, "prg inst x");
                    EXPECT(rd_u16(irec + 4) == 50, "prg inst y");
                }
                free(img);
            }
            fclose(f);
        }
    }

    EXPECT(r01_prom_write("test_prom.bin", err, sizeof(err)) == 0, "prom write");
    {
        FILE *f = fopen("test_prom.bin", "rb");
        uint8_t prom[R01_MASTER_COLORS];
        EXPECT(f != NULL, "open prom");
        if (f) {
            EXPECT(fread(prom, 1, sizeof(prom), f) == sizeof(prom), "prom size");
            fclose(f);
        }
    }

    /* Other SPR patterns merge into exported world SPR; OAM global banks pack as 0..3.
     * Tile 1 is relocated so the cart stub can occupy that slot. */
    {
        R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
        R01World *w2;
        R01EntityPart *pt;
        uint8_t art[R01_TILE_BYTES];
        uint8_t face[R01_TILE_BYTES];
        EXPECT(p2 != NULL, "alloc p2");
        if (p2) {
            r01_project_init(p2, "pb");
            w2 = &p2->worlds[0];
            memset(art, 0, sizeof(art));
            art[0] = 0x81;
            art[8] = 0x18;
            memset(face, 0, sizeof(face));
            face[0] = 0x5A;
            face[8] = 0xA5;
            EXPECT(r01_other_spr_write_tile(p2, 0, 0, art) == 0, "os tile0");
            EXPECT(r01_other_spr_write_tile(p2, 0, 1, face) == 0, "os tile1 face");
            EXPECT(r01_other_spr_write_tile(p2, 0, 2, art) == 0, "os tile2");
            EXPECT(r01_world_entity_add(p2) == 0, "pb entity");
            pt = &p2->entities[0].states[0].frames[0].parts[0];
            memset(pt, 0, sizeof(*pt));
            pt->bank = R01_GLOBAL_SPR_BANK_BASE;
            pt->tile_id = 1; /* face: must survive stub stamp */
            p2->entities[0].states[0].frames[0].part_count = 1;
            {
                R01EntityPart *pt2 = &p2->entities[0].states[0].frames[0].parts[1];
                memset(pt2, 0, sizeof(*pt2));
                pt2->bank = R01_GLOBAL_SPR_BANK_BASE;
                pt2->tile_id = 2;
                pt2->dx = 8;
                p2->entities[0].states[0].frames[0].part_count = 2;
            }
            r01_world_set_player_entity(p2, 0);
            EXPECT(r01_cart_write(p2, "test_cart_pb.retr01", err, sizeof(err)) == 0, "cart pb write");
            {
                FILE *f = fopen("test_cart_pb.retr01", "rb");
                EXPECT(f != NULL, "open pb cart");
                if (f) {
                    uint8_t ptrs[CART_PTR_SIZE];
                    uint8_t slot[8];
                    uint8_t hdr[WORLD_HDR_SIZE];
                    uint8_t got[R01_TILE_BYTES];
                    uint8_t defbuf[128];
                    uint32_t off_wtable, world_base;
                    uint16_t d0;
                    const uint8_t *st;
                    const uint8_t *fr;
                    EXPECT(fseek(f, CART_HDR_SIZE, SEEK_SET) == 0, "seek ptrs");
                    EXPECT(fread(ptrs, 1, sizeof(ptrs), f) == sizeof(ptrs), "read ptrs");
                    off_wtable = rd_u24(ptrs + 36);
                    EXPECT(fseek(f, (long)off_wtable, SEEK_SET) == 0, "seek wtable");
                    EXPECT(fread(slot, 1, 8, f) == 8, "read slot");
                    world_base = rd_u24(slot + 2);
                    EXPECT(fseek(f, (long)world_base, SEEK_SET) == 0, "seek world");
                    EXPECT(fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr), "read whdr");
                    {
                        uint32_t off_chr_blob = rd_u24(hdr + 8);
                        uint32_t off_chr = rd_u24(ptrs + 18);
                        uint32_t spr0 = off_chr;
                        EXPECT(off_chr_blob == 0, "world CHR omitted");
                        EXPECT(fseek(f, (long)(spr0 + 1u * R01_TILE_BYTES), SEEK_SET) == 0, "seek spr tile1");
                        EXPECT(fread(got, 1, sizeof(got), f) == sizeof(got), "read spr tile1");
                        EXPECT(got[0] == 0x5A && got[8] == 0xA5, "player bank tile1 kept");
                        EXPECT(fseek(f, (long)(spr0 + 2u * R01_TILE_BYTES), SEEK_SET) == 0, "seek spr tile2");
                        EXPECT(fread(got, 1, sizeof(got), f) == sizeof(got), "read spr tile2");
                        EXPECT(got[0] == 0x81 && got[8] == 0x18, "player bank tile2 kept");
                    }
                    {
                        uint32_t off_ents = rd_u24(ptrs + 24);
                        EXPECT(fseek(f, (long)off_ents, SEEK_SET) == 0, "seek types");
                        EXPECT(fread(defbuf, 1, 2, f) == 2, "dir0");
                        d0 = rd_u16(defbuf);
                        EXPECT(fseek(f, (long)(off_ents + d0), SEEK_SET) == 0, "seek def0");
                        {
                            size_t nread = fread(defbuf, 1, sizeof(defbuf), f);
                            EXPECT(nread >= 32, "read def0");
                        }
                        st = defbuf + rd_u16(defbuf + 4);
                        fr = st + rd_u16(st + 2);
                        EXPECT(fr[1] == 2, "two parts");
                        EXPECT(fr[6] == 1, "face stays tile 1");
                        EXPECT((fr[9] & 15) == 0, "player bank packs as spr bank 0");
                        EXPECT(fr[10] == 2, "tile2 part unchanged");
                    }
                    fclose(f);
                }
            }
            free(p2);
        }
    }

    free(p);
    remove("test_cart.retr01");
    remove("test_cart_pb.retr01");
    remove("test_prom.bin");
    TEST_EXIT();
}
