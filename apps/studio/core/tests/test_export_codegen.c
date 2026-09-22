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
    char marker[] = "r01_game_on_init unique-export-marker";

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "codegen");
    type_id = r01_world_entity_add(p);
    EXPECT(type_id >= 0, "entity type");
    r01_world_set_player_entity(p, type_id);
    inst = r01_world_place_entity(&p->worlds[0], type_id, 42, 84);
    EXPECT(inst >= 0, "place instance");

    EXPECT(r01_export_codegen(p, "codegen_out/test", err, sizeof(err)) == 0, "export codegen");
    EXPECT(path_exists("codegen_out/game_logic.c"), "game_logic.c");
    EXPECT(path_exists("codegen_out/data/pal_bg.bin"), "pal_bg.bin");
    EXPECT(!path_exists("codegen_out/C/base_game.c"), "no base_game.c");
    EXPECT(!path_exists("codegen_out/C/custom_logic.c"), "no custom_logic.c");
    EXPECT(!path_exists("codegen_out/ASM/main.s"), "no ASM tree");

    {
        FILE *f = fopen("codegen_out/game_logic.c", "a");
        EXPECT(f != NULL, "append marker");
        if (f) {
            fprintf(f, "\n/* %s */\n", marker);
            fclose(f);
        }
    }

    EXPECT(r01_export_codegen(p, "codegen_out/test", err, sizeof(err)) == 0, "re-export codegen");
    {
        FILE *f = fopen("codegen_out/game_logic.c", "r");
        char buf[8192];
        size_t n;
        EXPECT(f != NULL, "read game_logic");
        if (f) {
            n = fread(buf, 1, sizeof(buf) - 1u, f);
            buf[n] = '\0';
            fclose(f);
            EXPECT(strstr(buf, marker) != NULL, "second export keeps game_logic.c");
            EXPECT(strstr(buf, "r01_game_on_tick") != NULL, "author hook name");
        }
    }

    free(p);
    TEST_EXIT();
}
