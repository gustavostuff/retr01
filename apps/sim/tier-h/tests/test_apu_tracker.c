#include "r01_apu_tracker.h"
#include "r01_apu_window.h"
#include "r01_bgm_fd.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    R01ApuTracker t;
    uint8_t regs[R01_APU_REGS];
    /* FD ch0 C4, FE 01, FD ch0 C5, FB */
    static const uint8_t bgm[] = {
        R01_APU_FD_OP, 0x01u, 0xC4u, R01_APU_CTRL_FE, 0x01u, R01_APU_FD_OP, 0x01u, 0xC5u, R01_APU_CTRL_FB,
    };
    static const uint8_t sfx[] = {
        R01_APU_FD_OP, 0x20u, 0xC4u, R01_APU_CTRL_FB, /* ch6 (=bit5) */
    };
    uint16_t p0;
    int frames;
    int i;

    memset(regs, 0, sizeof(regs));
    r01_apu_tracker_init(&t);
    r01_apu_tracker_set_bgm(&t, bgm, (uint16_t)sizeof(bgm));

    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "nmi FD C4");
    expect_true(r01_apu_ch_enabled(regs, 0), "ch0 after FD");
    p0 = r01_apu_ch_period(regs, 0);

    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "nmi FE");
    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "nmi delay countdown");
    expect_true(r01_apu_ch_period(regs, 0) == p0, "period held during FE");

    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "nmi FD C5");
    expect_true(r01_apu_ch_period(regs, 0) == r01_apu_fd_note_period(0xC5), "C5 period");

    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "nmi FB");
    expect_true(!r01_apu_ch_enabled(regs, 0), "muted after FB");

    r01_apu_tracker_trigger_sfx(&t, sfx, (uint16_t)sizeof(sfx));
    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "sfx FD");
    expect_true(r01_apu_ch_enabled(regs, 5), "sfx ch6");
    expect_true(r01_apu_tracker_nmi(&t, regs) != 0, "sfx FB");
    expect_true(!r01_apu_ch_enabled(regs, 5), "sfx muted");

    /* Long region: 3 identical Noise "80" steps → one FD + FE hold for 3 grid steps. */
    {
        char cells[4][R01_BGM_FD_CH][R01_BGM_FD_TOKEN];
        uint8_t bc[256];
        int n;
        int held = 0;
        frames = r01_bgm_fd_frames_per_step();
        memset(cells, 0, sizeof(cells));
        for (i = 0; i < 4; i++) {
            int c;
            for (c = 0; c < R01_BGM_FD_CH; c++) {
                snprintf(cells[i][c], R01_BGM_FD_TOKEN, "%s", (c == 3 && i < 3) ? "80" : "--");
            }
        }
        n = r01_bgm_fd_encode_cells((const char(*)[R01_BGM_FD_CH][R01_BGM_FD_TOKEN])cells, 4, bc,
                                    sizeof(bc));
        expect_true(n > 4, "encode long noise");
        memset(regs, 0, sizeof(regs));
        r01_apu_tracker_init(&t);
        r01_apu_tracker_set_bgm(&t, bc, (uint16_t)n);
        for (i = 0; i < frames * 3; i++) {
            (void)r01_apu_tracker_nmi(&t, regs);
            if (r01_apu_ch_enabled(regs, 3)) {
                held++;
            }
        }
        expect_true(held == frames * 3, "noise held for full 3-step region");
        (void)r01_apu_tracker_nmi(&t, regs); /* step 3 mutes */
        expect_true(!r01_apu_ch_enabled(regs, 3), "noise off after region");
    }

    return test_done("test_apu_tracker");
}
