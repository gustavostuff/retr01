#include "test_harness.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CART_HDR_SIZE 16u
#define CART_PTR_SIZE 24u
#define CART_PAL_PLANE_BYTES 128u
#define CART_PRG_OFF (CART_HDR_SIZE + CART_PTR_SIZE + 2u * CART_PAL_PLANE_BYTES)
#define PRG_PLAY_SPAWN_C 0x0108u
#define PRG_PLAY_SPAWN_R 0x0109u
#define WORLD_SLOT_SIZE 8u
#define WORLD_HDR_SIZE 32u

static uint32_t rd_u24(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
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
    id = r01_chr_alloc_spr_tile(w, bank);
    EXPECT(id == 0, "spr tile 0");
    memset(tile, 0, sizeof(tile));
    tile[0] = 0xA5;
    tile[8] = 0x5A;
    EXPECT(r01_chr_write_spr_tile(w, bank, id, tile) == 0, "write spr0");
    /* Tile 1 reserved for player stub -- alloc skips to 2. */
    id = r01_chr_alloc_spr_tile(w, bank);
    EXPECT(id == 2, "spr tile 2 skips reserved 1");
    memset(tile, 0, sizeof(tile));
    tile[0] = 0x3C;
    tile[8] = 0xC3;
    EXPECT(r01_chr_write_spr_tile(w, bank, id, tile) == 0, "write spr2");
    cat = r01_world_sprite_add(w, bank, id, 2);
    EXPECT(cat == 0, "catalog");
    type_id = r01_world_entity_from_sprite(w, cat);
    EXPECT(type_id == 0, "entity type");
    w->entities[0].states[0].origin_x = 2;
    w->entities[0].states[0].origin_y = 3;
    w->entities[0].states[0].frames[0].parts[0].dx = 4;
    w->entities[0].states[0].frames[0].parts[0].dy = 5;
    inst = r01_world_place_entity(w, type_id, 40, 50);
    EXPECT(inst == 0, "instance");

    /* Legacy: art sitting on reserved tile 1 must relocate on export. */
    {
        R01EntityPart *pt;
        memset(tile, 0, sizeof(tile));
        tile[0] = 0x11;
        tile[8] = 0x22;
        EXPECT(r01_chr_write_spr_tile(w, bank, R01_SPR_PLAYER_TILE_ID, tile) == 0, "legacy tile1 art");
        type_id = r01_world_entity_add(w);
        EXPECT(type_id == 1, "legacy entity");
        pt = &w->entities[type_id].states[0].frames[0].parts[0];
        memset(pt, 0, sizeof(*pt));
        pt->bank = 0;
        pt->tile_id = R01_SPR_PLAYER_TILE_ID;
        w->entities[type_id].states[0].frames[0].part_count = 1;
        EXPECT(r01_world_place_entity(w, type_id, 10, 10) >= 0, "legacy inst");
    }

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
            EXPECT(fseek(f, prg_off + (long)PRG_PLAY_SPAWN_C, SEEK_SET) == 0, "seek prg spawn");
            EXPECT(fread(prg_spawn, 1, 2, f) == 2, "read prg spawn");
            EXPECT(prg_spawn[0] == 2, "prg spawn col matches default screen");
            EXPECT(prg_spawn[1] == 0, "prg spawn row matches default screen");

            EXPECT(fseek(f, 0, SEEK_END) == 0, "seek end");
            flen = ftell(f);
            EXPECT(flen > 0, "cart size");
            img = (uint8_t *)malloc((size_t)flen);
            EXPECT(img != NULL, "cart buf");
            if (img) {
                uint8_t ptrs[24];
                uint8_t slot[8];
                uint8_t hdr[WORLD_HDR_SIZE];
                uint32_t off_wtable, world_base, off_chr, off_types, off_insts;
                uint8_t type_n, inst_n;
                uint8_t trec[R01_CART_ENTITY_TYPE_SIZE];
                uint8_t irec[R01_CART_INSTANCE_SIZE];
                uint8_t spr_tile2[R01_TILE_BYTES];

                EXPECT(fseek(f, 0, SEEK_SET) == 0, "rewind");
                EXPECT(fread(img, 1, (size_t)flen, f) == (size_t)flen, "read cart");
                memcpy(ptrs, img + CART_HDR_SIZE, 24);
                off_wtable = rd_u24(ptrs + 18);
                memcpy(slot, img + off_wtable, 8);
                EXPECT(slot[0] != 0, "world0 present");
                world_base = rd_u24(slot + 2);
                memcpy(hdr, img + world_base, WORLD_HDR_SIZE);
                type_n = hdr[R01_CART_WHDR_TYPE_COUNT];
                inst_n = hdr[R01_CART_WHDR_INST_COUNT];
                off_types = rd_u24(hdr + R01_CART_WHDR_OFF_TYPES);
                off_insts = rd_u24(hdr + R01_CART_WHDR_OFF_INSTS);
                EXPECT(type_n == 2, "type count");
                EXPECT(inst_n == 2, "inst count");

                off_chr = rd_u24(hdr + 8);
                /* SPR bank 0 starts after 4 BG banks. */
                {
                    uint32_t spr0 = world_base + off_chr + 4u * R01_CHR_BANK_BYTES;
                    uint8_t stub[R01_TILE_BYTES];
                    uint8_t relocated[R01_TILE_BYTES];
                    memcpy(spr_tile2, img + spr0 + 2u * R01_TILE_BYTES, R01_TILE_BYTES);
                    EXPECT(spr_tile2[0] == 0x3C && spr_tile2[8] == 0xC3, "real spr CHR tile 2");
                    memcpy(stub, img + spr0 + (size_t)R01_SPR_PLAYER_TILE_ID * R01_TILE_BYTES,
                           R01_TILE_BYTES);
                    EXPECT(stub[0] == 0xFF && stub[8] == 0x00, "player stub color-1 at tile 1");
                    /* Legacy tile-1 art relocated to next free slot (tile_count was 3 -> dest 3). */
                    memcpy(relocated, img + spr0 + 3u * R01_TILE_BYTES, R01_TILE_BYTES);
                    EXPECT(relocated[0] == 0x11 && relocated[8] == 0x22, "relocated tile1 art");
                }

                memcpy(trec, img + world_base + off_types, R01_CART_ENTITY_TYPE_SIZE);
                EXPECT(trec[0] == 2 && trec[1] == 3, "type origin");
                EXPECT(trec[2] == 1, "part count");
                EXPECT(trec[4] == 2, "part tile");
                EXPECT(trec[6] == 4 && trec[7] == 5, "part dx dy");

                memcpy(trec, img + world_base + off_types + R01_CART_ENTITY_TYPE_SIZE,
                       R01_CART_ENTITY_TYPE_SIZE);
                EXPECT(trec[2] == 1, "legacy part count");
                EXPECT(trec[4] == 3, "legacy tile remapped off player stub");

                memcpy(irec, img + world_base + off_insts, R01_CART_INSTANCE_SIZE);
                EXPECT(irec[0] == 0, "inst type");
                EXPECT(rd_u16(irec + 2) == 40, "inst x");
                EXPECT(rd_u16(irec + 4) == 50, "inst y");
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

    free(p);
    TEST_EXIT();
}
