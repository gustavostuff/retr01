#include "test_harness.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/project.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;
    R01Screen *s;
    uint8_t tile[R01_TILE_BYTES];
    uint8_t tmp[R01_TILE_BYTES];
    int id;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "test");
    w = &p->worlds[0];
    s = &w->screens[2];

    memset(tile, 0, sizeof(tile));
    r01_tile_set_pixel(tile, 3, 4, 2);
    EXPECT(r01_tile_pixel_color(tile, 3, 4) == 2, "tile set/get pixel");

    r01_tile_orient(tile, 1, 0, tmp);
    r01_tile_orient(tmp, 1, 0, tile);
    EXPECT(r01_tile_pixel_color(tile, 3, 4) == 2, "flip_h is self-inverse");

    r01_tile_orient(tile, 0, 1, tmp);
    r01_tile_orient(tmp, 0, 1, tile);
    EXPECT(r01_tile_pixel_color(tile, 3, 4) == 2, "flip_v is self-inverse");

    id = r01_chr_alloc_tile(w, 0);
    EXPECT(id >= 0, "alloc tile");
    EXPECT(r01_chr_write_tile(w, 0, id, tile) == 0, "write tile");

    r01_screen_paint_tile(w, s, 1, 1, (uint8_t)id, r01_attr_pack(0, 1, 1, 0));
    EXPECT(s->tiles[1 * R01_SCREEN_TILES_X + 1] == (uint8_t)id, "paint sets tile id");
    EXPECT(r01_attr_bank(s->attrs[1 * R01_SCREEN_TILES_X + 1]) == 0, "paint sets attr bank");
    EXPECT(r01_attr_pal(s->attrs[1 * R01_SCREEN_TILES_X + 1]) == 1, "paint sets attr pal");
    EXPECT(r01_attr_flip_h(s->attrs[1 * R01_SCREEN_TILES_X + 1]), "paint sets flip_h");

    r01_screen_fill_pixels_from_bank(w, s);
    EXPECT(s->pixels[0] <= 3, "fill pixels produces palette indices");

    free(p);
    TEST_EXIT();
}
