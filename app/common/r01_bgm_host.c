#include "r01_bgm_host.h"
#include "r01_nes_synth.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define R01_BGM_AUDIO_RATE 44100
#define R01_BGM_TEMPO_BPM 140
#define R01_BGM_STEPS_PER_BEAT 4
/* Master host level: BGM and SFX share one gain (full softsynth / 4). */
#define R01_HOST_MIX_GAIN 0.25f
/* Peak matches ~pulse voice in r01_nes_synth so SFX ≈ BGM before master gain. */
#define R01_SFX_AMP 2000

typedef struct R01SfxVoice {
    int active;
    int samples_left;
    int kind; /* R01_SFX_X or R01_SFX_Y */
    double phase;
    float freq_hz;
    uint16_t noise_lfsr;
} R01SfxVoice;

typedef struct R01BgmHost {
    SDL_AudioDeviceID dev;
    int sample_rate;
    R01NesSynth synth;
    int playing;
    int step;
    int samples_left;
    int samples_per_step;
    int track_steps;
    char cell[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    R01SfxVoice sfx;
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

static int16_t sfx_sample(R01BgmHost *a) {
    R01SfxVoice *s = &a->sfx;
    int16_t amp = 0;
    float t;
    if (!s->active || s->samples_left <= 0) {
        s->active = 0;
        return 0;
    }
    s->samples_left--;
    if (s->kind == R01_SFX_Y) {
        /* Y: short noise tick */
        s->phase += 1.0;
        if (s->phase >= 1.0) {
            uint16_t l = s->noise_lfsr ? s->noise_lfsr : 1u;
            uint16_t bit = (uint16_t)(((l >> 0) ^ (l >> 1)) & 1u);
            s->noise_lfsr = (uint16_t)((l >> 1) | (bit << 14));
            s->phase = 0.0;
        }
        amp = (s->noise_lfsr & 1u) ? (int16_t)R01_SFX_AMP : (int16_t)(-R01_SFX_AMP);
    } else {
        /* X: fixed pulse blip */
        double step = (double)s->freq_hz / (double)a->sample_rate;
        s->phase += step;
        if (s->phase >= 1.0) {
            s->phase -= 1.0;
        }
        t = (s->phase < 0.5) ? 1.f : -1.f;
        amp = (int16_t)(t * (float)R01_SFX_AMP);
    }
    /* Fade out */
    {
        float fade = (float)s->samples_left / (float)(a->sample_rate / 10 + 1);
        if (fade > 1.f) {
            fade = 1.f;
        }
        amp = (int16_t)((float)amp * fade);
    }
    if (s->samples_left <= 0) {
        s->active = 0;
    }
    return amp;
}

static void SDLCALL bgm_audio_cb(void *userdata, Uint8 *stream, int len) {
    R01BgmHost *a = (R01BgmHost *)userdata;
    int16_t *out = (int16_t *)stream;
    int frames = len / (int)sizeof(int16_t);
    int i = 0;
    if (!a) {
        memset(stream, 0, (size_t)len);
        return;
    }
    memset(stream, 0, (size_t)len);
    if (a->playing) {
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
    for (i = 0; i < frames; i++) {
        int32_t mix = (int32_t)(((float)out[i] + (float)sfx_sample(a)) * R01_HOST_MIX_GAIN);
        if (mix > 32767) {
            mix = 32767;
        }
        if (mix < -32768) {
            mix = -32768;
        }
        out[i] = (int16_t)mix;
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
    g_bgm.sample_rate = have.freq > 0 ? have.freq : R01_BGM_AUDIO_RATE;
    r01_nes_synth_init(&g_bgm.synth, g_bgm.sample_rate);
    g_bgm.samples_per_step = (g_bgm.sample_rate * 60) / (R01_BGM_TEMPO_BPM * R01_BGM_STEPS_PER_BEAT);
    if (g_bgm.samples_per_step < 256) {
        g_bgm.samples_per_step = 256;
    }
    g_bgm.sfx.noise_lfsr = 1;
    SDL_PauseAudioDevice(g_bgm.dev, 0); /* keep running for SFX even without BGM */
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
    /* Keep device running so SFX still works. */
}

void r01_bgm_host_sfx_play(int id) {
    if (id != R01_SFX_X && id != R01_SFX_Y) {
        return;
    }
    if (r01_bgm_host_init() != 0) {
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    g_bgm.sfx.active = 1;
    g_bgm.sfx.kind = id;
    g_bgm.sfx.phase = 0.0;
    g_bgm.sfx.noise_lfsr = 1u;
    if (id == R01_SFX_X) {
        /* Fixed pulse blip (~C6). */
        g_bgm.sfx.freq_hz = 1046.5f;
        g_bgm.sfx.samples_left = g_bgm.sample_rate / 14; /* ~70ms */
    } else {
        /* Fixed noise tick. */
        g_bgm.sfx.freq_hz = 0.f;
        g_bgm.sfx.samples_left = g_bgm.sample_rate / 18; /* ~55ms */
    }
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
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
