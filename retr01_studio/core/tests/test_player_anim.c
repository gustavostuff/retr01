#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

#include "retr01_studio/entities.h"
#include "retr01_studio/game_runtime.h"
#include "retr01_studio/player_anim.h"
#include "retr01_studio/project.h"

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
        int pe = r01_world_entity_add(w);
        R01EntityType *ent = &w->entities[pe];
        R01EntityPart part;
        int fi;
        memset(&part, 0, sizeof(part));
        part.tile_id = 1;
        r01_world_set_player_entity(w, pe);
        r01_entity_frame_add_part(&ent->states[0].frames[0], &part);
        ent->states[0].frame_count = 4;
        r01_entity_ensure_state(ent, 1);
        ent->states[1].frame_count = 3;
        for (fi = 0; fi < 3; fi++) {
            r01_entity_frame_add_part(&ent->states[1].frames[fi], &part);
        }
    }
    r01_game_ctx_init(&ctx);
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

    r01_player_default_face_set(&ctx, R01_PLAYER_FACE_LEFT);
    EXPECT(r01_player_anim_flip_h(&ctx) == 1, "idle faces left with flip");

    {
        int pe = r01_world_player_entity(w);
        int i;
        for (i = 0; i < 12; i++) {
            r01_player_anim_update(&ctx, 0, 0);
            r01_player_anim_tick(&ctx, w, pe);
        }
        EXPECT(r01_player_anim_frame(&ctx) == 0, "idle skips empty frame slots");
    }

    {
        int pe = r01_world_player_entity(w);
        r01_player_anim_update(&ctx, 1, 0);
        r01_player_anim_tick(&ctx, w, pe);
        r01_player_anim_tick(&ctx, w, pe);
        EXPECT(r01_player_anim_frame(&ctx) == 1, "walk frames advance");
    }

    free(p);
    TEST_EXIT();
}
