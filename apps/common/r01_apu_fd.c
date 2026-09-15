#include "r01_apu_fd.h"

#include "r01_apu_window.h"

#include <string.h>

static const uint8_t R01_APU_FD_DEF_WAVE[R01_APU_CH_N] = {
    R01_APU_WAVE_PULSE,    R01_APU_WAVE_PULSE, R01_APU_WAVE_TRIANGLE, R01_APU_WAVE_NOISE,
    R01_APU_WAVE_DPCM,     R01_APU_WAVE_PULSE, R01_APU_WAVE_PULSE,    R01_APU_WAVE_NOISE,
};

void r01_apu_fd_pack_voice(uint8_t *regs, uint8_t ch, uint8_t enable, uint8_t vol, uint8_t duty,
                           uint8_t wave, uint16_t period) {
    uint8_t b;
    if (!regs || ch >= R01_APU_CH_N) {
        return;
    }
    b = r01_apu_ch_base(ch);
    regs[b] = (uint8_t)((enable ? R01_APU_CTRL_ENABLE : 0u) | ((vol & 0x0Fu) << R01_APU_CTRL_VOL_SHIFT));
    regs[b + 1u] = (uint8_t)(period & 0xFFu);
    regs[b + 2u] = (uint8_t)(((period >> 8) & 0x07u) | ((duty & 0x03u) << 4));
    regs[b + 3u] = (uint8_t)(wave & 0x03u);
}

uint16_t r01_apu_fd_note_period(uint8_t note) {
    /* High nibble letter (0=G,A-F), low nibble octave (+8 = flat). Rough Host Play table. */
    static const int semitone[16] = {
        /* 0=G */ 7, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* A */ 9, /* B */ 11, /* C */ 0, /* D */ 2, /* E */ 4, /* F */ 5};
    int letter = (note >> 4) & 0x0F;
    int flat = (note & 0x08) ? 1 : 0;
    int oct = (int)(note & 0x07u);
    int st;
    int midi;
    float hz;
    int p;

    if (letter > 0 && letter < 0x0A) {
        return 32u; /* reserved control - keep a short tick */
    }
    st = semitone[letter];
    if (flat) {
        st -= 1;
    }
    midi = (oct + 1) * 12 + st; /* C0 ~ midi 12 */
    if (midi < 12) {
        midi = 12;
    }
    if (midi > 96) {
        midi = 96;
    }
    /* period ~= 22050 / hz (viz_hz_to_period style) */
    hz = 440.f;
    {
        int d = midi - 69;
        /* 2^(d/12) approx via integer steps */
        while (d > 0) {
            hz *= 1.059463f;
            d--;
        }
        while (d < 0) {
            hz *= 0.943874f;
            d++;
        }
    }
    if (hz < 20.f) {
        return 0;
    }
    p = (int)(22050.f / hz + 0.5f);
    if (p < 2) {
        p = 2;
    }
    if (p > 2047) {
        p = 2047;
    }
    return (uint16_t)p;
}

int r01_apu_fd_encode(uint8_t mask, const uint8_t *payload, unsigned n_payload, uint8_t *out,
                      unsigned out_cap) {
    unsigned n = 0;
    unsigned i;
    uint8_t m;
    if (!out) {
        return -1;
    }
    for (m = mask; m; m = (uint8_t)(m & (uint8_t)(m - 1u))) {
        n++;
    }
    if (n != n_payload || n > R01_APU_FD_MAX_PAYLOAD) {
        return -1;
    }
    if (out_cap < 2u + n) {
        return -1;
    }
    out[0] = R01_APU_FD_OP;
    out[1] = mask;
    for (i = 0; i < n; i++) {
        out[2u + i] = payload ? payload[i] : 0;
    }
    return (int)(2u + n);
}

static void apply_payload(uint8_t *regs, uint8_t ch, uint8_t byte) {
    uint8_t b = r01_apu_ch_base(ch);
    uint8_t hi = (uint8_t)(byte & 0xF0u);
    uint8_t lo = (uint8_t)(byte & 0x0Fu);
    uint8_t vol = r01_apu_ch_vol(regs, ch);
    uint8_t duty = r01_apu_ch_duty(regs, ch);
    uint8_t wave = r01_apu_ch_wave(regs, ch);
    uint16_t per = r01_apu_ch_period(regs, ch);
    uint8_t en = r01_apu_ch_enabled(regs, ch) ? 1u : 0u;

    if (wave == 0 && regs[b + 3u] == 0 && !en && per == 0) {
        wave = R01_APU_FD_DEF_WAVE[ch];
    }

    if (hi == 0x80u) {
        vol = lo;
        en = (vol > 0u) ? 1u : 0u;
        if (en == 0u) {
            per = 0;
        }
    } else if (hi == 0x90u) {
        duty = (uint8_t)(lo & 0x03u);
        if (ch == 3u) {
            wave = R01_APU_WAVE_NOISE;
            per = (uint16_t)(8u + (unsigned)lo * 4u);
            en = 1u;
            if (vol == 0u) {
                vol = 10u;
            }
        }
    } else if (hi == 0x70u) {
        if (ch == 4u) {
            wave = R01_APU_WAVE_DPCM;
            en = 1u;
            vol = 12u;
            per = (lo != 0u) ? 40u : 24u;
        }
    } else {
        per = r01_apu_fd_note_period(byte);
        en = (per >= 2u) ? 1u : 0u;
        if (vol == 0u) {
            vol = (ch == 2u) ? 15u : 12u;
        }
        wave = R01_APU_FD_DEF_WAVE[ch];
        if (ch == 0u) {
            duty = 2u;
        } else if (ch == 1u) {
            duty = 1u;
        }
    }
    r01_apu_fd_pack_voice(regs, ch, en, vol, duty, wave, per);
}

int r01_apu_fd_apply(uint8_t *regs, const uint8_t *stream, unsigned len) {
    unsigned i = 0;
    if (!regs || !stream) {
        return -1;
    }
    while (i < len) {
        uint8_t mask;
        unsigned need = 0;
        uint8_t m;
        uint8_t ch;
        if (stream[i] != R01_APU_FD_OP) {
            return -1;
        }
        if (i + 1u >= len) {
            return -1;
        }
        mask = stream[i + 1u];
        for (m = mask; m; m = (uint8_t)(m & (uint8_t)(m - 1u))) {
            need++;
        }
        if (i + 2u + need > len) {
            return -1;
        }
        i += 2u;
        for (ch = 0; ch < R01_APU_CH_N; ch++) {
            if (mask & (uint8_t)(1u << ch)) {
                apply_payload(regs, ch, stream[i++]);
            }
        }
    }
    return (int)i;
}
