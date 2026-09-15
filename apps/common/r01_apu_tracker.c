#include "r01_apu_tracker.h"

#include "r01_apu_window.h"

#include <string.h>

static void mute_mask(uint8_t *regs, uint8_t mask) {
    uint8_t ch;
    if (!regs) {
        return;
    }
    for (ch = 0; ch < R01_APU_CH_N; ch++) {
        if (mask & (uint8_t)(1u << ch)) {
            r01_apu_fd_pack_voice(regs, ch, 0, 0, 0, r01_apu_ch_wave(regs, ch), 0);
        }
    }
}

static void stream_reset(R01ApuStreamSm *s, const uint8_t *rom, uint16_t len) {
    s->rom = rom;
    s->len = len;
    s->pc = 0;
    s->delay = 0;
    s->active = (rom && len > 0) ? 1u : 0u;
    s->loop = 1u;
}

void r01_apu_tracker_init(R01ApuTracker *t) {
    if (!t) {
        return;
    }
    memset(t, 0, sizeof(*t));
}

void r01_apu_tracker_set_bgm(R01ApuTracker *t, const uint8_t *rom, uint16_t len) {
    if (!t) {
        return;
    }
    stream_reset(&t->bgm, rom, len);
}

void r01_apu_tracker_trigger_sfx(R01ApuTracker *t, const uint8_t *rom, uint16_t len) {
    if (!t) {
        return;
    }
    stream_reset(&t->sfx, rom, len);
    t->sfx.loop = 0u; /* SFX play-once unless FA */
}

static int stream_tick(R01ApuStreamSm *s, uint8_t *regs, uint8_t family_mask) {
    uint8_t op;
    if (!s || !s->active || !s->rom) {
        return 0;
    }
    if (s->delay > 0) {
        s->delay--;
        return 1;
    }
    if (s->pc >= s->len) {
        if (s->loop) {
            s->pc = 0;
        } else {
            s->active = 0;
            mute_mask(regs, family_mask);
        }
        return 1;
    }
    op = s->rom[s->pc++];
    if (op == R01_APU_CTRL_FE) {
        if (s->pc >= s->len) {
            s->active = 0;
            return 1;
        }
        s->delay = s->rom[s->pc++];
        return 1;
    }
    if (op == R01_APU_CTRL_FA) {
        s->loop = 1u;
        s->pc = 0;
        /* Rewind without burning an NMI — keep long regions tight at loop points. */
        if (s->len > 0u && s->rom[0] != R01_APU_CTRL_FA) {
            return stream_tick(s, regs, family_mask);
        }
        return 1;
    }
    if (op == R01_APU_CTRL_FB) {
        s->active = 0;
        mute_mask(regs, family_mask);
        return 1;
    }
    if (op == R01_APU_FD_OP) {
        uint8_t mask;
        unsigned need = 0;
        uint8_t m;
        unsigned frame_len;
        int used;
        if (s->pc >= s->len) {
            s->active = 0;
            return 1;
        }
        mask = s->rom[s->pc];
        for (m = mask; m; m = (uint8_t)(m & (uint8_t)(m - 1u))) {
            need++;
        }
        frame_len = 2u + need;
        if ((uint16_t)(s->pc - 1u) + frame_len > s->len) {
            s->active = 0;
            return 1;
        }
        used = r01_apu_fd_apply(regs, &s->rom[s->pc - 1u], frame_len);
        if (used < 0) {
            s->active = 0;
            return 1;
        }
        s->pc = (uint16_t)(s->pc - 1u + (uint16_t)used);
        return 1;
    }
    /* Unknown byte: skip */
    return 1;
}

int r01_apu_tracker_nmi(R01ApuTracker *t, uint8_t *regs) {
    int advanced = 0;
    if (!t || !regs) {
        return 0;
    }
    advanced |= stream_tick(&t->bgm, regs, R01_APU_BGM_MASK);
    advanced |= stream_tick(&t->sfx, regs, R01_APU_SFX_MASK);
    return advanced;
}
