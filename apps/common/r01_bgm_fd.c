#include "r01_bgm_fd.h"

#include "r01_apu_tracker.h"
#include "r01_nes_synth.h"

#include <ctype.h>
#include <string.h>

int r01_bgm_fd_frames_per_step(void) {
    int fps =
        (R01_BGM_FD_NMI_HZ * 60) / (R01_BGM_FD_TEMPO_BPM * R01_BGM_FD_STEPS_PER_BEAT);
    if (fps < 1) {
        fps = 1;
    }
    if (fps > 255) {
        fps = 255;
    }
    return fps;
}

int r01_bgm_fd_token_payload(int ch, const char *tok, uint8_t *out) {
    float hz = 0.f;
    int hex = 0;
    char L;
    int i;
    int oct = 4;
    int flat = 0;
    uint8_t letter_nibble;
    if (!out || !tok || !tok[0] || (tok[0] == '-' && tok[1] == '-')) {
        return 0;
    }
    if (ch == 3) {
        if (r01_nes_parse_hex_u8(tok, &hex)) {
            *out = (uint8_t)(0x90u | (hex & 0x0Fu));
            return 1;
        }
    }
    if (ch == 4) {
        if (r01_nes_parse_hex_u8(tok, &hex)) {
            *out = (uint8_t)(0x70u | (hex & 0x0Fu));
            return 1;
        }
        if (r01_nes_parse_note_hz(tok, &hz)) {
            *out = 0x71u;
            return 1;
        }
        return 0;
    }
    if (r01_nes_parse_hex_u8(tok, &hex) && (hex & 0xF0u) == 0x80u) {
        *out = (uint8_t)hex;
        return 1;
    }
    L = (char)toupper((unsigned char)tok[0]);
    switch (L) {
    case 'G':
        letter_nibble = 0x00u;
        break;
    case 'A':
        letter_nibble = 0x0Au;
        break;
    case 'B':
        letter_nibble = 0x0Bu;
        break;
    case 'C':
        letter_nibble = 0x0Cu;
        break;
    case 'D':
        letter_nibble = 0x0Du;
        break;
    case 'E':
        letter_nibble = 0x0Eu;
        break;
    case 'F':
        letter_nibble = 0x0Fu;
        break;
    default:
        return 0;
    }
    i = 1;
    if (tok[i] == '#' || tok[i] == 's' || tok[i] == 'S') {
        i++;
    } else if (tok[i] == 'b') {
        flat = 1;
        i++;
    }
    if (tok[i] >= '0' && tok[i] <= '7') {
        oct = tok[i] - '0';
    }
    *out = (uint8_t)((letter_nibble << 4) | (flat ? 0x08u : 0u) | (oct & 7));
    (void)hz;
    return 1;
}

static int row_payload(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int step, uint8_t *mask,
                       uint8_t *payload, unsigned *n_payload) {
    int ch;
    *mask = 0;
    *n_payload = 0;
    for (ch = 0; ch < R01_BGM_FD_CH; ch++) {
        uint8_t byte = 0;
        if (r01_bgm_fd_token_payload(ch, cells[step][ch], &byte)) {
            *mask = (uint8_t)(*mask | (uint8_t)(1u << ch));
            payload[(*n_payload)++] = byte;
        } else if (ch != 4) {
            *mask = (uint8_t)(*mask | (uint8_t)(1u << ch));
            payload[(*n_payload)++] = 0x80u; /* vol 0 -> off */
        }
    }
    return (*n_payload > 0u) ? 1 : 0;
}

static int rows_equal(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int a, int b) {
    int ch;
    for (ch = 0; ch < R01_BGM_FD_CH; ch++) {
        if (strncmp(cells[a][ch], cells[b][ch], R01_BGM_FD_TOKEN) != 0) {
            return 0;
        }
    }
    return 1;
}

int r01_bgm_fd_apply_step(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int step, int steps,
                          uint8_t *regs) {
    uint8_t mask = 0;
    uint8_t payload[R01_APU_FD_MAX_PAYLOAD];
    unsigned n_payload = 0;
    uint8_t frame[R01_APU_FD_FRAME_MAX];
    int flen;
    if (!cells || !regs || step < 0 || step >= steps) {
        return -1;
    }
    memset(payload, 0, sizeof(payload));
    /* Studio grid row replaces BGM channel state (-- = mute; DPCM sustains). */
    if (!row_payload(cells, step, &mask, payload, &n_payload)) {
        return 0;
    }
    flen = r01_apu_fd_encode(mask, payload, n_payload, frame, sizeof(frame));
    if (flen < 0) {
        return -1;
    }
    return r01_apu_fd_apply(regs, frame, (unsigned)flen);
}

int r01_bgm_fd_encode_cells(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int steps,
                            uint8_t *out, unsigned out_cap) {
    int step;
    unsigned o = 0;
    int frames = r01_bgm_fd_frames_per_step();
    if (!cells || !out || steps < 1) {
        return -1;
    }
    if (steps > R01_BGM_FD_STEPS_MAX) {
        steps = R01_BGM_FD_STEPS_MAX;
    }
    /*
     * Cart / Host Play: FD then FE so each grid step lasts `frames` NMIs @ 60 Hz.
     * Tracker FE n: this NMI + n countdown NMIs = n+1 wait frames after the FD,
     * so FE (frames-2) + FD frame = `frames` total when frames >= 2.
     * Run-length merge identical rows so long regions (e.g. Noise 80 x3, A4 x3)
     * are one FD + one long hold - WAVE/audio keep the section for full duration.
     */
    for (step = 0; step < steps;) {
        uint8_t mask = 0;
        uint8_t payload[R01_APU_FD_MAX_PAYLOAD];
        unsigned n_payload = 0;
        uint8_t frame[R01_APU_FD_FRAME_MAX];
        int flen;
        int run = 1;
        int hold_frames;
        int fe_delay;
        memset(payload, 0, sizeof(payload));
        if (!row_payload(cells, step, &mask, payload, &n_payload)) {
            step++;
            continue;
        }
        while (step + run < steps && rows_equal(cells, step, step + run)) {
            run++;
        }
        flen = r01_apu_fd_encode(mask, payload, n_payload, frame, sizeof(frame));
        if (flen < 0 || o + (unsigned)flen + 3u > out_cap) {
            return -1;
        }
        memcpy(out + o, frame, (size_t)flen);
        o += (unsigned)flen;

        /* After FD: wait (hold_frames-1) NMIs via FE so the row lasts hold_frames total. */
        hold_frames = run * frames;
        {
            int left = hold_frames - 1;
            while (left > 0) {
                int chunk = left > 256 ? 256 : left;
                fe_delay = chunk - 1; /* FE n => this NMI + n countdowns = chunk waits */
                if (o + 2u > out_cap) {
                    return -1;
                }
                out[o++] = R01_APU_CTRL_FE;
                out[o++] = (uint8_t)fe_delay;
                left -= chunk;
            }
        }
        step += run;
    }
    if (o + 1u > out_cap) {
        return -1;
    }
    out[o++] = R01_APU_CTRL_FA; /* loop */
    return (int)o;
}
