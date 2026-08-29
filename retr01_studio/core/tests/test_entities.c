#include "test_harness.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;
    R01EntityType *e;
    R01EntityFrame *fr;
    R01EntityPart part;
    R01PlayState pl;
    R01OamEntry oam[R01_OAM_MAX];
    uint8_t tile[R01_TILE_BYTES];
    char err[128];
    int bank, id, cat, idx, idx2, inst, n;

    EXPECT(p != NULL && p2 != NULL, "alloc");
    if (!p || !p2) {
        free(p);
        free(p2);
        return 1;
    }

    r01_project_init(p, "entities");
    w = &p->worlds[0];

    bank = 0;
    id = r01_chr_alloc_spr_tile(w, bank);
    EXPECT(id == 0, "spr tile");
    memset(tile, 0, sizeof(tile));
    tile[0] = 0x11;
    tile[8] = 0x22;
    EXPECT(r01_chr_write_spr_tile(w, bank, id, tile) == 0, "write spr");
    cat = r01_world_sprite_add(w, bank, id, 1);
    EXPECT(cat == 0, "sprite catalog");

    idx = r01_world_entity_add(w);
    EXPECT(idx == 0, "entity add");
    EXPECT(w->entity_count == 1, "entity count");
    e = r01_world_entity(w, idx);
    EXPECT(e != NULL, "entity ptr");
    EXPECT(e->state_count == 1, "default 1 state");
    EXPECT(strcmp(e->states[0].name, "Idle") == 0, "Idle name");
    EXPECT(e->states[0].hitbox_w == R01_ENTITY_HITBOX_W, "hitbox w");
    EXPECT(e->states[0].frame_count == 1, "1 frame");

    fr = r01_entity_ensure_frame(e, 0, 1);
    EXPECT(fr != NULL, "ensure frame 1");
    EXPECT(e->states[0].frame_count == 2, "frame count 2");

    {
        R01EntityState *st1 = r01_entity_ensure_state(e, 1);
        EXPECT(st1 != NULL, "ensure state 1");
        EXPECT(e->state_count == 2, "state count 2");
        EXPECT(strcmp(st1->name, "Walk") == 0, "Walk default name");
        EXPECT(r01_entity_ensure_state(e, 3) != NULL, "ensure state 3");
        EXPECT(e->state_count == 4, "state count 4");
        EXPECT(strcmp(e->states[2].name, "Hurt") == 0, "Hurt name");
        EXPECT(strcmp(e->states[3].name, "Jump") == 0, "Jump name");
        EXPECT(r01_entity_trim_last_state(e) == 1, "trim empty Jump");
        EXPECT(e->state_count == 3, "after trim state");
        EXPECT(r01_entity_trim_last_state(e) == 1, "trim empty Hurt");
        EXPECT(r01_entity_trim_last_state(e) == 1, "trim empty Walk");
        EXPECT(e->state_count == 1, "back to Idle");
        EXPECT(r01_entity_trim_last_state(e) == 0, "cannot trim Idle");
        EXPECT(r01_entity_ensure_frame(e, 0, 3) != NULL, "ensure frame 3");
        EXPECT(e->states[0].frame_count == 4, "4 frames");
        EXPECT(r01_entity_trim_last_frame(e, 0) == 1, "trim empty frame");
        EXPECT(e->states[0].frame_count == 3, "3 frames");
        while (e->states[0].frame_count > 1) {
            EXPECT(r01_entity_trim_last_frame(e, 0) == 1, "trim frame");
        }
        EXPECT(r01_entity_trim_last_frame(e, 0) == 0, "keep frame 0");
    }

    memset(&part, 0, sizeof(part));
    part.bank = bank;
    part.tile_id = id;
    part.pal = 1;
    part.dx = 4;
    part.dy = 2;
    part.flip_h = 1;
    EXPECT(r01_entity_frame_add_part(&e->states[0].frames[0], &part) == 0, "add part");
    EXPECT(e->states[0].frames[0].part_count == 1, "part count");

    strncpy(e->name, "Hero", R01_ENTITY_NAME_MAX - 1);
    strncpy(e->states[0].name, "Walk", R01_ENTITY_NAME_MAX - 1);
    e->states[0].origin_x = 3;
    e->states[0].origin_y = 5;
    e->states[0].hitbox_x = 1;
    e->states[0].hitbox_y = 2;

    idx2 = r01_world_entity_from_sprite(w, cat);
    EXPECT(idx2 == 1, "from sprite");
    EXPECT(w->entities[idx2].states[0].frames[0].part_count == 1, "auto part");
    EXPECT(w->entities[idx2].states[0].frames[0].parts[0].tile_id == id, "auto tile");
    EXPECT(w->entities[idx2].states[0].frames[0].parts[0].pal == 1, "auto pal");

    EXPECT(r01_entity_frame_remove_part(&e->states[0].frames[0], 0) == 0, "remove part");
    EXPECT(e->states[0].frames[0].part_count == 0, "empty parts");
    EXPECT(r01_entity_frame_add_part(&e->states[0].frames[0], &part) == 0, "re-add part");

    inst = r01_world_place_entity(w, 0, 40, 50);
    EXPECT(inst == 0, "place entity");
    EXPECT(w->instance_count == 1, "1 instance");
    EXPECT(w->instances[0].world_x == 40 && w->instances[0].world_y == 50, "inst xy");
    EXPECT(w->instances[0].flip_h == 0, "inst flip default");
    EXPECT(w->instances[0].flip_v == 0, "inst flip_v default");
    {
        int dx, dy, fh, fv;
        r01_entity_part_instance_pose(&e->states[0], &part, 1, 0, &dx, &dy, &fh, &fv);
        /* origin 3, part dx 4 flip_h 1 -> mirrored dx = 2*3-4-8 = -6, flip cleared */
        EXPECT(dx == -6, "pose mirror dx");
        EXPECT(dy == 2, "pose mirror dy");
        EXPECT(fh == 0, "pose toggles part flip_h");
        EXPECT(fv == 0, "pose keeps flip_v");
        r01_entity_part_instance_pose(&e->states[0], &part, 0, 1, &dx, &dy, &fh, &fv);
        /* origin_y 5, part dy 2 -> dy' = 2*5-2-8 = 0, flip_v set */
        EXPECT(dx == 4, "pose v keeps dx");
        EXPECT(dy == 0, "pose mirror dy");
        EXPECT(fh == 1, "pose v keeps part flip_h");
        EXPECT(fv == 1, "pose toggles flip_v");
    }
    w->instances[0].flip_h = 1;

    inst = r01_world_place_sprite(w, cat, 10, 20);
    EXPECT(inst == 1, "place sprite");
    EXPECT(w->entity_count == 3, "auto entity from place");
    EXPECT(w->instance_count == 2, "2 instances");
    EXPECT(w->instances[1].type_id == 2, "new type id");

    {
        R01MetaspriteDef *ms;
        int meta = r01_world_metasprite_add(w);
        EXPECT(meta == 0, "metasprite add");
        ms = r01_world_metasprite(w, meta);
        strncpy(ms->name, "Blob", R01_ENTITY_NAME_MAX - 1);
        part.dx = 1;
        part.dy = 0;
        EXPECT(r01_metasprite_add_part(ms, &part) == 0, "meta part 0");
        part.dx = 2;
        part.dy = 0;
        EXPECT(r01_metasprite_add_part(ms, &part) == 1, "meta part 1");
        inst = r01_world_place_metasprite(w, meta, 64, 72);
        EXPECT(inst == 2, "place metasprite");
        EXPECT(w->entity_count == 4, "auto entity from meta");
        EXPECT(w->entities[3].states[0].frames[0].part_count == 2, "meta parts copied");
        EXPECT(w->instances[2].world_x == 64 && w->instances[2].world_y == 72, "meta inst xy");
    }

    /* Spawn on screen (0,0) so instances near the origin stay in the viewport. */
    {
        int ds = r01_world_find_screen(w, 0, 0);
        EXPECT(ds >= 0 && w->screens[ds].present, "screen 0,0 present");
        w->default_screen = ds;
    }
    EXPECT(r01_play_start(&pl, p), "play start");
    n = r01_play_build_oam(p, &pl, oam, R01_OAM_MAX);
    EXPECT(n >= 3, "oam has player + parts");
    EXPECT(oam[0].tile_id == 1, "player oam tile");
    /* Instance 0 flipped: part dx 4 -> -6, draw at world+(dx-origin). */
    {
        int found = 0;
        int oi;
        int expect_x = r01_entity_world_x(40, 3, -6) - pl.cam_x;
        int expect_y = r01_entity_world_y(50, 5, 2) - pl.cam_y;
        for (oi = 1; oi < n; oi++) {
            if (oam[oi].x == expect_x && oam[oi].y == expect_y && oam[oi].tile_id == id) {
                found = 1;
                EXPECT(oam[oi].flip_h == 0, "oam flip toggled by inst");
                EXPECT(oam[oi].pal == 1, "oam pal");
                break;
            }
        }
        EXPECT(found, "instance 0 in oam");
    }

    /* Mark entity 0 as player: Play uses state0/frame0 at player pos; skips its instances. */
    {
        R01OamEntry oam2[R01_OAM_MAX];
        int n2, found_player = 0, found_inst = 0, oi;
        int expect_px = r01_entity_world_x(pl.player_x, 3, 4) - pl.cam_x;
        int expect_py = r01_entity_world_y(pl.player_y, 5, 2) - pl.cam_y;
        r01_world_set_player_entity(w, 0);
        EXPECT(r01_world_player_entity(w) == 0, "player marked");
        n2 = r01_play_build_oam(p, &pl, oam2, R01_OAM_MAX);
        EXPECT(n2 >= 1, "player entity oam");
        for (oi = 0; oi < n2; oi++) {
            if (oam2[oi].tile_id == id && oam2[oi].x == expect_px && oam2[oi].y == expect_py) {
                found_player = 1;
            }
            if (oam2[oi].tile_id == id && oam2[oi].x == r01_entity_world_x(40, 3, -6) - pl.cam_x) {
                found_inst = 1;
            }
        }
        EXPECT(found_player, "player uses entity art");
        EXPECT(!found_inst, "player type instance skipped");
    }

    EXPECT(r01_project_save_json(p, "test_entities.r01proj", err, sizeof(err)) == 0, "save");
    EXPECT(r01_project_load_json(p2, "test_entities.r01proj", err, sizeof(err)) == 0, "load");
    EXPECT(p2->worlds[0].entity_count == 4, "roundtrip entity count");
    EXPECT(p2->worlds[0].player_entity == 0, "player entity rt");
    EXPECT(p2->worlds[0].instance_count == 3, "roundtrip instances");
    EXPECT(p2->worlds[0].instances[0].world_x == 40, "inst0 x");
    EXPECT(p2->worlds[0].instances[0].world_y == 50, "inst0 y");
    EXPECT(p2->worlds[0].instances[0].flip_h == 1, "inst0 fh rt");
    EXPECT(p2->worlds[0].instances[1].world_x == 10, "inst1 x");
    EXPECT(p2->worlds[0].instances[2].world_x == 64, "meta inst x");
    EXPECT(p2->worlds[0].metasprite_count == 1, "metasprite rt");
    EXPECT(strcmp(p2->worlds[0].entities[0].name, "Hero") == 0, "entity name rt");
    EXPECT(strcmp(p2->worlds[0].entities[0].states[0].name, "Walk") == 0, "name rt");
    EXPECT(p2->worlds[0].entities[0].states[0].origin_x == 3, "origin x");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].dx == 4, "part dx");

    {
        char id[R01_ID_MAX];
        char slug[32];
        r01_id_slugify(slug, sizeof(slug), "Player Walking!");
        EXPECT(strcmp(slug, "player_walking") == 0, "slugify");
        strncpy(e->name, "Player", R01_ENTITY_NAME_MAX - 1);
        strncpy(e->states[0].name, "Walking", R01_ENTITY_NAME_MAX - 1);
        r01_entity_frame_id(id, sizeof(id), 4, e, 0, 2);
        EXPECT(strcmp(id, "w_05_player_walking_frame_02") == 0, "frame id");
        r01_entity_type_id(id, sizeof(id), 0, e);
        EXPECT(strcmp(id, "w_01_player") == 0, "type id");
    }

    EXPECT(r01_world_entity_remove(&p2->worlds[0], 0) == 0, "remove type");
    EXPECT(p2->worlds[0].entity_count == 3, "count after remove");
    EXPECT(p2->worlds[0].player_entity == -1, "player cleared on remove");
    /* Instance of type 0 removed; remaining type ids remapped. */
    EXPECT(p2->worlds[0].instance_count == 2, "inst of removed type gone");
    EXPECT(p2->worlds[0].instances[0].type_id == 1, "remapped type");

    free(p);
    free(p2);
    TEST_EXIT();
}
