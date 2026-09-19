#include "r01_apu_fd.h"
#include "r01_apu_mix.h"
#include "r01_apu_tracker.h"
#include "r01_apu_window.h"
#include "r01_bgm_fd.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *m) {
    fprintf(stderr, "test_apu: %s\n", m);
    return 1;
}

int main(void) {
    uint8_t regs[R01_APU_REGS];
    uint8_t frame[R01_APU_FD_FRAME_MAX];
    uint8_t sfx[16];
    uint8_t payload;
    int n;
    int i;
    int energy;
    int16_t pcm[256];
    R01ApuMix mix;
    R01ApuTracker tr;

    memset(regs, 0, sizeof(regs));
    payload = 0x71u;
    n = r01_apu_fd_encode(0x10u, &payload, 1u, frame, sizeof(frame));
    if (n < 0 || r01_apu_fd_apply(regs, frame, (unsigned)n) < 0) {
        return fail("dpcm fd apply");
    }
    if (r01_apu_ch_wave(regs, 4) != R01_APU_WAVE_DPCM || r01_apu_ch_dpcm_id(regs, 4) != 1u) {
        return fail("dpcm id in [1]");
    }
    if (!r01_apu_ch_enabled(regs, 4)) {
        return fail("dpcm enable");
    }

    memset(regs, 0, sizeof(regs));
    r01_apu_fd_pack_voice(regs, 0, 1, 12, 2, R01_APU_WAVE_PULSE, r01_apu_fd_note_period(0xC4u));
    r01_apu_mix_init(&mix, 44100);
    r01_apu_mix_set_regs(&mix, regs);
    r01_apu_mix_render(&mix, pcm, 256);
    energy = 0;
    for (i = 0; i < 256; i++) {
        if (pcm[i] > 80 || pcm[i] < -80) {
            energy = 1;
            break;
        }
    }
    if (!energy) {
        return fail("mix pulse energy");
    }

    n = r01_apu_sfx_encode(R01_APU_SFX_X, sfx, sizeof(sfx));
    if (n < 4 || sfx[0] != R01_APU_FD_OP || sfx[1] != 0x20u) {
        return fail("sfx x mask ch6");
    }

    memset(regs, 0, sizeof(regs));
    r01_apu_tracker_init(&tr);
    r01_apu_fd_pack_voice(regs, 0, 1, 12, 2, R01_APU_WAVE_PULSE, 32);
    r01_apu_tracker_trigger_sfx(&tr, sfx, (uint16_t)n);
    (void)r01_apu_tracker_nmi(&tr, regs);
    if (!r01_apu_ch_enabled(regs, 0)) {
        return fail("sfx must not mute bgm");
    }
    if (!r01_apu_ch_enabled(regs, 5)) {
        return fail("sfx ch6 enable");
    }

    {
        uint8_t b_c = 0;
        uint8_t b_cs = 0;
        uint16_t p_c;
        uint16_t p_cs;
        if (!r01_bgm_fd_token_payload(0, "C4", &b_c) || !r01_bgm_fd_token_payload(0, "C#4", &b_cs)) {
            return fail("token C4 / C#4");
        }
        if (b_c == b_cs) {
            return fail("C# must not encode as C");
        }
        p_c = r01_apu_fd_note_period(b_c);
        p_cs = r01_apu_fd_note_period(b_cs);
        if (p_cs >= p_c) {
            return fail("C# period should be shorter than C");
        }
        if (b_cs != 0xDCu) {
            return fail("C#4 encodes as D-flat 4");
        }
    }
    return 0;
}
