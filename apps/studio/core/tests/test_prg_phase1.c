#include "test_harness.h"

#include "retr01_studio/bgm_pack.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include "r01_apu_cart.h"
#include "r01_apu_fd.h"

#include <stdio.h>
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

    r01_prg_fill_phase1(prg, p, &layout);

    EXPECT(prg[0] == 0x78, "SEI at reset");
    EXPECT(prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P',
           "R01P marker");
    EXPECT(prg[0x00F4] == 4, "R01P solid-pattern ver");
    EXPECT(prg[R01_PRG_BGM_BOOT_OFF] == 0, "boot track empty after fill");

    /* Play table lives at PRG+$0100 (CPU $8100). */
    EXPECT(prg[0x0100] != 0xEA || prg[0x0108] != 0xEA, "play table region written");

    /* Reset vector at CPU $FFFC => PRG+$7FFC. */
    reset = (uint16_t)prg[0x7FFC] | ((uint16_t)prg[0x7FFD] << 8);
    EXPECT(reset == 0x8000u, "reset vector $8000");

    {
        FILE *f = fopen("test_bgm_logic.c", "w");
        R01BgmData bgm;
        uint16_t off;
        uint16_t len;
        memset(&bgm, 0, sizeof(bgm));
        bgm.present = 1;
        bgm.track_count = 1;
        bgm.region_count[0][0] = 1;
        bgm.region[0][0][0].start = 0;
        bgm.region[0][0][0].len = 2;
        snprintf(bgm.region[0][0][0].tok, sizeof(bgm.region[0][0][0].tok), "C4");
        bgm.ch_ins[0][0] = R01_BGM_INS_PIANO;
        bgm.ch_ins[0][2] = R01_BGM_INS_FLUTE;
        EXPECT(f != NULL, "write custom_logic");
        if (f) {
            fputs("void r01_custom_on_init(R01GameCtx *ctx) {\n    r01_bgm_play(ctx, 1);\n}\n", f);
            fclose(f);
        }
        r01_bgm_pack_prg(prg, &bgm, "test_bgm_logic.c");
        EXPECT(prg[R01_PRG_BGM_BOOT_OFF] == 1, "boot track 1 from r01_bgm_play");
        EXPECT(prg[R01_PRG_BGM_OFF] == R01_PRG_BGM_MAGIC0 && prg[R01_PRG_BGM_OFF + 1] == R01_PRG_BGM_MAGIC1,
               "BG magic");
        EXPECT(prg[R01_PRG_BGM_OFF + 2] == 1, "packed track count");
        EXPECT(prg[R01_PRG_BGM_OFF + 3] == R01_PRG_BGM_INS_VER, "ins table present");
        EXPECT(prg[R01_PRG_BGM_OFF + R01_PRG_BGM_HDR] == R01_BGM_INS_PIANO, "track 1 ch1 piano");
        EXPECT(prg[R01_PRG_BGM_OFF + R01_PRG_BGM_HDR + 2] == R01_BGM_INS_FLUTE, "track 1 ch3 flute");
        off = (uint16_t)prg[R01_PRG_BGM_OFF + 4] | ((uint16_t)prg[R01_PRG_BGM_OFF + 5] << 8);
        len = (uint16_t)prg[R01_PRG_BGM_OFF + 20] | ((uint16_t)prg[R01_PRG_BGM_OFF + 21] << 8);
        EXPECT(off == R01_PRG_BGM_HDR_V1, "payload starts after ins table");
        EXPECT(len > 0, "payload length");
        EXPECT(prg[R01_PRG_BGM_OFF + off] == R01_APU_FD_OP, "FD stream");
        r01_bgm_pack_prg(prg, &bgm, NULL);
        EXPECT(prg[R01_PRG_BGM_BOOT_OFF] == 0, "no custom_logic means no autoplay");
        reset = (uint16_t)prg[0x7FFC] | ((uint16_t)prg[0x7FFD] << 8);
        EXPECT(reset == 0x8000u, "vectors survive BGM pack");
        remove("test_bgm_logic.c");
    }

    free(prg);
    free(p);
    TEST_EXIT();
}
