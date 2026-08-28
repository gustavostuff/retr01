#include "test_harness.h"

#include "retr01_studio/project.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "test");
    w = &p->worlds[0];

    EXPECT(w->screen_count == R01_GRID_MAX * R01_GRID_MAX, "8x8 screen slots");
    EXPECT(w->screens[2].col == 2 && w->screens[2].row == 0, "start screen 2,0");
    EXPECT(w->screens[0].present && w->screens[3].present == 0, "default 3x3 present region");
    EXPECT(p->active_screen == 2, "active start 2,0");
    EXPECT(r01_world_screen_index(w, 2, 0) == 2, "screen index 2,0");
    EXPECT(r01_world_screen_index(w, -1, 0) < 0, "invalid col rejected");
    EXPECT(r01_world_find_screen(w, 2, 0) == 2, "find screen 2,0");
    EXPECT(r01_world_find_screen(w, 7, 7) < 0, "missing screen not found");

    EXPECT(r01_world_default_screen(w) == 2 || r01_world_default_screen(w) >= 0, "default_screen valid");
    w->default_screen = 2;
    EXPECT(r01_world_default_screen(w) == 2, "explicit default_screen");

    r01_project_begin_play(p);
    EXPECT(w->default_screen == 2, "begin_play keeps default_screen");
    EXPECT(p->active_world == p->default_world, "begin_play uses default_world");

    EXPECT(r01_world_create_screen(w, 5, 5) >= 0, "create screen 5,5");
    EXPECT(r01_world_find_screen(w, 5, 5) >= 0, "created screen findable");
    EXPECT(r01_world_remove_screen(w, 5, 5) == 0, "remove screen 5,5");
    EXPECT(r01_world_find_screen(w, 5, 5) < 0, "removed screen gone");

    {
        int ds = w->default_screen;
        int dc = w->screens[ds].col;
        int dr = w->screens[ds].row;
        EXPECT(r01_world_remove_screen(w, dc, dr) == 0, "remove default screen cell");
        EXPECT(w->default_screen != ds || !w->screens[ds].present, "default_screen resynced after remove");
    }

    EXPECT(r01_project_set_active_world(p, 1) == 0, "activate world 1");
    EXPECT(p->active_world == 1, "active_world updated");
    EXPECT(p->worlds[1].present, "lazy world init");
    EXPECT(r01_project_set_active_world(p, R01_MAX_WORLDS - 1) == 0, "activate last world");
    EXPECT(p->active_world == R01_MAX_WORLDS - 1, "last world index");
    EXPECT(r01_project_set_active_world(p, R01_MAX_WORLDS) < 0, "reject world past max");

    {
        uint8_t merged = r01_attr_merge(r01_attr_pack(0, 0, 0, 0) | R01_ATTR_SOLID | R01_ATTR_ANIM, 2, 3, 1, 1);
        EXPECT(r01_attr_bank(merged) == 2, "attr_merge bank");
        EXPECT(r01_attr_pal(merged) == 3, "attr_merge pal");
        EXPECT(r01_attr_flip_h(merged), "attr_merge flip_h");
        EXPECT(r01_attr_solid(merged), "attr_merge keeps solid");
        EXPECT(r01_attr_anim(merged), "attr_merge keeps anim");
    }

    free(p);
    TEST_EXIT();
}
