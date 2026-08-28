#include "test_harness.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
    uint8_t tile[R01_TILE_BYTES];
    char err[128];
    int bank, id, cat;

    EXPECT(p != NULL && p2 != NULL, "alloc");
    if (!p || !p2) {
        free(p);
        free(p2);
        return 1;
    }

    r01_project_init(p, "sprites");
    EXPECT(r01_chr_find_spr_bank_space(&p->worlds[0]) == 0, "prefer bank 0");
    bank = 0;
    id = r01_chr_alloc_spr_tile(&p->worlds[0], bank);
    EXPECT(id == 0, "first spr tile id 0");
    memset(tile, 0, sizeof(tile));
    tile[0] = 0x81;
    tile[8] = 0x42;
    EXPECT(r01_chr_write_spr_tile(&p->worlds[0], bank, id, tile) == 0, "write spr tile");
    cat = r01_world_sprite_add(&p->worlds[0], bank, id, 2);
    EXPECT(cat == 0, "catalog add");
    EXPECT(p->worlds[0].sprite_count == 1, "sprite count");
    EXPECT(p->worlds[0].sprites[0].pal == 2, "default pal");

    EXPECT(r01_world_sprite_set_pal(&p->worlds[0], 0, 3) == 0, "set pal");
    EXPECT(p->worlds[0].sprites[0].pal == 3, "pal updated");

    /* Fill bank 0 to force bank 1. */
    while (r01_chr_alloc_spr_tile(&p->worlds[0], 0) >= 0) {
    }
    EXPECT(r01_chr_find_spr_bank_space(&p->worlds[0]) == 1, "overflow to bank 1");

    EXPECT(r01_project_save_json(p, "test_sprites.r01proj", err, sizeof(err)) == 0, "save");
    EXPECT(r01_project_load_json(p2, "test_sprites.r01proj", err, sizeof(err)) == 0, "load");
    EXPECT(p2->worlds[0].sprite_count == 1, "catalog roundtrip count");
    EXPECT(p2->worlds[0].sprites[0].bank == 0, "catalog bank");
    EXPECT(p2->worlds[0].sprites[0].tile_id == 0, "catalog tile");
    EXPECT(p2->worlds[0].sprites[0].pal == 3, "catalog pal");
    EXPECT(p2->worlds[0].spr_banks[0].tile_count == R01_TILES_PER_BANK, "spr bank0 tiles");
    EXPECT(memcmp(p2->worlds[0].spr_banks[0].chr, tile, R01_TILE_BYTES) == 0, "spr chr bytes");

    EXPECT(r01_world_sprite_remove(&p2->worlds[0], 0) == 0, "remove");
    EXPECT(p2->worlds[0].sprite_count == 0, "empty after remove");

    free(p);
    free(p2);
    TEST_EXIT();
}
