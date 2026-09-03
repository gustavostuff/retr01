#include "test_harness.h"

#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <stdlib.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    uint8_t r, g, b;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "palette");

    r01_kit_rgb(0, &r, &g, &b);
    EXPECT(r == 0 && g == 0 && b == 0, "kit index 0 is black");
    r01_kit_rgb(48, &r, &g, &b);
    EXPECT(r == 0xFF && g == 0xFF && b == 0xFF, "kit index 48 is white");

    EXPECT(r01_kit_nearest_master(0, 0, 0) == 0, "nearest black");
    EXPECT(r01_kit_nearest_master(255, 255, 255) == 48, "nearest white");

    r01_project_player_rgb(p, &r, &g, &b);
    EXPECT(r > 0 || g > 0 || b > 0, "player rgb non-zero");

    {
        const R01World *w = &p->worlds[0];
        const R01Screen *s = &w->screens[2];
        uint8_t pr, pg, pb;
        r01_screen_pixel_rgb(p, w, s, 0, 0, &pr, &pg, &pb);
        (void)pr;
        (void)pg;
        (void)pb;
    }

    r01_project_backdrop_rgb(p, &p->worlds[0], &r, &g, &b);
    (void)r;
    (void)g;
    (void)b;

    free(p);
    TEST_EXIT();
}
