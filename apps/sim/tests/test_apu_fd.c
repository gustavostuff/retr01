#include "r01_apu_fd.h"
#include "r01_apu_window.h"
#include "test_common.h"

#include <string.h>

int main(void) {
    uint8_t regs[R01_APU_REGS];
    uint8_t frame[R01_APU_FD_FRAME_MAX];
    uint8_t payload[3];
    int n;

    memset(regs, 0, sizeof(regs));
    payload[0] = 0xC4; /* ch0 note C4 */
    payload[1] = 0x8C; /* ch1 vol 12 */
    payload[2] = 0x71; /* ch4 DPCM id 1 */
    n = r01_apu_fd_encode(0x13u, payload, 3, frame, sizeof(frame));
    expect_true(n == 5, "encode length");
    expect_true(frame[0] == R01_APU_FD_OP, "FD op");
    expect_true(frame[1] == 0x13u, "mask");

    expect_true(r01_apu_fd_apply(regs, frame, (unsigned)n) == n, "apply");
    expect_true(r01_apu_ch_enabled(regs, 0), "ch0 on");
    expect_true(r01_apu_ch_wave(regs, 0) == R01_APU_WAVE_PULSE, "ch0 pulse");
    expect_true(r01_apu_ch_period(regs, 0) >= 2u, "ch0 period");
    expect_true(r01_apu_ch_vol(regs, 1) == 12u, "ch1 vol");
    expect_true(r01_apu_ch_enabled(regs, 4), "ch4 dpcm");
    expect_true(r01_apu_ch_wave(regs, 4) == R01_APU_WAVE_DPCM, "ch4 wave");

    /* vol 0 mutes */
    {
        uint8_t mute_frame[4] = {R01_APU_FD_OP, 0x01u, 0x80u};
        expect_true(r01_apu_fd_apply(regs, mute_frame, 3) == 3, "mute apply");
        expect_true(!r01_apu_ch_enabled(regs, 0), "vol0 clears enable");
    }

    r01_apu_fd_pack_voice(regs, 2, 1, 15, 0, R01_APU_WAVE_TRIANGLE, 40);
    expect_true(r01_apu_ch_period(regs, 2) == 40u, "pack period");
    expect_true(r01_apu_fd_note_period(0xC4) >= 2u, "note period");

    return test_done("test_apu_fd");
}
