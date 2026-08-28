#include "test_harness.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    R01Project *p2 = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];

    EXPECT(p != NULL && p2 != NULL, "alloc projects");
    if (!p || !p2) {
        free(p);
        free(p2);
        return 1;
    }

    r01_project_init(p, "roundtrip");
    p->default_world = 0;
    p->worlds[0].screens[9].present = 1;
    p->worlds[0].default_screen = 9;
    p->worlds[0].default_pal_row = 3;
    p->worlds[0].screens[2].attrs[0] = r01_attr_pack(0, 2, 1, 0);
    p->global_pal_bg[1][2].idx[1] = 42;

    EXPECT(r01_project_save_json(p, "test_roundtrip.r01proj", err, sizeof(err)) == 0, "save json");
    EXPECT(r01_project_load_json(p2, "test_roundtrip.r01proj", err, sizeof(err)) == 0, "load json");
    EXPECT(p2->worlds[0].default_screen == 9, "default_screen roundtrip");
    EXPECT(p2->worlds[0].default_pal_row == 3, "default_pal_row roundtrip");
    EXPECT(p2->global_pal_bg[1][2].idx[1] == 42, "palette roundtrip");
    EXPECT(p2->worlds[0].screens[2].attrs[0] == r01_attr_pack(0, 2, 1, 0), "tile attr roundtrip");
    EXPECT(p2->worlds[0].screen_count == R01_GRID_MAX * R01_GRID_MAX, "screen slot count roundtrip");

    free(p);
    free(p2);
    TEST_EXIT();
}
