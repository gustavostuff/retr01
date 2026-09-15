#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

#include "retr01_studio/project.h"
#include "retr01_studio/warps.h"

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "warp_test");
    w = r01_project_active_world(p);
    EXPECT(w != NULL, "active world");

    {
        int idx = r01_world_warp_entrance_add(w, 1, 0, 5, 3);
        EXPECT(idx == 0, "first entrance index");
        EXPECT(strcmp(w->warp_entrances[0].id, "w_00") == 0, "autogen id");
        EXPECT(w->warp_entrances[0].screen_col == 1 && w->warp_entrances[0].tile_col == 5, "entrance pos");
    }
    {
        int ex = r01_world_warp_exit_set(w, 0, 2, 1, 10, 8, R01_WARP_FADE_OUT | R01_WARP_FADE_IN);
        EXPECT(ex == 0, "exit slot");
        EXPECT(w->warp_exits[0].dest_screen_col == 2, "exit dest col");
    }
    {
        int wx, wy;
        r01_world_warp_tile_world_pos(2, 1, 10, 8, &wx, &wy);
        EXPECT(wx == 2 * R01_SCREEN_PX_W + 80 && wy == 1 * R01_SCREEN_PX_H + 64, "tile world pos");
    }
    {
        int hit = r01_world_warp_entrance_hit(w, 1 * R01_SCREEN_PX_W + 40, 24, 8, 8);
        EXPECT(hit == 0, "player overlaps entrance tile");
    }

    free(p);
    TEST_EXIT();
}
