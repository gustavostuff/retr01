#include "test_harness.h"

#include "retr01_studio/entity_import.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "r01_kit_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void put_px(uint8_t *rgba, int w, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t *p = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4u;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

static void kit_px(uint8_t *rgba, int w, int x, int y, int master, uint8_t a) {
    uint8_t r, g, b;
    r01_kit_rgb(master, &r, &g, &b);
    put_px(rgba, w, x, y, r, g, b, a);
}

static void set_spr_pal0(R01Project *p, int a, int b, int c) {
    int row = p->worlds[0].default_pal_row;
    p->global_pal_spr[row][0].idx[0] = 0;
    p->global_pal_spr[row][0].idx[1] = (uint8_t)a;
    p->global_pal_spr[row][0].idx[2] = (uint8_t)b;
    p->global_pal_spr[row][0].idx[3] = (uint8_t)c;
}

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    char err[192];
    uint8_t rgba[8 * 8 * 4];
    R01EntityImport in;
    int idx;
    char tmpdir[] = "r01_ase_test_XXXXXX";
    char proj_path[sizeof(tmpdir) + 16];
    char ase_dir[sizeof(tmpdir) + 32];
    char player_dir[sizeof(tmpdir) + 40];
    char ase_file[sizeof(tmpdir) + 56];
    FILE *f;

    EXPECT(p != NULL, "alloc");
    if (!p) {
        return 1;
    }
    r01_project_init(p, "import");
    set_spr_pal0(p, R01_KIT_RED_MASTER, 48, 16);

    memset(&in, 0, sizeof(in));
    snprintf(in.name, sizeof(in.name), "hero");
    in.state_count = 1;
    snprintf(in.states[0].name, sizeof(in.states[0].name), "idle");
    in.states[0].frame_count = 1;
    memset(rgba, 0, sizeof(rgba));
    kit_px(rgba, 8, 0, 0, R01_KIT_RED_MASTER, 255);
    kit_px(rgba, 8, 1, 0, 48, 255);
    in.states[0].frames[0].rgba = rgba;
    in.states[0].frames[0].w = 8;
    in.states[0].frames[0].h = 8;
    in.states[0].frames[0].delay = 6;
    idx = r01_world_import_entity_frames(p, &p->worlds[0], &in, err, sizeof(err));
    EXPECT(idx == 0, "import 2 opaque + trans");
    EXPECT(p->worlds[0].entity_count == 1, "count 1");
    EXPECT(p->worlds[0].entities[0].states[0].frames[0].part_count == 1, "one part");
    EXPECT(p->worlds[0].entities[0].states[0].frames[0].delay == 6, "delay 6");
    EXPECT(p->worlds[0].entities[0].states[0].frames[0].parts[0].pal == 0, "pal 0");

    {
        uint8_t rgba3[8 * 8 * 4];
        memset(rgba3, 0, sizeof(rgba3));
        kit_px(rgba3, 8, 0, 0, R01_KIT_RED_MASTER, 255);
        kit_px(rgba3, 8, 1, 0, 48, 255);
        kit_px(rgba3, 8, 2, 0, 16, 255);
        snprintf(in.name, sizeof(in.name), "full");
        in.states[0].frames[0].rgba = rgba3;
        idx = r01_world_import_entity_frames(p, &p->worlds[0], &in, err, sizeof(err));
        EXPECT(idx == 1, "trans + 3 opaque ok");
    }

    {
        uint8_t rgba_op[8 * 8 * 4];
        int x, y;
        int masters[3] = {R01_KIT_RED_MASTER, 48, 16};
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) {
                kit_px(rgba_op, 8, x, y, masters[(x + y) % 3], 255);
            }
        }
        snprintf(in.name, sizeof(in.name), "opaque3");
        in.states[0].frames[0].rgba = rgba_op;
        idx = r01_world_import_entity_frames(p, &p->worlds[0], &in, err, sizeof(err));
        EXPECT(idx == 2, "3 opaque no trans ok");
    }

    {
        uint8_t small[8 * 8 * 4];
        uint8_t tall[16 * 8 * 4];
        R01EntityImport mixed;
        memset(&mixed, 0, sizeof(mixed));
        snprintf(mixed.name, sizeof(mixed.name), "mixed");
        mixed.state_count = 2;
        snprintf(mixed.states[0].name, sizeof(mixed.states[0].name), "idle");
        snprintf(mixed.states[1].name, sizeof(mixed.states[1].name), "crouch");
        mixed.states[0].frame_count = 1;
        mixed.states[1].frame_count = 1;
        memset(small, 0, sizeof(small));
        memset(tall, 0, sizeof(tall));
        kit_px(small, 8, 0, 0, R01_KIT_RED_MASTER, 255);
        kit_px(tall, 16, 0, 0, R01_KIT_RED_MASTER, 255);
        kit_px(tall, 16, 8, 0, 48, 255);
        mixed.states[0].frames[0].rgba = tall;
        mixed.states[0].frames[0].w = 16;
        mixed.states[0].frames[0].h = 8;
        mixed.states[0].frames[0].delay = 1;
        mixed.states[1].frames[0].rgba = small;
        mixed.states[1].frames[0].w = 8;
        mixed.states[1].frames[0].h = 8;
        mixed.states[1].frames[0].delay = 1;
        idx = r01_world_import_entity_frames(p, &p->worlds[0], &mixed, err, sizeof(err));
        EXPECT(idx == 3, "mixed 16x8 + 8x8 ok");
        EXPECT(p->worlds[0].entities[idx].states[0].frames[0].part_count == 2, "tall two parts");
        EXPECT(p->worlds[0].entities[idx].states[1].frames[0].part_count == 1, "small one part");
    }

    {
        uint8_t rgba4[8 * 8 * 4];
        memset(rgba4, 0, sizeof(rgba4));
        kit_px(rgba4, 8, 0, 0, R01_KIT_RED_MASTER, 255);
        kit_px(rgba4, 8, 1, 0, 48, 255);
        kit_px(rgba4, 8, 2, 0, 16, 255);
        kit_px(rgba4, 8, 3, 0, 32, 255);
        snprintf(in.name, sizeof(in.name), "too_many");
        in.states[0].frames[0].rgba = rgba4;
        idx = r01_world_import_entity_frames(p, &p->worlds[0], &in, err, sizeof(err));
        EXPECT(idx < 0, "4 opaque fail");
    }

    {
        uint8_t bad[8 * 8 * 4];
        memset(bad, 0, sizeof(bad));
        put_px(bad, 8, 0, 0, 1, 2, 3, 255);
        snprintf(in.name, sizeof(in.name), "notkit");
        in.states[0].frames[0].rgba = bad;
        idx = r01_world_import_entity_frames(p, &p->worlds[0], &in, err, sizeof(err));
        EXPECT(idx < 0, "non-kit fail");
    }

    {
        uint8_t other[8 * 8 * 4];
        memset(other, 0, sizeof(other));
        kit_px(other, 8, 0, 0, 1, 255);
        snprintf(in.name, sizeof(in.name), "notpal");
        in.states[0].frames[0].rgba = other;
        idx = r01_world_import_entity_frames(p, &p->worlds[0], &in, err, sizeof(err));
        EXPECT(idx < 0, "kit but not pal 0 fail");
    }

    EXPECT(r01_kit_exact_master(0, 0, 0) == 0, "exact black");
    EXPECT(r01_kit_exact_master(1, 2, 3) == -1, "exact miss");

    {
        R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
        EXPECT(p2 != NULL, "p2");
        if (p2) {
            EXPECT(r01_project_save_json(p, "test_ase_import.r01proj", err, sizeof(err)) == 0, "save");
            EXPECT(r01_project_load_json(p2, "test_ase_import.r01proj", err, sizeof(err)) == 0, "load");
            EXPECT(p2->worlds[0].entities[0].states[0].frames[0].delay == 6, "delay json");
            free(p2);
        }
        remove("test_ase_import.r01proj");
    }

    EXPECT(mkdtemp(tmpdir) != NULL, "mkdtemp");
    snprintf(proj_path, sizeof(proj_path), "%s/game.r01proj", tmpdir);
    snprintf(ase_dir, sizeof(ase_dir), "%s/%s", tmpdir, R01_ASEPRITE_ENTITIES_DIR);
    snprintf(player_dir, sizeof(player_dir), "%s/%s/player", tmpdir, R01_ASEPRITE_ENTITIES_DIR);
    snprintf(ase_file, sizeof(ase_file), "%s/%s/player/idle.ase", tmpdir, R01_ASEPRITE_ENTITIES_DIR);
    EXPECT(mkdir(ase_dir, 0755) == 0, "mkdir ase");
    EXPECT(mkdir(player_dir, 0755) == 0, "mkdir player");
    f = fopen(ase_file, "wb");
    EXPECT(f != NULL, "touch ase");
    if (f) {
        fclose(f);
    }

    {
        R01AsepriteListing listing;
        R01AsepriteImportResult res;
        EXPECT(r01_aseprite_listing_scan(ase_dir, &listing, err, sizeof(err)) == 0, "scan");
        EXPECT(listing.count == 1, "one file");
        EXPECT(strcmp(listing.files[0], "player/idle.ase") == 0, "rel path");

        res.generated = -1;
        res.unchanged = 0;
        snprintf(p->aseprite_entities_files[0], R01_ASEPRITE_REL_MAX, "player/idle.ase");
        p->aseprite_entities_file_count = 1;
        EXPECT(r01_project_import_aseprite_entities(p, proj_path, &res, err, sizeof(err)) == 0, "unchanged");
        EXPECT(res.unchanged == 1, "flag unchanged");
        EXPECT(res.generated == 0, "no generate");

        p->aseprite_entities_file_count = 0;
        snprintf(p->worlds[0].entities[0].name, sizeof(p->worlds[0].entities[0].name), "player");
        res.unchanged = 1;
        EXPECT(r01_project_import_aseprite_entities(p, proj_path, &res, err, sizeof(err)) == 0, "skip existing");
        EXPECT(res.unchanged == 0, "listing differed");
        EXPECT(res.generated == 0, "skipped name");
        EXPECT(p->aseprite_entities_file_count == 1, "listing stored");
    }

    {
        R01AsepriteImportResult res;
        char empty_dir[sizeof(tmpdir) + 16];
        char missing[sizeof(tmpdir) + 48];
        snprintf(empty_dir, sizeof(empty_dir), "%s/empty", tmpdir);
        snprintf(missing, sizeof(missing), "%s/empty/none.r01proj", tmpdir);
        EXPECT(mkdir(empty_dir, 0755) == 0, "mkdir empty");
        EXPECT(r01_project_import_aseprite_entities(p, missing, &res, err, sizeof(err)) != 0, "missing dir");
        rmdir(empty_dir);
    }

    unlink(ase_file);
    rmdir(player_dir);
    rmdir(ase_dir);
    rmdir(tmpdir);
    free(p);
    TEST_EXIT();
}
