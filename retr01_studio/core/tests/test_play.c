#include "test_harness.h"

#include "retr01_studio/collision.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01PlayState pl;
    R01Screen *s;
    int i;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "test");
    for (i = 0; i < p->worlds[0].screen_count; i++) {
        p->worlds[0].screens[i].present = 1;
    }

    p->worlds[0].default_screen = 2;
    r01_project_begin_play(p);
    EXPECT(r01_play_start(&pl, p), "play start");
    EXPECT(pl.active, "play active");
    EXPECT(pl.player_x == R01_PLAY_SPAWN_CENTER_X(2), "spawn center x on default screen");
    EXPECT(pl.player_y == R01_PLAY_SPAWN_CENTER_Y(0), "spawn center y on default screen");
    EXPECT(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2 &&
               pl.cam_y == pl.player_y + R01_PLAY_PLAYER_H / 2 - R01_SCREEN_PX_H / 2,
           "camera centers on spawn");

    r01_play_tick(&pl, p, 0, 0);
    EXPECT(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2, "idle keeps camera follow");

    {
        int before = pl.player_x;
        r01_play_tick(&pl, p, 1, 0);
        EXPECT(pl.player_x == before + 1, "move right");
        EXPECT(pl.cam_x == pl.player_x + R01_PLAY_PLAYER_W / 2 - R01_SCREEN_PX_W / 2, "camera follows move");
    }

    /* Solid under player blocks any move (all four AABB corners share the tile). */
    s = &p->worlds[0].screens[p->worlds[0].default_screen];
    {
        int lx = pl.player_x % R01_SCREEN_PX_W;
        int ly = pl.player_y % R01_SCREEN_PX_H;
        int cell = (ly / 8) * R01_SCREEN_TILES_X + (lx / 8);
        int before_x = pl.player_x;
        s->attrs[cell] |= R01_ATTR_SOLID;
        r01_play_tick(&pl, p, -1, 0);
        EXPECT(pl.player_x == before_x, "solid tile blocks movement");
        r01_play_tick(&pl, p, 1, 0);
        EXPECT(pl.player_x == before_x, "solid tile blocks movement both axes");
        s->attrs[cell] &= (uint8_t)~R01_ATTR_SOLID;
    }

    EXPECT(r01_play_button(&pl, p, R01_PLAY_BTN_X), "warp X");
    EXPECT(pl.player_x == R01_PLAY_SPAWN_CENTER_X(0), "warp to col 0 center");
    EXPECT(pl.player_y == R01_PLAY_SPAWN_CENTER_Y(0), "warp to row 0 center");

    EXPECT(r01_play_screen_index(&pl, &p->worlds[0]) == r01_world_find_screen(&p->worlds[0], 0, 0),
           "play_screen_index matches warp cell");

    {
        uint8_t r = 0, g = 0, b = 0;
        EXPECT(r01_play_sample_bg(p, &pl, 0, 0, &r, &g, &b) == 0, "sample bg in view");
    }

    {
        uint8_t hw = r01_attr_hw(r01_attr_pack(1, 2, 0, 1));
        int touched;
        s = &p->worlds[0].screens[p->worlds[0].default_screen];
        s->attrs[0] = r01_attr_pack(1, 2, 0, 1);
        s->attrs[1] = r01_attr_pack(1, 2, 0, 1);
        s->attrs[2] = r01_attr_pack(0, 0, 0, 0);
        touched = r01_world_apply_solid_hw(&p->worlds[0], hw, 1);
        EXPECT(touched >= 2, "solid by hw touches matching tiles");
        EXPECT(r01_attr_solid(s->attrs[0]) && r01_attr_solid(s->attrs[1]), "matching attrs solid");
        EXPECT(!r01_attr_solid(s->attrs[2]), "non-matching attrs unchanged");
    }

    /* Seam: solid on neighboring screen blocks crossing the edge. */
    {
        int right_idx;
        R01Screen *right;
        int edge_x;
        int ti;
        EXPECT(r01_world_create_screen(&p->worlds[0], 3, 0) >= 0, "create right neighbor");
        right_idx = r01_world_find_screen(&p->worlds[0], 3, 0);
        EXPECT(right_idx >= 0, "right neighbor screen present");
        right = &p->worlds[0].screens[right_idx];
        memset(right->attrs, 0, sizeof(right->attrs));
        for (ti = 0; ti < R01_SCREEN_TILES_Y; ti++) {
            right->attrs[ti * R01_SCREEN_TILES_X] |= R01_ATTR_SOLID;
        }
        edge_x = 3 * R01_SCREEN_PX_W - R01_PLAY_PLAYER_W;
        pl.player_x = edge_x;
        pl.player_y = R01_PLAY_SPAWN_CENTER_Y(0);
        r01_play_tick(&pl, p, 1, 0);
        EXPECT(pl.player_x == edge_x, "solid seam blocks move into next screen");
    }

    /* Missing screen also blocks AABB. */
    {
        int before = pl.player_x;
        int miss_idx = r01_world_find_screen(&p->worlds[0], 4, 0);
        int edge4 = 4 * R01_SCREEN_PX_W - R01_PLAY_PLAYER_W;
        EXPECT(miss_idx >= 0, "grid slot for col 4");
        p->worlds[0].screens[miss_idx].present = 0;
        pl.player_x = edge4;
        r01_play_tick(&pl, p, 1, 0);
        EXPECT(pl.player_x == edge4, "missing screen blocks move");
        pl.player_x = before;
    }

    /* Marked player hitbox is offset from the Play origin. */
    {
        int type_id;
        int hx, hy, hw, hh;
        int cell;
        R01Screen *scr;
        type_id = r01_world_entity_add(&p->worlds[0]);
        EXPECT(type_id >= 0, "hitbox entity");
        p->worlds[0].entities[type_id].states[0].origin_x = 4;
        p->worlds[0].entities[type_id].states[0].origin_y = 4;
        p->worlds[0].entities[type_id].states[0].hitbox_x = 0;
        p->worlds[0].entities[type_id].states[0].hitbox_y = 0;
        p->worlds[0].entities[type_id].states[0].hitbox_w = 8;
        p->worlds[0].entities[type_id].states[0].hitbox_h = 8;
        r01_world_set_player_entity(&p->worlds[0], type_id);
        pl.player_x = R01_PLAY_SPAWN_CENTER_X(0);
        pl.player_y = R01_PLAY_SPAWN_CENTER_Y(0);
        r01_play_player_hit_rect(&p->worlds[0], pl.player_x, pl.player_y, &hx, &hy, &hw, &hh);
        EXPECT(hx == pl.player_x - 4 && hy == pl.player_y - 4, "hitbox offset from origin");
        EXPECT(hw == 8 && hh == 8, "hitbox size");
        scr = &p->worlds[0].screens[r01_world_find_screen(&p->worlds[0], 0, 0)];
        cell = ((hy % R01_SCREEN_PX_H) / 8) * R01_SCREEN_TILES_X + ((hx % R01_SCREEN_PX_W) / 8);
        scr->attrs[cell] |= R01_ATTR_SOLID;
        {
            int before = pl.player_x;
            r01_play_tick(&pl, p, -1, 0);
            EXPECT(pl.player_x == before, "offset hitbox blocks via solid under box");
        }
        scr->attrs[cell] &= (uint8_t)~R01_ATTR_SOLID;
        r01_world_set_player_entity(&p->worlds[0], -1);
    }

    EXPECT(r01_oam_tile_off_screen(-8, 0), "oam fully left off");
    EXPECT(r01_oam_tile_off_screen(128, 0), "oam fully right off");
    EXPECT(r01_oam_tile_off_screen(0, -8), "oam fully above off");
    EXPECT(r01_oam_tile_off_screen(0, 120), "oam fully below off");
    EXPECT(!r01_oam_tile_off_screen(-4, 10), "oam partial left on");
    EXPECT(!r01_oam_tile_off_screen(120, 10), "oam partial right on");

    {
        R01OamEntry oam[R01_OAM_MAX];
        int n;
        int type_id, inst;
        type_id = r01_world_entity_add(&p->worlds[0]);
        EXPECT(type_id >= 0, "oam entity type");
        p->worlds[0].entities[type_id].states[0].frames[0].part_count = 1;
        p->worlds[0].entities[type_id].states[0].frames[0].parts[0].tile_id = 2;
        inst = r01_world_place_entity(&p->worlds[0], type_id, pl.cam_x - 64, pl.cam_y);
        EXPECT(inst >= 0, "oam far instance");
        n = r01_play_build_oam(p, &pl, oam, R01_OAM_MAX);
        EXPECT(n >= 1, "oam has player");
        /* Far entity should be skipped as fully off-screen. */
        {
            int i, found = 0;
            for (i = 1; i < n; i++) {
                if (oam[i].tile_id == 2) {
                    found = 1;
                }
            }
            EXPECT(!found, "fully off-screen entity omitted from OAM");
        }
    }

    r01_play_stop(&pl);
    EXPECT(!pl.active, "play stopped");

    free(p);
    TEST_EXIT();
}
