#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

#include "retr01_studio/entities.h"
#include "retr01_studio/game_runtime.h"
#include "retr01_studio/player_anim.h"
#include "retr01_studio/project.h"
#include "r01_play_anim_cart.h"

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;
    R01GameCtx ctx;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }
    r01_project_init(p, "anim_test");
    w = r01_project_active_world(p);
    {
        int pe = r01_world_entity_add(p);
        R01EntityType *ent = &p->entities[pe];
        R01EntityPart part;
        int fi;
        memset(&part, 0, sizeof(part));
        part.tile_id = 1;
        r01_world_set_player_entity(p, pe);
        r01_entity_frame_add_part(&ent->states[0].frames[0], &part);
        ent->states[0].frame_count = 4;
        r01_entity_ensure_state(ent, 1);
        ent->states[1].frame_count = 3;
        for (fi = 0; fi < 3; fi++) {
            r01_entity_frame_add_part(&ent->states[1].frames[fi], &part);
            ent->states[1].frames[fi].delay = 2;
        }
    }
    r01_game_ctx_init(&ctx);
    r01_player_anim_update(&ctx, -1, 0);
    EXPECT(r01_player_anim_entity_state(&ctx) == 0, "unmapped walk stays state 0");
    EXPECT(r01_player_anim_frame(&ctx) == 0, "unmapped walk freezes frame 0");
    EXPECT(r01_player_anim_flip_h(&ctx) == 1, "unmapped still flips for facing");
    r01_player_anim_set_idle_state(&ctx, 0);
    r01_player_anim_set_walk_all(&ctx, 1);
    r01_entity_state_frame_delay_set(&ctx, 1, 2);

    r01_player_anim_update(&ctx, -1, 0);
    EXPECT(r01_player_anim_moving(&ctx), "left input walks");
    EXPECT(r01_player_anim_entity_state(&ctx) == 1, "walk state");
    EXPECT(r01_player_anim_flip_h(&ctx) == 1, "flip when walking left");

    r01_player_anim_update(&ctx, -1, -1);
    EXPECT(r01_player_anim_moving(&ctx), "diagonal still walking");
    EXPECT(r01_player_anim_entity_state(&ctx) == 1, "still walk state on diagonal");
    EXPECT(r01_player_anim_dir(&ctx) == R01_PLAYER_DIR_UP_LEFT, "up-left direction");

    r01_player_anim_update(&ctx, 0, 0);
    EXPECT(!r01_player_anim_moving(&ctx), "idle when stopped");
    EXPECT(r01_player_anim_entity_state(&ctx) == 0, "idle state");
    EXPECT(r01_player_anim_dir(&ctx) == R01_PLAYER_DIR_UP_LEFT, "idle keeps last facing");
    EXPECT(r01_player_anim_flip_h(&ctx) == 1, "idle keeps left flip after walk");

    r01_player_anim_set_crouch_state(&ctx, 2);
    r01_entity_ensure_state(&p->entities[r01_world_player_entity(p)], 2);
    ctx.player_crouching = 1;
    r01_player_anim_update(&ctx, 0, 0);
    EXPECT(r01_player_anim_entity_state(&ctx) == 2, "crouch state");
    EXPECT(!r01_player_anim_moving(&ctx), "crouch is not walk");
    ctx.player_crouching = 0;
    r01_player_anim_update(&ctx, 0, 0);
    EXPECT(r01_player_anim_entity_state(&ctx) == 0, "uncrouch to idle");

    r01_player_default_face_set(&ctx, R01_PLAYER_FACE_LEFT);
    EXPECT(r01_player_anim_flip_h(&ctx) == 1, "idle faces left with flip");

    {
        int pe = r01_world_player_entity(p);
        int i;
        for (i = 0; i < 12; i++) {
            r01_player_anim_update(&ctx, 0, 0);
            r01_player_anim_tick(&ctx, p, pe);
        }
        EXPECT(r01_player_anim_frame(&ctx) == 0, "idle skips empty frame slots");
    }

    {
        int pe = r01_world_player_entity(p);
        r01_player_anim_update(&ctx, 1, 0);
        r01_player_anim_tick(&ctx, p, pe);
        r01_player_anim_tick(&ctx, p, pe);
        EXPECT(r01_player_anim_frame(&ctx) == 1, "walk frames advance");
    }

    {
        int pe = r01_world_player_entity(p);
        r01_entity_ensure_state(&p->entities[pe], 3);
        r01_player_anim_set_jump_state(&ctx, 3);
        ctx.player_airborne = 1;
        r01_player_anim_update(&ctx, 0, 0);
        EXPECT(r01_player_anim_entity_state(&ctx) == 3, "airborne uses jump state");
        r01_player_anim_update(&ctx, -1, 0);
        EXPECT(r01_player_anim_entity_state(&ctx) == 3, "airborne jump keeps pose while moving");
        EXPECT(r01_player_anim_flip_h(&ctx) == 1, "airborne still flips");
        ctx.player_airborne = 0;
        r01_player_anim_update(&ctx, 0, 0);
        EXPECT(r01_player_anim_entity_state(&ctx) == 0, "land returns to idle");
    }

    {
        int dx, dy;
        uint8_t attr = 0;
        r01_cart_part_pose(8, 8, 4, 2, attr, 1, 0, &dx, &dy, &attr);
        EXPECT((attr & R01_CART_OAM_FLIP_H) != 0, "left facing sets H flip bit 6");
        EXPECT((attr & 0x0Fu) == 0, "left facing keeps bank");
        EXPECT(((attr >> 4) & 3) == 0, "left facing keeps pal");
    }

    {
        uint8_t blob[] = {
            'P', 'A', 1, 2, 0, 0, 0, 0, 8, 8, 1, 24, 1, 0, 0, 0, 0, 0, 0, 0, 8, 8, 1, 24, 2, 0, 0, 0,
        };
        R01CartPlayerAnim anim;
        R01PlayAnimCtx ac;
        int i;
        EXPECT(r01_cart_player_anim_parse(blob, sizeof(blob), &anim) == 0, "mini PA parse");
        r01_play_anim_init(&ac);
        r01_play_anim_set_idle_state(&ac, 0);
        for (i = 0; i < 23; i++) {
            r01_play_anim_tick_cart(&ac, &anim);
        }
        EXPECT(r01_play_anim_frame(&ac) == 0, "authored delay 24 holds 23 ticks");
        r01_play_anim_tick_cart(&ac, &anim);
        EXPECT(r01_play_anim_frame(&ac) == 1, "authored delay 24 advances on 24th");
        r01_play_anim_init(&ac);
        r01_play_anim_set_idle_state(&ac, 0);
        r01_play_anim_set_frame_delay(&ac, 5);
        for (i = 0; i < 4; i++) {
            r01_play_anim_tick_cart(&ac, &anim);
        }
        EXPECT(r01_play_anim_frame(&ac) == 0, "override 5 holds 4");
        r01_play_anim_tick_cart(&ac, &anim);
        EXPECT(r01_play_anim_frame(&ac) == 1, "override 5 advances on 5th");
    }

    free(p);
    TEST_EXIT();
}
