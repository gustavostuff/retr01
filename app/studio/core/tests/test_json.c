#include "test_harness.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];

    EXPECT(p != NULL && p2 != NULL, "alloc projects");
    if (!p || !p2) {
        free(p);
        free(p2);
        return 1;
    }

    r01_project_init(p, "roundtrip");
    p->default_world = 0;
    p->worlds[0].screens[9].present = 1;
    p->worlds[0].default_screen = 9;
    p->worlds[0].default_pal_row = 3;
    p->worlds[0].screens[2].attrs[0] = r01_attr_pack(0, 2, 1, 0);
    p->global_pal_bg[1][2].idx[1] = 42;

    EXPECT(r01_project_save_json(p, "test_roundtrip.r01proj", err, sizeof(err)) == 0, "save json");
    EXPECT(r01_project_load_json(p2, "test_roundtrip.r01proj", err, sizeof(err)) == 0, "load json");
    EXPECT(p2->worlds[0].default_screen == 9, "default_screen roundtrip");
    EXPECT(p2->worlds[0].default_pal_row == 3, "default_pal_row roundtrip");
    EXPECT(p2->global_pal_bg[1][2].idx[1] == 42, "palette roundtrip");
    EXPECT(p2->worlds[0].screens[2].attrs[0] == r01_attr_pack(0, 2, 1, 0), "tile attr roundtrip");
    EXPECT(p2->worlds[0].screen_count == R01_GRID_MAX * R01_GRID_MAX, "screen slot count roundtrip");
    EXPECT(p2->worlds[0].sprite_count == 0, "legacy empty sprites");

    /* v5 sprite catalog roundtrip */
    {
        uint8_t tile[R01_TILE_BYTES];
        int id = r01_chr_alloc_spr_tile(&p->worlds[0], 0);
        memset(tile, 0xA5, sizeof(tile));
        EXPECT(id >= 0, "alloc spr");
        EXPECT(r01_chr_write_spr_tile(&p->worlds[0], 0, id, tile) == 0, "write spr");
        EXPECT(r01_world_sprite_add(&p->worlds[0], 0, id, 1) == 0, "add sprite");
        EXPECT(r01_project_save_json(p, "test_roundtrip.r01proj", err, sizeof(err)) == 0, "save v5");
        EXPECT(r01_project_load_json(p2, "test_roundtrip.r01proj", err, sizeof(err)) == 0, "load v5");
        EXPECT(p2->worlds[0].sprite_count == 1, "sprite count v5");
        EXPECT(p2->worlds[0].sprites[0].pal == 1, "sprite pal v5");
    }

    free(p);
    free(p2);
    TEST_EXIT();
}
