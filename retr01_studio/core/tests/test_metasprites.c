#include "test_harness.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
    R01World *w;
    R01MetaspriteDef *ms;
    R01EntityPart part;
    int meta, type_id, inst, fr_parts;
    char err[128];

    EXPECT(p != NULL && p2 != NULL, "alloc");
    if (!p || !p2) {
        return 1;
    }

    r01_project_init(p, "meta");
    w = &p->worlds[0];

    meta = r01_world_metasprite_add(w);
    EXPECT(meta == 0, "add metasprite");
    ms = r01_world_metasprite(w, meta);
    EXPECT(ms != NULL, "metasprite ptr");
    strncpy(ms->name, "Blob", sizeof(ms->name) - 1);

    memset(&part, 0, sizeof(part));
    part.tile_id = 2;
    part.dx = 0;
    part.dy = 0;
    EXPECT(r01_metasprite_add_part(ms, &part) >= 0, "part0");
    part.tile_id = 3;
    part.dx = 8;
    part.dy = 0;
    EXPECT(r01_metasprite_add_part(ms, &part) >= 0, "part1");
    EXPECT(ms->frame.part_count == 2, "two parts");

    {
        R01EntityFrame fr;
        memset(&fr, 0, sizeof(fr));
        EXPECT(r01_entity_frame_add_metasprite(&fr, ms, 0, 0) == 0, "flatten into frame");
        fr_parts = fr.part_count;
        EXPECT(fr_parts == 2, "flattened part count");
        EXPECT(fr.parts[0].tile_id == 2 && fr.parts[1].tile_id == 3, "flattened tiles");
    }

    type_id = r01_world_entity_from_metasprite(w, meta);
    EXPECT(type_id >= 0, "entity from metasprite");
    EXPECT(w->entities[type_id].states[0].frames[0].part_count == 2, "entity got parts");

    inst = r01_world_place_metasprite(w, meta, 64, 48);
    EXPECT(inst >= 0, "place metasprite instance");
    EXPECT(w->instances[inst].world_x == 64 && w->instances[inst].world_y == 48, "instance pos");

    EXPECT(r01_project_save_json(p, "test_metasprites.r01proj", err, sizeof(err)) == 0, "save");
    EXPECT(r01_project_load_json(p2, "test_metasprites.r01proj", err, sizeof(err)) == 0, "load");
    EXPECT(p2->worlds[0].metasprite_count == 1, "roundtrip count");
    EXPECT(strcmp(p2->worlds[0].metasprites[0].name, "Blob") == 0, "roundtrip name");
    EXPECT(p2->worlds[0].metasprites[0].frame.part_count == 2, "roundtrip parts");

    EXPECT(r01_world_metasprite_remove(w, 0) == 0, "remove");
    EXPECT(w->metasprite_count == 0, "empty after remove");

    free(p);
    free(p2);
    TEST_EXIT();
}
