#include "test_harness.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
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

    memset(&part, 0, sizeof(part));
    part.bank = bank;
    part.tile_id = id;
    part.pal = 1;
    part.dx = 4;
    part.dy = 2;
    part.flip_h = 1;
    EXPECT(r01_entity_frame_add_part(&e->states[0].frames[0], &part) == 0, "add part");
    EXPECT(e->states[0].frames[0].part_count == 1, "part count");

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

    inst = r01_world_place_sprite(w, cat, 10, 20);
    EXPECT(inst == 1, "place sprite");
    EXPECT(w->entity_count == 3, "auto entity from place");
    EXPECT(w->instance_count == 2, "2 instances");
    EXPECT(w->instances[1].type_id == 2, "new type id");

    EXPECT(r01_play_start(&pl, p), "play start");
    n = r01_play_build_oam(p, &pl, oam, R01_OAM_MAX);
    EXPECT(n >= 3, "oam has player + parts");
    EXPECT(oam[0].tile_id == 1, "player oam tile");
    /* Instance 0: part (4,2), origin (3,5) at world (40,50) -> draw at 41,47 */
    {
        int found = 0;
        int oi;
        int expect_x = r01_entity_world_x(40, 3, 4) - pl.cam_x;
        int expect_y = r01_entity_world_y(50, 5, 2) - pl.cam_y;
        for (oi = 1; oi < n; oi++) {
            if (oam[oi].x == expect_x && oam[oi].y == expect_y && oam[oi].tile_id == id) {
                found = 1;
                EXPECT(oam[oi].flip_h == 1, "oam flip");
                EXPECT(oam[oi].pal == 1, "oam pal");
                break;
            }
        }
        EXPECT(found, "instance 0 in oam");
    }

    EXPECT(r01_project_save_json(p, "test_entities.r01proj", err, sizeof(err)) == 0, "save");
    EXPECT(r01_project_load_json(p2, "test_entities.r01proj", err, sizeof(err)) == 0, "load");
    EXPECT(p2->worlds[0].entity_count == 3, "roundtrip entity count");
    EXPECT(p2->worlds[0].instance_count == 2, "roundtrip instances");
    EXPECT(p2->worlds[0].instances[0].world_x == 40, "inst0 x");
    EXPECT(p2->worlds[0].instances[0].world_y == 50, "inst0 y");
    EXPECT(p2->worlds[0].instances[1].world_x == 10, "inst1 x");
    EXPECT(strcmp(p2->worlds[0].entities[0].states[0].name, "Walk") == 0, "name rt");
    EXPECT(p2->worlds[0].entities[0].states[0].origin_x == 3, "origin x");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].dx == 4, "part dx");

    EXPECT(r01_world_entity_remove(&p2->worlds[0], 0) == 0, "remove type");
    EXPECT(p2->worlds[0].entity_count == 2, "count after remove");
    /* Instance of type 0 removed; remaining type ids remapped. */
    EXPECT(p2->worlds[0].instance_count == 1, "inst of removed type gone");
    EXPECT(p2->worlds[0].instances[0].type_id == 1, "remapped type");

    free(p);
    free(p2);
    TEST_EXIT();
}
