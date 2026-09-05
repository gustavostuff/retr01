#include "r01_bgm_host.h"
#include "r01_nes_synth.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define R01_BGM_AUDIO_RATE 44100
#define R01_BGM_TEMPO_BPM 140
#define R01_BGM_STEPS_PER_BEAT 4

typedef struct R01BgmHost {
    SDL_AudioDeviceID dev;
    R01NesSynth synth;
    int playing;
    int step;
    int samples_left;
    int samples_per_step;
    int track_steps;
    char cell[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
} R01BgmHost;

static R01BgmHost g_bgm;

/* Built-in demo matching Studio Audio Track 1 placeholders. */
static void load_builtin_track1(R01BgmHost *a) {
    static const char *demo[][R01_BGM_CH] = {
        {"C4", "E4", "G3", "--", "--"}, {"--", "--", "--", "8F", "--"}, {"D4", "F4", "A3", "--", "FD"},
        {"--", "--", "--", "--", "--"}, {"E4", "G4", "B3", "--", "--"}, {"--", "--", "G3", "8F", "--"},
        {"C4", "--", "--", "--", "--"}, {"--", "E4", "--", "--", "--"},
    };
    int r, c;
    memset(a->cell, 0, sizeof(a->cell));
    for (r = 0; r < R01_BGM_STEPS; r++) {
        for (c = 0; c < R01_BGM_CH; c++) {
            snprintf(a->cell[r][c], R01_BGM_TOKEN, "--");
        }
    }
    for (r = 0; r < (int)(sizeof(demo) / sizeof(demo[0])); r++) {
        for (c = 0; c < R01_BGM_CH; c++) {
            snprintf(a->cell[r][c], R01_BGM_TOKEN, "%s", demo[r][c]);
        }
    }
    a->track_steps = R01_BGM_STEPS;
}

static void apply_step(R01BgmHost *a, int step) {
    int ch;
    if (step < 0 || step >= a->track_steps) {
        return;
    }
    for (ch = 0; ch < R01_BGM_CH; ch++) {
        const char *tok = a->cell[step][ch];
        float hz = 0.f;
        int hex = 0;
        if (!tok || !tok[0] || (tok[0] == '-' && tok[1] == '-')) {
            if (ch != 4) {
                r01_nes_synth_off(&a->synth, ch);
            }
            continue;
        }
        if (ch == 0 || ch == 1) {
            if (r01_nes_parse_note_hz(tok, &hz)) {
                r01_nes_synth_set_pulse(&a->synth, ch, hz, 12, ch == 0 ? 2 : 1);
            } else {
                r01_nes_synth_off(&a->synth, ch);
            }
        } else if (ch == 2) {
            if (r01_nes_parse_note_hz(tok, &hz)) {
                r01_nes_synth_set_triangle(&a->synth, hz);
            } else {
                r01_nes_synth_off(&a->synth, ch);
            }
        } else if (ch == 3) {
            if (r01_nes_parse_hex_u8(tok, &hex)) {
                r01_nes_synth_set_noise(&a->synth, hex & 0x0f, 10);
            } else if (r01_nes_parse_note_hz(tok, &hz)) {
                r01_nes_synth_set_noise(&a->synth, 8, 10);
            } else {
                r01_nes_synth_off(&a->synth, ch);
            }
        } else if (ch == 4) {
            if (r01_nes_parse_hex_u8(tok, &hex) || r01_nes_parse_note_hz(tok, &hz)) {
                r01_nes_synth_trigger_dpcm(&a->synth, (hex == 0xFD || hz > 0.f) ? 0 : 1);
            }
        }
    }
}

static void SDLCALL bgm_audio_cb(void *userdata, Uint8 *stream, int len) {
    R01BgmHost *a = (R01BgmHost *)userdata;
    int16_t *out = (int16_t *)stream;
    int frames = len / (int)sizeof(int16_t);
    int i = 0;
    if (!a || !a->playing) {
        memset(stream, 0, (size_t)len);
        return;
    }
    while (i < frames) {
        int n;
        if (a->samples_left <= 0) {
            a->step++;
            if (a->step >= a->track_steps) {
                a->step = 0;
            }
            apply_step(a, a->step);
            a->samples_left = a->samples_per_step;
        }
        n = a->samples_left;
        if (n > frames - i) {
            n = frames - i;
        }
        r01_nes_synth_render(&a->synth, out + i, n);
        a->samples_left -= n;
        i += n;
    }
}

int r01_bgm_host_init(void) {
    SDL_AudioSpec want, have;
    if (g_bgm.dev) {
        return 0;
    }
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            fprintf(stderr, "SDL_InitSubSystem(AUDIO): %s\n", SDL_GetError());
            return -1;
        }
    }
    memset(&want, 0, sizeof(want));
    want.freq = R01_BGM_AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = bgm_audio_cb;
    want.userdata = &g_bgm;
    g_bgm.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!g_bgm.dev) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return -1;
    }
    r01_nes_synth_init(&g_bgm.synth, have.freq > 0 ? have.freq : R01_BGM_AUDIO_RATE);
    g_bgm.samples_per_step =
        ((have.freq > 0 ? have.freq : R01_BGM_AUDIO_RATE) * 60) / (R01_BGM_TEMPO_BPM * R01_BGM_STEPS_PER_BEAT);
    if (g_bgm.samples_per_step < 256) {
        g_bgm.samples_per_step = 256;
    }
    SDL_PauseAudioDevice(g_bgm.dev, 1);
    return 0;
}

void r01_bgm_host_shutdown(void) {
    r01_bgm_host_stop();
    if (g_bgm.dev) {
        SDL_CloseAudioDevice(g_bgm.dev);
        g_bgm.dev = 0;
    }
}

static int load_track_file(R01BgmHost *a, const char *path) {
    FILE *f;
    size_t need = (size_t)R01_BGM_STEPS * R01_BGM_CH * R01_BGM_TOKEN;
    size_t n;
    if (!path || !path[0]) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    n = fread(a->cell, 1, need, f);
    fclose(f);
    if (n != need) {
        return -1;
    }
    a->track_steps = R01_BGM_STEPS;
    return 0;
}

int r01_bgm_host_play(int track, const char *path) {
    if (track < 1) {
        return -1;
    }
    if (r01_bgm_host_init() != 0) {
        return -1;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    if (path && path[0] && load_track_file(&g_bgm, path) == 0) {
        /* ok */
    } else if (track == 1) {
        load_builtin_track1(&g_bgm);
    } else {
        SDL_UnlockAudioDevice(g_bgm.dev);
        return -1;
    }
    g_bgm.step = -1;
    g_bgm.samples_left = 0;
    r01_nes_synth_silence(&g_bgm.synth);
    g_bgm.playing = 1;
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
    return 0;
}

void r01_bgm_host_play_cells(char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN]) {
    if (r01_bgm_host_init() != 0 || !cells) {
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    memcpy(g_bgm.cell, cells, sizeof(g_bgm.cell));
    g_bgm.track_steps = R01_BGM_STEPS;
    g_bgm.step = -1;
    g_bgm.samples_left = 0;
    r01_nes_synth_silence(&g_bgm.synth);
    g_bgm.playing = 1;
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
}

void r01_bgm_host_stop(void) {
    if (!g_bgm.dev) {
        g_bgm.playing = 0;
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    g_bgm.playing = 0;
    r01_nes_synth_silence(&g_bgm.synth);
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 1);
}

int r01_bgm_host_playing(void) {
    return g_bgm.playing;
}

int r01_bgm_host_step(void) {
    int step;
    if (!g_bgm.dev || !g_bgm.playing) {
        return -1;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    step = g_bgm.step;
    SDL_UnlockAudioDevice(g_bgm.dev);
    return step;
}
