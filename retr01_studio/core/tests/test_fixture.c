#include "test_harness.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *loaded = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];
    const char *proj_path;
    const char *cart_path;
    FILE *f;
    char magic[6];

    proj_path = argc > 1 ? argv[1] : R01_ROM_DIR "/test.r01proj";
    cart_path = argc > 2 ? argv[2] : R01_ROM_DIR "/test.retr01";

    EXPECT(p != NULL && loaded != NULL, "alloc project");
    if (!p || !loaded) {
        return 1;
    }

    EXPECT(r01_project_load_json(loaded, proj_path, err, sizeof(err)) == 0, "load golden project");
    EXPECT(loaded->worlds[0].screen_count == R01_GRID_MAX * R01_GRID_MAX, "world screen slots");
    EXPECT(loaded->default_world >= 0 && loaded->default_world < R01_MAX_WORLDS, "default world");

    f = fopen(cart_path, "rb");
    EXPECT(f != NULL, "open golden cart");
    if (f) {
        EXPECT(fread(magic, 1, 6, f) == 6, "read cart magic");
        EXPECT(memcmp(magic, "retr01", 6) == 0, "cart magic");
        fclose(f);
    }

    r01_project_init(p, "fixture");
    for (int i = 0; i < p->worlds[0].screen_count; i++) {
        p->worlds[0].screens[i].present = 1;
    }
    EXPECT(r01_export_bundle(p, "fixture_export", err, sizeof(err)) == 0, "export bundle");
    {
        FILE *out = fopen("fixture_export.retr01", "rb");
        EXPECT(out != NULL, "open exported cart");
        if (out) {
            EXPECT(fread(magic, 1, 6, out) == 6, "read exported magic");
            EXPECT(memcmp(magic, "retr01", 6) == 0, "exported magic");
            fclose(out);
        }
    }

    free(loaded);
    free(p);
    TEST_EXIT();
}
