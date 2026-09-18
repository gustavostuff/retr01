#include "test_harness.h"

#include "retr01_studio/collision.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/play.h"
#include "retr01_studio/player_anim.h"
#include "retr01_studio/project.h"
#include "r01_custom_logic_scan.h"
#include "r01_play_anim.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void expect_camera(R01GameCtx *ctx, int line) {
    int expect_x = ctx->cam_x;
    int expect_y = ctx->cam_y;
    r01_play_camera_update(&expect_x, &expect_y, ctx->player_x, ctx->player_y, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H,
                           R01_SCREEN_PX_W, R01_SCREEN_PX_H, ctx->cam_deadzone_x, ctx->cam_deadzone_y,
                           ctx->cam_axis_lock);
    if (ctx->cam_x != expect_x || ctx->cam_y != expect_y) {
        fprintf(stderr, "FAIL line %d: camera got %d,%d expected %d,%d\n", line, ctx->cam_x, ctx->cam_y, expect_x,
                expect_y);
        exit(1);
    }
}

#define EXPECT_CAMERA(ctx) expect_camera((ctx), __LINE__)

static int test_floor_ok(void *user, int x, int y) {
    int floor = user ? *(const int *)user : 0;
    (void)x;
    return y >= 0 && y <= floor && x >= 0 && x < 200;
}

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
    EXPECT(r01_play_start(&pl, p, NULL), "play start");
    EXPECT(pl.active, "play active");
    EXPECT(pl.ctx.player_x == R01_PLAY_SPAWN_CENTER_X(2), "spawn center x on default screen");
    EXPECT(pl.ctx.player_y == R01_PLAY_SPAWN_CENTER_Y(0), "spawn center y on default screen");
    EXPECT_CAMERA(&pl.ctx);

    /* Marked player with a placed instance starts at that instance origin. */
    {
        int type_id, inst;
        type_id = r01_world_entity_add(&p->worlds[0]);
        EXPECT(type_id >= 0, "spawn entity type");
        r01_world_set_player_entity(&p->worlds[0], type_id);
        inst = r01_world_place_entity(&p->worlds[0], type_id, 90, 70);
        EXPECT(inst >= 0, "spawn instance");
        r01_play_stop(&pl);
        EXPECT(r01_play_start(&pl, p, NULL), "play start at instance");
        EXPECT(pl.ctx.player_x == 90 && pl.ctx.player_y == 70, "spawn at placed instance");
        r01_world_set_player_entity(&p->worlds[0], -1);
        r01_play_stop(&pl);
        EXPECT(r01_play_start(&pl, p, NULL), "play start fallback");
        EXPECT(pl.ctx.player_x == R01_PLAY_SPAWN_CENTER_X(2), "unmarked falls back to screen center");
    }

    r01_play_tick(&pl, p, 0, 0, 0);
    EXPECT_CAMERA(&pl.ctx);

    {
        int before = pl.ctx.player_x;
        int before_cam_x = pl.ctx.cam_x;
        r01_play_tick(&pl, p, 1, 0, 0);
        EXPECT(pl.ctx.player_x == before + 1, "move right");
        EXPECT_CAMERA(&pl.ctx);
        (void)before_cam_x;
    }

    /* Solid under player blocks any move (all four AABB corners share the tile). */
    s = &p->worlds[0].screens[p->worlds[0].default_screen];
    {
        int lx = pl.ctx.player_x % R01_SCREEN_PX_W;
        int ly = pl.ctx.player_y % R01_SCREEN_PX_H;
        int cell = (ly / 8) * R01_SCREEN_TILES_X + (lx / 8);
        int before_x = pl.ctx.player_x;
        s->attrs[cell] |= R01_ATTR_SOLID;
        r01_play_tick(&pl, p, -1, 0, 0);
        EXPECT(pl.ctx.player_x == before_x, "solid tile blocks movement");
        r01_play_tick(&pl, p, 1, 0, 0);
        EXPECT(pl.ctx.player_x == before_x, "solid tile blocks movement both axes");
        s->attrs[cell] &= (uint8_t)~R01_ATTR_SOLID;
    }

    EXPECT(!r01_play_button(&pl, p, R01_PLAY_BTN_X), "X has no warp");
    EXPECT(!r01_play_button(&pl, p, R01_PLAY_BTN_Y), "Y has no warp");

    EXPECT(r01_play_screen_index(&pl, &p->worlds[0]) == p->worlds[0].default_screen,
           "play_screen_index stays on spawn screen");

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
        pl.ctx.player_x = edge_x;
        pl.ctx.player_y = R01_PLAY_SPAWN_CENTER_Y(0);
        r01_play_tick(&pl, p, 1, 0, 0);
        EXPECT(pl.ctx.player_x == edge_x, "solid seam blocks move into next screen");
    }

    /* Missing screen also blocks AABB. */
    {
        int before = pl.ctx.player_x;
        int miss_idx = r01_world_find_screen(&p->worlds[0], 4, 0);
        int edge4 = 4 * R01_SCREEN_PX_W - R01_PLAY_PLAYER_W;
        EXPECT(miss_idx >= 0, "grid slot for col 4");
        p->worlds[0].screens[miss_idx].present = 0;
        pl.ctx.player_x = edge4;
        r01_play_tick(&pl, p, 1, 0, 0);
        EXPECT(pl.ctx.player_x == edge4, "missing screen blocks move");
        pl.ctx.player_x = before;
    }

    /* Marked player hitbox is offset from the Play origin. */
    {
        int type_id;
        int hx, hy, hw, hh;
        int cell;
        R01Screen *scr;
        type_id = r01_world_entity_add(&p->worlds[0]);
        EXPECT(type_id >= 0, "hitbox entity");
        p->worlds[0].entities[type_id].states[0].frames[0].origin_x = 4;
        p->worlds[0].entities[type_id].states[0].frames[0].origin_y = 4;
        p->worlds[0].entities[type_id].states[0].hitbox_x = 0;
        p->worlds[0].entities[type_id].states[0].hitbox_y = 0;
        p->worlds[0].entities[type_id].states[0].hitbox_w = 8;
        p->worlds[0].entities[type_id].states[0].hitbox_h = 8;
        r01_world_set_player_entity(&p->worlds[0], type_id);
        pl.ctx.player_x = R01_PLAY_SPAWN_CENTER_X(0);
        pl.ctx.player_y = R01_PLAY_SPAWN_CENTER_Y(0);
        r01_play_player_hit_rect(&p->worlds[0], &pl.ctx, pl.ctx.player_x, pl.ctx.player_y, &hx, &hy, &hw, &hh);
        EXPECT(hx == pl.ctx.player_x - 4 && hy == pl.ctx.player_y - 4, "hitbox offset from origin");
        EXPECT(hw == 8 && hh == 8, "hitbox size");
        {
            R01EntityType *ent = &p->worlds[0].entities[type_id];
            R01EntityFrame *fr1;
            ent->states[0].frames[0].part_count = 1;
            fr1 = r01_entity_ensure_frame(ent, 0, 1);
            EXPECT(fr1 != NULL, "walk frame");
            fr1->part_count = 1;
            fr1->origin_x = 20;
            fr1->origin_y = 20;
            pl.ctx.player_anim_frame = 1;
            r01_play_player_hit_rect(&p->worlds[0], &pl.ctx, pl.ctx.player_x, pl.ctx.player_y, &hx, &hy, &hw,
                                     &hh);
            EXPECT(hx == pl.ctx.player_x - 4 && hy == pl.ctx.player_y - 4,
                   "later frame origin does not move hitbox");
            pl.ctx.player_anim_frame = 0;
        }
        scr = &p->worlds[0].screens[r01_world_find_screen(&p->worlds[0], 0, 0)];
        cell = ((hy % R01_SCREEN_PX_H) / 8) * R01_SCREEN_TILES_X + ((hx % R01_SCREEN_PX_W) / 8);
        scr->attrs[cell] |= R01_ATTR_SOLID;
        {
            int before = pl.ctx.player_x;
            r01_play_tick(&pl, p, -1, 0, 0);
            EXPECT(pl.ctx.player_x == before, "offset hitbox blocks via solid under box");
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
        inst = r01_world_place_entity(&p->worlds[0], type_id, pl.ctx.cam_x - 64, pl.ctx.cam_y);
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

    {
        R01PlayPhysics ph;
        int x = 10;
        int y = 10;
        int floor = 40;
        int anim_dx = 0;
        int anim_dy = 0;
        int i;
        int jump_y;
        r01_play_physics_init(&ph);
        r01_play_physics_set_mode(&ph, R01_GAME_MODE_PLATFORMER);
        r01_play_physics_set_gravity(&ph, 1);
        r01_play_physics_set_jump(&ph, 8);
        r01_play_physics_set_meter(&ph, 16);
        for (i = 0; i < 80; i++) {
            r01_play_physics_tick(&ph, &x, &y, 0, 0, 0, test_floor_ok, &floor, &anim_dx, &anim_dy);
        }
        EXPECT(y == floor, "fall lands on floor");
        EXPECT(ph.grounded, "grounded after land");
        EXPECT(anim_dy == 0, "platformer anim ignores vertical");
        r01_play_physics_tick(&ph, &x, &y, 0, -1, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
        jump_y = y;
        EXPECT(jump_y < floor, "jump leaves floor");
        EXPECT(!ph.grounded, "airborne after jump");
        r01_play_physics_tick(&ph, &x, &y, 0, -1, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
        EXPECT(y <= jump_y, "held jump does not re-boost in air");
    }

    {
        R01PlayAnimCtx anim;
        r01_play_anim_init(&anim);
        r01_play_anim_set_crouch_state(&anim, 2);
        r01_play_anim_set_crouching(&anim, 1);
        r01_play_anim_update(&anim, 0, 0);
        EXPECT(r01_play_anim_entity_state(&anim) == 2, "crouch pose");
        r01_play_anim_set_crouching(&anim, 0);
        r01_play_anim_update(&anim, 0, 0);
        EXPECT(r01_play_anim_entity_state(&anim) == 0, "release crouch to idle");
    }

    {
        R01PlayPhysics hold;
        R01PlayPhysics tap;
        int hx = 10;
        int hy = 10;
        int tx = 10;
        int ty = 10;
        int floor = 80;
        int anim_dx = 0;
        int anim_dy = 0;
        int i;
        int hold_peak;
        int tap_peak;
        r01_play_physics_init(&hold);
        r01_play_physics_set_mode(&hold, R01_GAME_MODE_PLATFORMER);
        r01_play_physics_set_gravity(&hold, 1);
        r01_play_physics_set_jump(&hold, 8);
        r01_play_physics_set_meter(&hold, 16);
        r01_play_physics_init(&tap);
        r01_play_physics_set_mode(&tap, R01_GAME_MODE_PLATFORMER);
        r01_play_physics_set_gravity(&tap, 1);
        r01_play_physics_set_jump(&tap, 8);
        r01_play_physics_set_meter(&tap, 16);
        for (i = 0; i < 120; i++) {
            r01_play_physics_tick(&hold, &hx, &hy, 0, 0, 0, test_floor_ok, &floor, &anim_dx, &anim_dy);
            r01_play_physics_tick(&tap, &tx, &ty, 0, 0, 0, test_floor_ok, &floor, &anim_dx, &anim_dy);
        }
        r01_play_physics_tick(&hold, &hx, &hy, 0, 0, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
        r01_play_physics_tick(&tap, &tx, &ty, 0, 0, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
        hold_peak = hy;
        tap_peak = ty;
        for (i = 0; i < 20; i++) {
            r01_play_physics_tick(&hold, &hx, &hy, 0, 0, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
            if (hy < hold_peak) {
                hold_peak = hy;
            }
            r01_play_physics_tick(&tap, &tx, &ty, 0, 0, 0, test_floor_ok, &floor, &anim_dx, &anim_dy);
            if (ty < tap_peak) {
                tap_peak = ty;
            }
        }
        EXPECT(tap_peak > hold_peak, "release while rising peaks lower");
    }

    {
        R01PlayPhysics big;
        R01PlayPhysics small;
        int bx = 10;
        int by = 10;
        int sx = 10;
        int sy = 10;
        int floor = 40;
        int anim_dx = 0;
        int anim_dy = 0;
        int i;
        r01_play_physics_init(&big);
        r01_play_physics_set_mode(&big, R01_GAME_MODE_PLATFORMER);
        r01_play_physics_set_gravity(&big, 1);
        r01_play_physics_set_jump(&big, 8);
        r01_play_physics_set_meter(&big, 16);
        r01_play_physics_init(&small);
        r01_play_physics_set_mode(&small, R01_GAME_MODE_PLATFORMER);
        r01_play_physics_set_gravity(&small, 1);
        r01_play_physics_set_jump(&small, 8);
        r01_play_physics_set_meter(&small, 8);
        for (i = 0; i < 80; i++) {
            r01_play_physics_tick(&big, &bx, &by, 0, 0, 0, test_floor_ok, &floor, &anim_dx, &anim_dy);
            r01_play_physics_tick(&small, &sx, &sy, 0, 0, 0, test_floor_ok, &floor, &anim_dx, &anim_dy);
        }
        r01_play_physics_tick(&big, &bx, &by, 0, 0, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
        r01_play_physics_tick(&small, &sx, &sy, 0, 0, 1, test_floor_ok, &floor, &anim_dx, &anim_dy);
        EXPECT((floor - by) > (floor - sy), "smaller meter jumps fewer pixels");
    }

    {
        FILE *f = fopen("plat_logic.c", "w");
        int mode = 0;
        int grav = 0;
        int jump = 0;
        int meter = 0;
        EXPECT(f != NULL, "write plat_logic");
        if (f) {
            fputs("void r01_custom_on_init(R01GameCtx *ctx) {\n"
                  "    r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);\n"
                  "    r01_platformer_set_gravity(ctx, 2);\n"
                  "    r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);\n"
                  "    r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);\n"
                  "    r01_player_anim_set_crouch_state(ctx, 2);\n"
                  "}\n",
                  f);
            fclose(f);
        }
        EXPECT(r01_custom_logic_scan_game_mode("plat_logic.c", &mode) == 0 && mode == 1, "scan platformer");
        EXPECT(r01_custom_logic_scan_plat_gravity("plat_logic.c", &grav) == 0 && grav == 2, "scan gravity");
        EXPECT(r01_custom_logic_scan_plat_jump("plat_logic.c", &jump) == 0 && jump == 0,
               "scan jump default token is engine default");
        EXPECT(r01_custom_logic_scan_plat_meter("plat_logic.c", &meter) == 0 && meter == 0,
               "scan meter default token is engine default");
        {
            int crouch = -1;
            EXPECT(r01_custom_logic_scan_plat_crouch("plat_logic.c", &crouch) == 0 && crouch == 2, "scan crouch state");
        }
        remove("plat_logic.c");
    }

    /* Play tick: platformer falls onto a solid row. Y jumps. Up/Down do not walk. */
    EXPECT(r01_play_start(&pl, p, NULL), "play start for platformer");
    r01_game_set_mode(&pl.ctx, R01_GAME_MODE_PLATFORMER);
    r01_platformer_set_gravity(&pl.ctx, 1);
    r01_platformer_set_jump(&pl.ctx, 8);
    r01_platformer_set_meter(&pl.ctx, 16);
    {
        int pe = r01_world_entity_add(&p->worlds[0]);
        R01EntityType *ent;
        EXPECT(pe >= 0, "crouch player type");
        r01_world_set_player_entity(&p->worlds[0], pe);
        ent = &p->worlds[0].entities[pe];
        r01_entity_ensure_state(ent, 2);
        snprintf(ent->states[2].name, sizeof(ent->states[2].name), "crouching");
        r01_player_anim_set_crouch_state(&pl.ctx, 2);
    }
    {
        int si = p->worlds[0].default_screen;
        R01Screen *scr;
        int tx;
        int before_y;
        int after_up;
        EXPECT(si >= 0 && si < p->worlds[0].screen_count, "default screen");
        scr = &p->worlds[0].screens[si];
        pl.ctx.player_x = scr->col * R01_SCREEN_PX_W + 16;
        pl.ctx.player_y = scr->row * R01_SCREEN_PX_H + 8;
        pl.ctx.plat_vel_y = 0;
        pl.ctx.plat_frac_x = 0;
        pl.ctx.plat_frac_y = 0;
        pl.ctx.plat_grounded = 0;
        pl.ctx.plat_jump_held = 0;
        for (tx = 0; tx < R01_SCREEN_TILES_X; tx++) {
            scr->attrs[2 * R01_SCREEN_TILES_X + tx] |= R01_ATTR_SOLID;
        }
        {
            int i;
            for (i = 0; i < 40; i++) {
                r01_play_tick(&pl, p, 0, 0, 0);
            }
        }
        before_y = pl.ctx.player_y;
        EXPECT(pl.ctx.plat_grounded, "play tick grounded on solid row");
        r01_play_tick(&pl, p, 0, -1, 0);
        EXPECT(pl.ctx.player_y == before_y, "Up does not jump in platformer");
        r01_play_tick(&pl, p, 0, 1, 0);
        EXPECT(pl.ctx.player_y == before_y, "Down crouch stays grounded");
        EXPECT(r01_player_anim_entity_state(&pl.ctx) == 2, "Down crouches in platformer");
        r01_play_tick(&pl, p, -1, 1, 0);
        EXPECT(pl.ctx.player_x == scr->col * R01_SCREEN_PX_W + 16, "crouch does not walk");
        r01_play_tick(&pl, p, 0, 0, 0);
        EXPECT(r01_player_anim_entity_state(&pl.ctx) == 0, "release Down returns idle");
        r01_play_tick(&pl, p, 0, 0, 1);
        after_up = pl.ctx.player_y;
        EXPECT(after_up < before_y, "Y jumps in platformer");
        r01_play_tick(&pl, p, 0, 1, 0);
        EXPECT(pl.ctx.player_y <= after_up, "Down does not walk in platformer");
    }

    r01_play_stop(&pl);
    EXPECT(!pl.active, "play stopped after platformer");

    free(p);
    TEST_EXIT();
}
