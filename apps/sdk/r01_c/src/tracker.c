#include "r01_engine.h"

#include "r01_apu_cart.h"

#ifndef R01_HOST_TEST

#define R01_BGM_RAM_MAX 4096u
#define R01_APU_FD_OP 0xFDu
#define R01_APU_CTRL_FE 0xFEu
#define R01_APU_CTRL_FA 0xFAu
#define R01_APU_CTRL_FB 0xFBu

static const uint8_t k_def_wave[8] = {0, 0, 1, 2, 3, 0, 0, 2};

static uint8_t s_regs[32];
static uint8_t s_bgm[R01_BGM_RAM_MAX];
static uint16_t s_len;
static uint16_t s_pc;
static uint8_t s_delay;
static uint8_t s_active;
static uint8_t s_loop;
static uint8_t s_dirty;

static void pack_voice(uint8_t ch, uint8_t en, uint8_t vol, uint8_t duty, uint8_t wave, uint16_t per) {
    uint8_t b = (uint8_t)(ch * 4u);
    s_regs[b] = (uint8_t)((en ? 0x01u : 0u) | ((vol & 0x0Fu) << 4));
    if ((wave & 0x03u) == 3u) {
        s_regs[b + 1u] = (uint8_t)(per & 0xFFu);
        s_regs[b + 2u] = (uint8_t)((duty & 0x03u) << 4);
    } else {
        s_regs[b + 1u] = (uint8_t)(per & 0xFFu);
        s_regs[b + 2u] = (uint8_t)(((per >> 8) & 0x07u) | ((duty & 0x03u) << 4));
    }
    s_regs[b + 3u] = (uint8_t)(wave & 0x03u);
    s_dirty = 1;
}

static uint16_t note_period(uint8_t note) {
    /* C0..B0 at 22050 Hz clock, then shift down by octave. */
    static const uint16_t c0[12] = {1348u, 1273u, 1201u, 1134u, 1070u, 1010u,
                                    954u,  900u,  850u,  802u,  757u,  714u};
    static const int8_t semitone[16] = {7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 11, 0, 2, 4, 5};
    int letter = (note >> 4) & 0x0F;
    int flat = (note & 0x08) ? 1 : 0;
    int oct = (int)(note & 0x07u);
    int st;
    uint16_t p;
    if (letter > 0 && letter < 0x0A) {
        return 32u;
    }
    st = (int)semitone[letter] - flat;
    if (st < 0) {
        st += 12;
        oct -= 1;
    }
    if (st > 11) {
        st = 11;
    }
    if (oct < 0) {
        oct = 0;
    }
    p = c0[st];
    while (oct > 0 && p > 2u) {
        p = (uint16_t)(p >> 1);
        oct--;
    }
    if (p < 2u) {
        p = 2u;
    }
    if (p > 2047u) {
        p = 2047u;
    }
    return p;
}

static uint8_t ch_vol(uint8_t ch) {
    return (uint8_t)((s_regs[ch * 4u] >> 4) & 0x0Fu);
}

static uint8_t ch_duty(uint8_t ch) {
    return (uint8_t)((s_regs[ch * 4u + 2u] >> 4) & 0x03u);
}

static uint8_t ch_wave(uint8_t ch) {
    return (uint8_t)(s_regs[ch * 4u + 3u] & 0x03u);
}

static uint16_t ch_period(uint8_t ch) {
    uint8_t b = (uint8_t)(ch * 4u);
    return (uint16_t)((s_regs[b + 1u] | ((s_regs[b + 2u] & 0x07u) << 8)) & 0x7FFu);
}

static uint8_t ch_en(uint8_t ch) {
    return (uint8_t)(s_regs[ch * 4u] & 0x01u);
}

static void apply_payload(uint8_t ch, uint8_t byte) {
    uint8_t hi = (uint8_t)(byte & 0xF0u);
    uint8_t lo = (uint8_t)(byte & 0x0Fu);
    uint8_t vol = ch_vol(ch);
    uint8_t duty = ch_duty(ch);
    uint8_t wave = ch_wave(ch);
    uint16_t per = ch_period(ch);
    uint8_t en = ch_en(ch);
    if (wave == 0 && s_regs[ch * 4u + 3u] == 0 && !en && per == 0) {
        wave = k_def_wave[ch];
    }
    if (hi == 0x80u) {
        vol = lo;
        en = (vol > 0u) ? 1u : 0u;
        if (en == 0u) {
            per = 0;
        }
    } else if (hi == 0x90u) {
        duty = (uint8_t)(lo & 0x03u);
        if (ch == 3u || k_def_wave[ch] == 2u) {
            wave = 2u;
            per = (uint16_t)(8u + (unsigned)lo * 4u);
            en = 1u;
            if (vol == 0u) {
                vol = 10u;
            }
        }
    } else if (hi == 0x70u) {
        if (ch == 4u) {
            wave = 3u;
            en = 1u;
            vol = 12u;
            per = lo;
        }
    } else {
        per = note_period(byte);
        en = (per >= 2u) ? 1u : 0u;
        if (vol == 0u) {
            vol = (ch == 2u) ? 15u : 12u;
        }
        wave = k_def_wave[ch];
        if (ch == 0u) {
            duty = 2u;
        } else if (ch == 1u) {
            duty = 1u;
        }
    }
    pack_voice(ch, en, vol, duty, wave, per);
}

static void mute_mask(uint8_t mask) {
    uint8_t ch;
    for (ch = 0; ch < 8u; ch++) {
        if (mask & (uint8_t)(1u << ch)) {
            pack_voice(ch, 0, 0, 0, ch_wave(ch), 0);
        }
    }
}

static void publish_regs(void) {
    uint8_t i;
    for (i = 0; i < 32u; i++) {
        R01_APU[i] = s_regs[i];
    }
    s_dirty = 0;
}

static uint8_t stream_at(uint16_t pc) {
    if (pc >= s_len) {
        return 0;
    }
    return s_bgm[pc];
}

static void stream_tick(void) {
    uint8_t op;
    if (!s_active) {
        return;
    }
    if (s_delay > 0u) {
        s_delay--;
        return;
    }
    if (s_pc >= s_len) {
        if (s_loop) {
            s_pc = 0;
        } else {
            s_active = 0;
            mute_mask(0x1Fu);
        }
        return;
    }
    op = stream_at(s_pc++);
    if (op == R01_APU_CTRL_FE) {
        if (s_pc >= s_len) {
            s_active = 0;
            return;
        }
        s_delay = stream_at(s_pc++);
        return;
    }
    if (op == R01_APU_CTRL_FA) {
        s_loop = 1;
        s_pc = 0;
        if (s_len > 0u && stream_at(0) != R01_APU_CTRL_FA) {
            stream_tick();
        }
        return;
    }
    if (op == R01_APU_CTRL_FB) {
        s_active = 0;
        mute_mask(0x1Fu);
        return;
    }
    if (op == R01_APU_FD_OP) {
        uint8_t mask;
        uint8_t ch;
        if (s_pc >= s_len) {
            s_active = 0;
            return;
        }
        mask = stream_at(s_pc++);
        for (ch = 0; ch < 8u; ch++) {
            if (mask & (uint8_t)(1u << ch)) {
                if (s_pc >= s_len) {
                    s_active = 0;
                    return;
                }
                apply_payload(ch, stream_at(s_pc++));
            }
        }
        return;
    }
}

void r01_tracker_boot(void) {
    uint32_t bgm = r01_boot_u24(9);
    uint8_t boot;
    uint8_t magic0;
    uint8_t magic1;
    uint8_t ver;
    uint16_t off;
    uint16_t len;
    uint16_t i;
    uint16_t hdr = R01_BGM_HDR;

    s_active = 0;
    s_len = 0;
    s_pc = 0;
    s_delay = 0;
    s_loop = 1;
    s_dirty = 0;
    if (bgm == 0u) {
        return;
    }
    r01_map_seek(bgm);
    magic0 = r01_map_read();
    magic1 = r01_map_read();
    (void)r01_map_read();
    ver = r01_map_read();
    if (magic0 != R01_BGM_MAGIC0 || magic1 != R01_BGM_MAGIC1) {
        return;
    }
    if (ver == R01_BGM_INS_VER) {
        hdr = (uint16_t)R01_BGM_HDR_V1;
    }
    boot = *(volatile uint8_t *)(uint16_t)0x80FEu;
    if (boot < 1u || boot > R01_BGM_TRACKS) {
        return;
    }
    r01_map_seek(bgm + 4u + (uint32_t)(boot - 1u) * 2u);
    off = r01_map_read();
    off |= (uint16_t)r01_map_read() << 8;
    r01_map_seek(bgm + 20u + (uint32_t)(boot - 1u) * 2u);
    len = r01_map_read();
    len |= (uint16_t)r01_map_read() << 8;
    if (off < hdr || len == 0u) {
        return;
    }
    if (len > R01_BGM_RAM_MAX) {
        len = R01_BGM_RAM_MAX;
    }
    r01_map_seek(bgm + off);
    for (i = 0; i < len; i++) {
        s_bgm[i] = r01_map_read();
    }
    s_len = len;
    s_active = 1;
    s_loop = 1;
}

void r01_tracker_nmi(void) {
    /* One stream opcode per NMI (FA may recurse once). Then publish $7F40. */
    if (s_active) {
        stream_tick();
        if (s_dirty) {
            publish_regs();
        }
    }
}

#else
void r01_tracker_boot(void) {
}
void r01_tracker_nmi(void) {
}
#endif
