#include "test_harness.h"

#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/chr_pack.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    uint8_t r, g, b;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "palette");

    r01_kit_rgb(0, &r, &g, &b);
    EXPECT(r == 0 && g == 0 && b == 0, "kit index 0 is black");
    r01_kit_rgb(48, &r, &g, &b);
    EXPECT(r == 0xFF && g == 0xFF && b == 0xFF, "kit index 48 is white");

    EXPECT(r01_kit_nearest_master(0, 0, 0) == 0, "nearest black");
    EXPECT(r01_kit_nearest_master(255, 255, 255) == 48, "nearest white");

    r01_project_player_rgb(p, &r, &g, &b);
    EXPECT(r > 0 || g > 0 || b > 0, "player rgb non-zero");

    {
        const R01World *w = &p->worlds[0];
        const R01Screen *s = &w->screens[2];
        uint8_t pr, pg, pb;
        r01_screen_pixel_rgb(p, w, s, 0, 0, &pr, &pg, &pb);
        (void)pr;
        (void)pg;
        (void)pb;
    }

    r01_project_backdrop_rgb(p, &p->worlds[0], &r, &g, &b);
    (void)r;
    (void)g;
    (void)b;

    {
        R01World *w = &p->worlds[0];
        R01Screen *bg1 = &w->screens[2];
        R01Screen bg0;
        uint8_t c0r, c0g, c0b, br, bg, bb, cr, cg, cb;
        int cell = 0;
        int id_blank;
        int id_fill;
        uint8_t tblank[R01_TILE_BYTES];
        uint8_t tfill[R01_TILE_BYTES];
        memset(&bg0, 0, sizeof(bg0));
        bg0.present = 1;
        bg0.col = bg1->col;
        bg0.row = bg1->row;
        memset(tblank, 0, sizeof(tblank));
        memset(tfill, 0, sizeof(tfill));
        r01_tile_set_pixel(tfill, 0, 0, 1);
        id_blank = r01_chr_alloc_tile(w, 0);
        id_fill = r01_chr_alloc_tile(w, 0);
        EXPECT(id_blank >= 0 && id_fill >= 0, "alloc blank+fill tiles");
        r01_chr_write_tile(w, 0, id_blank, tblank);
        r01_chr_write_tile(w, 0, id_fill, tfill);
        bg1->tiles[cell] = (uint8_t)id_blank;
        bg1->attrs[cell] = r01_attr_pack(0, 0, 0, 0);
        bg0.tiles[cell] = (uint8_t)id_fill;
        bg0.attrs[cell] = r01_attr_pack(0, 0, 0, 0);
        EXPECT(r01_screen_pixel_color(w, bg1, 0, 0) == 0, "BG1 transparent");
        EXPECT(r01_screen_pixel_color(w, &bg0, 0, 0) == 1, "BG0 opaque");
        r01_project_backdrop_rgb(p, w, &br, &bg, &bb);
        r01_compose_screen_pixel_rgb(p, w, bg1, &bg0, 0, 0, &cr, &cg, &cb);
        r01_screen_pixel_rgb(p, w, &bg0, 0, 0, &c0r, &c0g, &c0b);
        EXPECT(cr == c0r && cg == c0g && cb == c0b, "compose show-through uses BG0");
        r01_compose_screen_pixel_rgb(p, w, bg1, NULL, 0, 0, &cr, &cg, &cb);
        EXPECT(cr == br && cg == bg && cb == bb, "compose without BG0 uses backdrop");
    }

    free(p);
    TEST_EXIT();
}
