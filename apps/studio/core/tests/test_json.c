#include "test_harness.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdio.h>
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
    /* Non-zero tile: empty cells force attr 0 on load (sanitize leftover pals). */
    p->worlds[0].screens[2].tiles[0] = 1;
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

    /* v14 BGM ticks were quarter notes. v15 doubles them to eighths. */
    {
        FILE *f = fopen("test_bgm_v14.r01proj", "w");
        EXPECT(f != NULL, "write v14 bgm");
        if (f) {
            fputs("{\n"
                  "  \"version\": 14,\n"
                  "  \"name\": \"v14bgm\",\n"
                  "  \"bgm\": {\n"
                  "    \"track_count\": 1,\n"
                  "    \"tracks\": [\n"
                  "      {\"name\": \"T\", \"channels\": [\n"
                  "        [{\"s\":1,\"l\":2,\"m\":60,\"t\":\"C4\"}],[],[],[],[]\n"
                  "      ]}\n"
                  "    ]\n"
                  "  }\n"
                  "}\n",
                  f);
            fclose(f);
        }
        EXPECT(r01_project_load_json(p2, "test_bgm_v14.r01proj", err, sizeof(err)) == 0, "load v14 bgm");
        EXPECT(p2->bgm.present == 1, "v14 bgm present");
        EXPECT(p2->bgm.region[0][0][0].start == 2, "v14 start scaled");
        EXPECT(p2->bgm.region[0][0][0].len == 4, "v14 len scaled");
        remove("test_bgm_v14.r01proj");
    }

    free(p);
    free(p2);
    TEST_EXIT();
}
