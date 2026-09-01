#include "test_harness.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/export_codegen.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];
    int type_id;
    int inst;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "codegen");
    type_id = r01_world_entity_add(&p->worlds[0]);
    EXPECT(type_id >= 0, "entity type");
    r01_world_set_player_entity(&p->worlds[0], type_id);
    inst = r01_world_place_entity(&p->worlds[0], type_id, 42, 84);
    EXPECT(inst >= 0, "place instance");

    EXPECT(r01_export_codegen(p, "codegen_out/test", err, sizeof(err)) == 0, "export codegen");
    EXPECT(path_exists("codegen_out/C/base_game.c"), "base_game.c");
    EXPECT(path_exists("codegen_out/C/custom_logic.c"), "custom_logic.c");
    EXPECT(path_exists("codegen_out/C/include/r01_game.h"), "r01_game.h");
    EXPECT(path_exists("codegen_out/ASM/main.s"), "ASM main.s");
    EXPECT(path_exists("codegen_out/ASM/collision/play_collision.s"), "play_collision.s");
    EXPECT(path_exists("codegen_out/data/pal_bg.bin"), "pal_bg.bin");

    EXPECT(r01_export_codegen(p, "codegen_out/test", err, sizeof(err)) == 0, "re-export codegen");
    {
        FILE *f = fopen("codegen_out/C/base_game.c", "r");
        char buf[4096];
        size_t n;
        EXPECT(f != NULL, "read base_game");
        if (f) {
            n = fread(buf, 1, sizeof(buf) - 1u, f);
            buf[n] = '\0';
            fclose(f);
            EXPECT(strstr(buf, "42, 84") != NULL, "instance coords in base_game");
            EXPECT(strstr(buf, "player_instance_spawn") != NULL, "spawn helper in base_game");
        }
    }

    free(p);
    TEST_EXIT();
}
