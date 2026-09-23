#include "test_harness.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/export_codegen.h"
#include "retr01_studio/project.h"
#include "retr01_studio/warps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int file_contains(const char *path, const char *needle) {
    FILE *f;
    char buf[8192];
    size_t n;
    if (!path || !needle) {
        return 0;
    }
    f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    n = fread(buf, 1, sizeof(buf) - 1u, f);
    buf[n] = '\0';
    fclose(f);
    return strstr(buf, needle) != NULL;
}

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];
    int type_id;
    int slime;
    int inst;
    int warp;
    char marker[] = "r01_game_on_init unique-export-marker";

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    (void)system("rm -rf codegen_out");

    r01_project_init(p, "codegen");
    type_id = r01_world_entity_add(p);
    EXPECT(type_id >= 0, "entity type");
    strncpy(p->entities[type_id].name, "player", R01_ENTITY_NAME_MAX - 1);
    p->entities[type_id].name[R01_ENTITY_NAME_MAX - 1] = '\0';
    r01_world_set_player_entity(p, type_id);
    slime = r01_world_entity_add(p);
    EXPECT(slime >= 0, "slime type");
    strncpy(p->entities[slime].name, "Slime", R01_ENTITY_NAME_MAX - 1);
    p->entities[slime].name[R01_ENTITY_NAME_MAX - 1] = '\0';
    inst = r01_world_place_entity(&p->worlds[0], type_id, 42, 84);
    EXPECT(inst >= 0, "place instance");
    warp = r01_world_warp_entrance_add(&p->worlds[0], 0, 0, 1, 1);
    EXPECT(warp >= 0, "warp entrance");

    EXPECT(r01_export_codegen(p, "codegen_out/test", err, sizeof(err)) == 0, "export codegen");
    EXPECT(path_exists("codegen_out/game_logic.c"), "game_logic.c");
    EXPECT(path_exists("codegen_out/include/r01_entity_ids.h"), "entity ids header");
    EXPECT(path_exists("codegen_out/include/r01_warp_ids.h"), "warp ids header");
    EXPECT(path_exists("codegen_out/data/pal_bg.bin"), "pal_bg.bin");
    EXPECT(path_exists("codegen_out/data/spawns.bin"), "spawns.bin");
    EXPECT(path_exists("codegen_out/data/play8100.bin"), "play8100.bin");
    EXPECT(path_exists("codegen_out/data/worlddir.bin"), "worlddir.bin");
    EXPECT(path_exists("codegen_out/data/solids.bin"), "solids.bin");
    EXPECT(path_exists("codegen_out/data/r01p.bin"), "r01p.bin");
    EXPECT(!path_exists("codegen_out/C/base_game.c"), "no base_game.c");
    EXPECT(!path_exists("codegen_out/C/custom_logic.c"), "no custom_logic.c");
    EXPECT(!path_exists("codegen_out/ASM/main.s"), "no ASM tree");
    EXPECT(file_contains("codegen_out/game_logic.c", "#include \"r01_entity_ids.h\""),
           "template includes entity ids");
    EXPECT(file_contains("codegen_out/game_logic.c", "#include \"r01_warp_ids.h\""),
           "template includes warp ids");
    EXPECT(!file_contains("codegen_out/game_logic.c", "SLIME_TYPE"), "template has no raw slime index");
    EXPECT(file_contains("codegen_out/include/r01_entity_ids.h", "#define R01_ENT_TYPE_COUNT 2"),
           "type count");
    EXPECT(file_contains("codegen_out/include/r01_entity_ids.h", "#define R01_ENT_MARKED_PLAYER 0"),
           "marked player");
    EXPECT(file_contains("codegen_out/include/r01_entity_ids.h", "#define R01_ENT_PLAYER 0"),
           "player symbol");
    EXPECT(file_contains("codegen_out/include/r01_entity_ids.h", "#define R01_ENT_SLIME 1"),
           "slime symbol");
    EXPECT(file_contains("codegen_out/include/r01_warp_ids.h", "#define R01_WARP_ENTRANCE_COUNT 1"),
           "warp count");
    EXPECT(file_contains("codegen_out/include/r01_warp_ids.h", "#define R01_WARP_W_00 0"), "warp id");

    {
        FILE *f = fopen("codegen_out/game_logic.c", "a");
        EXPECT(f != NULL, "append marker");
        if (f) {
            fprintf(f, "\n/* %s */\n", marker);
            fclose(f);
        }
    }
    {
        FILE *f = fopen("codegen_out/include/r01_entity_ids.h", "a");
        EXPECT(f != NULL, "append stale header");
        if (f) {
            fprintf(f, "\n#define STALE_HEADER 1\n");
            fclose(f);
        }
    }
    strncpy(p->entities[slime].name, "Goomba", R01_ENTITY_NAME_MAX - 1);
    p->entities[slime].name[R01_ENTITY_NAME_MAX - 1] = '\0';

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
    EXPECT(file_contains("codegen_out/include/r01_entity_ids.h", "#define R01_ENT_GOOMBA 1"),
           "header refresh rename");
    EXPECT(!file_contains("codegen_out/include/r01_entity_ids.h", "R01_ENT_SLIME"),
           "old slime symbol gone");
    EXPECT(!file_contains("codegen_out/include/r01_entity_ids.h", "STALE_HEADER"),
           "header overwrite drops stale");

    free(p);
    TEST_EXIT();
}
