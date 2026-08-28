#include "test_harness.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
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
    uint8_t tile[R01_TILE_BYTES];
    char err[128];
    int bank, id, cat, idx, idx2;

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

    EXPECT(r01_project_save_json(p, "test_entities.r01proj", err, sizeof(err)) == 0, "save");
    EXPECT(r01_project_load_json(p2, "test_entities.r01proj", err, sizeof(err)) == 0, "load");
    EXPECT(p2->worlds[0].entity_count == 2, "roundtrip count");
    EXPECT(strcmp(p2->worlds[0].entities[0].states[0].name, "Walk") == 0, "name rt");
    EXPECT(p2->worlds[0].entities[0].states[0].origin_x == 3, "origin x");
    EXPECT(p2->worlds[0].entities[0].states[0].origin_y == 5, "origin y");
    EXPECT(p2->worlds[0].entities[0].states[0].hitbox_x == 1, "hb x");
    EXPECT(p2->worlds[0].entities[0].states[0].hitbox_y == 2, "hb y");
    EXPECT(p2->worlds[0].entities[0].states[0].frame_count == 2, "frames rt");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].part_count == 1, "parts rt");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].dx == 4, "part dx");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].dy == 2, "part dy");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].flip_h == 1, "part fh");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].pal == 1, "part pal");
    EXPECT(p2->worlds[0].entities[1].states[0].frames[0].parts[0].tile_id == id, "from-sprite rt");

    EXPECT(r01_world_entity_remove(&p2->worlds[0], 0) == 0, "remove");
    EXPECT(p2->worlds[0].entity_count == 1, "count after remove");
    EXPECT(p2->worlds[0].entities[0].states[0].frames[0].parts[0].tile_id == id, "shifted");

    free(p);
    free(p2);
    TEST_EXIT();
}
