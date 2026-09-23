#include "test_harness.h"

#include "retr01_studio/bgm_pack.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include "r01_apu_cart.h"
#include "r01_apu_fd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    uint8_t *prg;
    R01PrgCartLayout layout;

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

    EXPECT(prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P',
           "R01P marker");
    EXPECT(prg[0x00F4] == R01_PRG_R01P_VER, "R01P C-runtime ver");
    EXPECT(prg[R01_PRG_BGM_BOOT_OFF] == 0, "boot track empty after fill");

    /* Play table lives at PRG+$0100 (CPU $8100). */
    EXPECT(prg[0x0120] == R01_CELL_PACK(R01_START_COL, R01_START_ROW) || prg[0x0100] != 0, "play table written");
    {
        uint16_t g00 = (uint16_t)prg[R01_PRG_COLL_GRID_OFF] | ((uint16_t)prg[R01_PRG_COLL_GRID_OFF + 1u] << 8);
        EXPECT(g00 == 0x8701u, "coll grid (0,0) probe");
    }

    EXPECT(prg[R01_PRG_BOOTMAP_OFF] == 0x34, "boot MAP pal_bg lo");
    EXPECT(prg[R01_PRG_BOOTMAP_OFF + 6] == 0x00 && prg[R01_PRG_BOOTMAP_OFF + 7] == 0x10,
           "boot MAP screen0");
    EXPECT(prg[R01_PRG_BOOTMAP_OFF + 9] == 0 && prg[R01_PRG_BOOTMAP_OFF + 12] == 0,
           "boot MAP bgm/world unset");

    {
        uint8_t blob[R01_CART_BGM_BLOB_MAX];
        int nblob;
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
        nblob = r01_bgm_pack_blob(blob, (unsigned)sizeof(blob), &bgm);
        EXPECT(nblob >= (int)R01_PRG_BGM_HDR_V1, "blob length");
        r01_bgm_pack_boot(prg, blob, nblob);
        EXPECT(prg[R01_PRG_BGM_BOOT_OFF] == 0, "boot track is author C");
        EXPECT(blob[0] == R01_PRG_BGM_MAGIC0 && blob[1] == R01_PRG_BGM_MAGIC1, "BG magic");
        EXPECT(blob[2] == 1, "packed track count");
        EXPECT(blob[3] == R01_PRG_BGM_INS_VER, "ins table present");
        EXPECT(blob[R01_PRG_BGM_HDR] == R01_BGM_INS_PIANO, "track 1 ch1 piano");
        EXPECT(blob[R01_PRG_BGM_HDR + 2] == R01_BGM_INS_FLUTE, "track 1 ch3 flute");
        off = (uint16_t)blob[4] | ((uint16_t)blob[5] << 8);
        len = (uint16_t)blob[20] | ((uint16_t)blob[21] << 8);
        EXPECT(off == R01_PRG_BGM_HDR_V1, "payload starts after ins table");
        EXPECT(len > 0, "payload length");
        EXPECT(blob[off] == R01_APU_FD_OP, "FD stream");
        EXPECT(prg[0x00F0] == 'R', "R01P survives BGM pack");
    }

    EXPECT(prg[R01_PRG_PLAY_INST_COUNT_OFF] == 0, "empty instance count");

    {
        const char *cc = R01_REPO_ROOT "/tools/llvm-mos/bin/mos-common-clang";
        struct stat stcc;
        if (stat(cc, &stcc) == 0) {
            char err[256];
            uint8_t keep;
            EXPECT(r01_prg_compile_sdk(NULL, prg, "sdk_overlay.prg", err, sizeof(err)) == 0, "compile sdk prg");
            keep = prg[R01_PRG_C_OFF];
            EXPECT(prg[0] == 0x78, "compiled SEI");
            EXPECT(prg[0x7FFC] == 0x00 && prg[0x7FFD] == 0x80, "compiled RESET");
            r01_prg_overlay_tables(prg, p, &layout);
            EXPECT(prg[R01_PRG_C_OFF] == keep, "overlay keeps C at $C400");
            EXPECT(prg[0] == 0x78, "overlay keeps boot");
            EXPECT(prg[0x7FFC] == 0x00 && prg[0x7FFD] == 0x80, "overlay keeps RESET");
            EXPECT(prg[R01_PRG_BOOTMAP_OFF] == 0x34, "overlay bootmap pal");
            {
                uint8_t mark = 0xABu;
                prg[R01_PRG_PLAT_GRAVITY_OFF] = mark;
                prg[R01_PRG_BGM_BOOT_OFF] = 2;
                r01_prg_overlay_tables(prg, p, &layout);
                EXPECT(prg[R01_PRG_PLAT_GRAVITY_OFF] == mark, "overlay leaves $80F7");
                EXPECT(prg[R01_PRG_BGM_BOOT_OFF] == 2, "overlay leaves $80FE");
            }
        }
    }

    free(prg);
    free(p);
    TEST_EXIT();
}
