#include "test_harness.h"

#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include <stdlib.h>
#include <string.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    uint8_t *prg;
    R01PrgCartLayout layout;
    uint16_t reset;

    EXPECT(p != NULL, "alloc");
    if (!p) {
        return 1;
    }
    prg = (uint8_t *)malloc(R01_PRG_BYTES);
    EXPECT(prg != NULL, "prg buf");
    if (!prg) {
        free(p);
        return 1;
    }

    r01_project_init(p, "prg");
    memset(&layout, 0, sizeof(layout));
    layout.off_pal_bg = 0x34;
    layout.len_pal_bg = 128;
    layout.off_pal_spr = 0xB4;
    layout.len_pal_spr = 128;
    layout.off_map_screen0 = 0x1000;
    layout.default_pal_row = 0;

    r01_prg_fill_phase1(prg, &p->worlds[0], &layout);

    EXPECT(prg[0] == 0x78, "SEI at reset");
    EXPECT(prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P',
           "R01P marker");
    EXPECT(prg[0x00F4] == 2, "R01P collision ver");

    /* Play table lives at PRG+$0100 (CPU $8100). */
    EXPECT(prg[0x0100] != 0xEA || prg[0x0108] != 0xEA, "play table region written");

    /* Reset vector at CPU $FFFC => PRG+$7FFC. */
    reset = (uint16_t)prg[0x7FFC] | ((uint16_t)prg[0x7FFD] << 8);
    EXPECT(reset == 0x8000u, "reset vector $8000");

    free(prg);
    free(p);
    TEST_EXIT();
}
