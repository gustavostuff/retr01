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

/* Cart note bytes only store natural or flat. C# encodes as D-flat, F# as G-flat. */
static uint8_t midi_to_note_byte(int midi) {
    static const uint8_t let[12] = {0xCu, 0xDu, 0xDu, 0xEu, 0xEu, 0xFu,
                                    0x00u, 0x00u, 0xAu, 0xAu, 0xBu, 0xBu};
    static const uint8_t flat[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    int pc;
    int oct;
    if (midi < 12) {
        midi = 12;
    }
    if (midi > 107) {
        midi = 107;
    }
    pc = midi % 12;
    if (pc < 0) {
        pc += 12;
    }
    oct = midi / 12 - 1;
    if (oct < 0) {
        oct = 0;
    }
    if (oct > 7) {
        oct = 7;
    }
    return (uint8_t)((let[pc] << 4) | (flat[pc] ? 0x08u : 0u) | (uint8_t)(oct & 7));
}

static int parse_melodic_tok(const char *tok, int *out_midi, int *out_minor) {
    char L;
    int i;
    int pc = -1;
    int oct = 4;
    int acc = 0;
    int minor = 0;
    if (!tok || !tok[0] || (tok[0] == '-' && tok[1] == '-')) {
        return 0;
    }
    L = (char)toupper((unsigned char)tok[0]);
    switch (L) {
    case 'C':
        pc = 0;
        break;
    case 'D':
        pc = 2;
        break;
    case 'E':
        pc = 4;
        break;
    case 'F':
        pc = 5;
        break;
    case 'G':
        pc = 7;
        break;
    case 'A':
        pc = 9;
        break;
    case 'B':
        pc = 11;
        break;
    default:
        return 0;
    }
    i = 1;
    if (tok[i] == '#' || tok[i] == 's' || tok[i] == 'S') {
        acc = 1;
        i++;
    } else if (tok[i] == 'b') {
        acc = -1;
        i++;
    }
    if (tok[i] < '0' || tok[i] > '7') {
        return 0;
    }
    oct = tok[i] - '0';
    i++;
    if (tok[i] == 'm' || tok[i] == 'M') {
        minor = 1;
    }
    if (out_midi) {
        *out_midi = (oct + 1) * 12 + pc + acc;
    }
    if (out_minor) {
        *out_minor = minor;
    }
    return 1;
}

int r01_bgm_fd_token_payload(int ch, const char *tok, uint8_t *out) {
    float hz = 0.f;
    int hex = 0;
    int midi = 0;
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
    if (!parse_melodic_tok(tok, &midi, NULL)) {
        return 0;
    }
    *out = midi_to_note_byte(midi);
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

static int row_needs_arp(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int step) {
    int ch;
    for (ch = 0; ch < 3; ch++) {
        int midi = 0;
        int minor = 0;
        if (parse_melodic_tok(cells[step][ch], &midi, &minor) && minor) {
            return 1;
        }
    }
    return 0;
}

static int row_payload_arp(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int step, int phase,
                           uint8_t *mask, uint8_t *payload, unsigned *n_payload) {
    int ch;
    *mask = 0;
    *n_payload = 0;
    for (ch = 0; ch < R01_BGM_FD_CH; ch++) {
        uint8_t byte = 0;
        int midi = 0;
        int minor = 0;
        if (ch < 3 && parse_melodic_tok(cells[step][ch], &midi, &minor) && minor) {
            int add = 0;
            if (phase == 1) {
                add = 3;
            } else if (phase == 2) {
                add = 7;
            }
            byte = midi_to_note_byte(midi + add);
            *mask = (uint8_t)(*mask | (uint8_t)(1u << ch));
            payload[(*n_payload)++] = byte;
        } else if (r01_bgm_fd_token_payload(ch, cells[step][ch], &byte)) {
            *mask = (uint8_t)(*mask | (uint8_t)(1u << ch));
            payload[(*n_payload)++] = byte;
        } else if (ch != 4) {
            *mask = (uint8_t)(*mask | (uint8_t)(1u << ch));
            payload[(*n_payload)++] = 0x80u;
        }
    }
    return (*n_payload > 0u) ? 1 : 0;
}

static int emit_fd_hold(uint8_t *out, unsigned *o, unsigned out_cap, uint8_t mask, const uint8_t *payload,
                        unsigned n_payload, int hold_frames) {
    uint8_t frame[R01_APU_FD_FRAME_MAX];
    int flen;
    int left;
    flen = r01_apu_fd_encode(mask, payload, n_payload, frame, sizeof(frame));
    if (flen < 0 || *o + (unsigned)flen + 3u > out_cap) {
        return -1;
    }
    memcpy(out + *o, frame, (size_t)flen);
    *o += (unsigned)flen;
    left = hold_frames - 1;
    while (left > 0) {
        int chunk = left > 256 ? 256 : left;
        int fe_delay = chunk - 1;
        if (*o + 2u > out_cap) {
            return -1;
        }
        out[(*o)++] = R01_APU_CTRL_FE;
        out[(*o)++] = (uint8_t)fe_delay;
        left -= chunk;
    }
    return 0;
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
        int run = 1;
        int hold_frames;
        memset(payload, 0, sizeof(payload));
        if (!row_payload(cells, step, &mask, payload, &n_payload)) {
            step++;
            continue;
        }
        while (step + run < steps && rows_equal(cells, step, step + run)) {
            run++;
        }
        hold_frames = run * frames;
        if (row_needs_arp(cells, step)) {
            int t = 0;
            while (t < hold_frames) {
                int phase = (t / 4) % 3;
                int chunk = 4;
                if (chunk > hold_frames - t) {
                    chunk = hold_frames - t;
                }
                memset(payload, 0, sizeof(payload));
                mask = 0;
                n_payload = 0;
                if (!row_payload_arp(cells, step, phase, &mask, payload, &n_payload)) {
                    break;
                }
                if (emit_fd_hold(out, &o, out_cap, mask, payload, n_payload, chunk) < 0) {
                    return -1;
                }
                t += chunk;
            }
        } else if (emit_fd_hold(out, &o, out_cap, mask, payload, n_payload, hold_frames) < 0) {
            return -1;
        }
        step += run;
    }
    if (o + 1u > out_cap) {
        return -1;
    }
    out[o++] = R01_APU_CTRL_FA; /* loop */
    return (int)o;
}
