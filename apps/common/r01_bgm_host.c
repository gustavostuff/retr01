#include "r01_bgm_host.h"

#include "r01_apu_mix.h"
#include "r01_apu_tracker.h"
#include "r01_apu_window.h"
#include "r01_bgm_fd.h"
#include "r01_spi_mailbox.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define R01_BGM_AUDIO_RATE 44100
#define R01_BGM_AUDIO_SAMPLES 256
#define R01_HOST_MIX_GAIN 0.5f
#define R01_BGM_BYTECODE_MAX 8192u
#define R01_BGM_SFX_MAX 16u

typedef struct R01BgmHost {
    SDL_AudioDeviceID dev;
    int sample_rate;
    R01ApuMix mix;
    R01ApuTracker tracker;
    uint8_t regs[R01_APU_REGS];
    uint8_t bytecode[R01_BGM_BYTECODE_MAX];
    uint16_t bytecode_len;
    uint8_t sfx_rom[R01_BGM_SFX_MAX];
    uint16_t sfx_len;
    const uint8_t *live_regs;
    int playing;
    int paused;
    int track_steps;
    int frames_per_step;
    int samples_per_nmi;
    int nmi_samples_left;
    int nmi_count;
    char cell[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
} R01BgmHost;

static R01BgmHost g_bgm;

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
    a->track_steps = (int)(sizeof(demo) / sizeof(demo[0]));
}

static int encode_cells(R01BgmHost *a) {
    int n;
    memset(a->regs, 0, sizeof(a->regs));
    r01_apu_tracker_init(&a->tracker);
    n = r01_bgm_fd_encode_cells((const char(*)[R01_BGM_FD_CH][R01_BGM_FD_TOKEN])a->cell, a->track_steps,
                                a->bytecode, R01_BGM_BYTECODE_MAX);
    if (n < 0) {
        a->bytecode_len = 0;
        return -1;
    }
    a->bytecode_len = (uint16_t)n;
    r01_apu_tracker_set_bgm(&a->tracker, a->bytecode, a->bytecode_len);
    (void)r01_apu_tracker_nmi(&a->tracker, a->regs);
    r01_apu_mix_set_regs(&a->mix, a->regs);
    a->nmi_count = 0;
    a->nmi_samples_left = a->samples_per_nmi;
    a->frames_per_step = r01_bgm_fd_frames_per_step();
    if (a->frames_per_step < 1) {
        a->frames_per_step = 1;
    }
    return 0;
}

static void mix_chunk(R01BgmHost *a, int16_t *out, int frames) {
    int i;
    r01_apu_mix_render(&a->mix, out, frames);
    for (i = 0; i < frames; i++) {
        int32_t s = (int32_t)((float)out[i] * R01_HOST_MIX_GAIN);
        if (s > 32767) {
            s = 32767;
        }
        if (s < -32768) {
            s = -32768;
        }
        out[i] = (int16_t)s;
    }
}

static void SDLCALL bgm_audio_cb(void *userdata, Uint8 *stream, int len) {
    R01BgmHost *a = (R01BgmHost *)userdata;
    int16_t *out = (int16_t *)stream;
    int frames = len / (int)sizeof(int16_t);
    int i = 0;
    uint8_t snap[R01_APU_REGS];
    if (!a) {
        memset(stream, 0, (size_t)len);
        return;
    }
    memset(stream, 0, (size_t)len);
    if (a->live_regs) {
        memcpy(snap, a->live_regs, R01_APU_REGS);
        r01_apu_mix_set_regs(&a->mix, snap);
        mix_chunk(a, out, frames);
        return;
    }
    if (!(a->playing && !a->paused) && !a->tracker.sfx.active) {
        return;
    }
    while (i < frames) {
        int n;
        if (a->nmi_samples_left <= 0) {
            (void)r01_apu_tracker_nmi(&a->tracker, a->regs);
            r01_apu_mix_set_regs(&a->mix, a->regs);
            a->nmi_samples_left = a->samples_per_nmi;
            if (a->playing && !a->paused) {
                a->nmi_count++;
            }
        }
        n = a->nmi_samples_left;
        if (n > frames - i) {
            n = frames - i;
        }
        mix_chunk(a, out + i, n);
        a->nmi_samples_left -= n;
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
    want.samples = R01_BGM_AUDIO_SAMPLES;
    want.callback = bgm_audio_cb;
    want.userdata = &g_bgm;
    g_bgm.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!g_bgm.dev) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return -1;
    }
    g_bgm.sample_rate = have.freq > 0 ? have.freq : R01_BGM_AUDIO_RATE;
    fprintf(stderr, "r01_bgm_host: audio %d Hz, %u samples (%.1f ms)\n", g_bgm.sample_rate,
            (unsigned)have.samples, (1000.0 * (double)have.samples) / (double)g_bgm.sample_rate);
    r01_apu_mix_init(&g_bgm.mix, g_bgm.sample_rate);
    g_bgm.samples_per_nmi = g_bgm.sample_rate / R01_BGM_FD_NMI_HZ;
    if (g_bgm.samples_per_nmi < 1) {
        g_bgm.samples_per_nmi = 1;
    }
    g_bgm.frames_per_step = r01_bgm_fd_frames_per_step();
    SDL_PauseAudioDevice(g_bgm.dev, 0);
    return 0;
}

void r01_bgm_host_shutdown(void) {
    r01_bgm_host_attach_window(NULL);
    r01_bgm_host_stop();
    if (g_bgm.dev) {
        SDL_CloseAudioDevice(g_bgm.dev);
        g_bgm.dev = 0;
    }
}

void r01_bgm_host_attach_window(const uint8_t *regs) {
    if (r01_bgm_host_init() != 0) {
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    g_bgm.live_regs = regs;
    if (regs) {
        g_bgm.playing = 0;
        g_bgm.paused = 0;
    }
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
}

static int load_track_file(R01BgmHost *a, const char *path) {
    FILE *f;
    size_t cell_bytes = (size_t)R01_BGM_CH * R01_BGM_TOKEN;
    size_t n;
    long file_sz;
    int steps;
    if (!path || !path[0]) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    file_sz = ftell(f);
    if (file_sz < (long)cell_bytes || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    steps = (int)(file_sz / (long)cell_bytes);
    if (steps > R01_BGM_STEPS) {
        steps = R01_BGM_STEPS;
    }
    if (steps < 1) {
        fclose(f);
        return -1;
    }
    memset(a->cell, 0, sizeof(a->cell));
    {
        int r, c;
        for (r = 0; r < R01_BGM_STEPS; r++) {
            for (c = 0; c < R01_BGM_CH; c++) {
                snprintf(a->cell[r][c], R01_BGM_TOKEN, "--");
            }
        }
    }
    n = fread(a->cell, 1, (size_t)steps * cell_bytes, f);
    fclose(f);
    if (n != (size_t)steps * cell_bytes) {
        return -1;
    }
    a->track_steps = steps;
    return 0;
}

static int begin_internal_play(R01BgmHost *a) {
    a->live_regs = NULL;
    if (encode_cells(a) != 0) {
        return -1;
    }
    a->paused = 0;
    a->playing = 1;
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
        /* loaded */
    } else if (track == 1) {
        load_builtin_track1(&g_bgm);
    } else {
        SDL_UnlockAudioDevice(g_bgm.dev);
        return -1;
    }
    if (begin_internal_play(&g_bgm) != 0) {
        SDL_UnlockAudioDevice(g_bgm.dev);
        return -1;
    }
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
    return 0;
}

void r01_bgm_host_play_cells(char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN], int steps) {
    if (r01_bgm_host_init() != 0 || !cells) {
        return;
    }
    if (steps < 1) {
        steps = 1;
    }
    if (steps > R01_BGM_STEPS) {
        steps = R01_BGM_STEPS;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    memcpy(g_bgm.cell, cells, sizeof(g_bgm.cell));
    g_bgm.track_steps = steps;
    (void)begin_internal_play(&g_bgm);
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
}

void r01_bgm_host_stop(void) {
    if (!g_bgm.dev) {
        g_bgm.playing = 0;
        g_bgm.paused = 0;
        g_bgm.nmi_count = 0;
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    g_bgm.playing = 0;
    g_bgm.paused = 0;
    g_bgm.nmi_count = 0;
    g_bgm.nmi_samples_left = 0;
    r01_apu_tracker_init(&g_bgm.tracker);
    memset(g_bgm.regs, 0, sizeof(g_bgm.regs));
    r01_apu_mix_set_regs(&g_bgm.mix, g_bgm.regs);
    SDL_UnlockAudioDevice(g_bgm.dev);
}

void r01_bgm_host_pause(void) {
    if (!g_bgm.dev) {
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    if (g_bgm.playing) {
        g_bgm.paused = 1;
    }
    SDL_UnlockAudioDevice(g_bgm.dev);
}

void r01_bgm_host_resume(void) {
    if (!g_bgm.dev) {
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    if (g_bgm.playing && g_bgm.paused) {
        g_bgm.paused = 0;
    }
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
}

void r01_bgm_host_sfx_play(int id) {
    int n;
    if (id != R01_SFX_X && id != R01_SFX_Y) {
        return;
    }
    if (r01_bgm_host_init() != 0) {
        return;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    if (g_bgm.live_regs) {
        SDL_UnlockAudioDevice(g_bgm.dev);
        return;
    }
    n = r01_apu_sfx_encode((uint8_t)id, g_bgm.sfx_rom, R01_BGM_SFX_MAX);
    if (n > 0) {
        g_bgm.sfx_len = (uint16_t)n;
        r01_apu_tracker_trigger_sfx(&g_bgm.tracker, g_bgm.sfx_rom, g_bgm.sfx_len);
    }
    SDL_UnlockAudioDevice(g_bgm.dev);
    SDL_PauseAudioDevice(g_bgm.dev, 0);
}

int r01_bgm_host_playing(void) {
    return g_bgm.playing && !g_bgm.paused;
}

int r01_bgm_host_paused(void) {
    return g_bgm.playing && g_bgm.paused;
}

int r01_bgm_host_step(void) {
    float pos = r01_bgm_host_position();
    if (pos < 0.f) {
        return -1;
    }
    return (int)pos;
}

float r01_bgm_host_position(void) {
    int nmi;
    int left;
    int per;
    int fps;
    int steps;
    float raw;
    if (!g_bgm.dev || !g_bgm.playing) {
        return -1.f;
    }
    SDL_LockAudioDevice(g_bgm.dev);
    nmi = g_bgm.nmi_count;
    left = g_bgm.nmi_samples_left;
    per = g_bgm.samples_per_nmi;
    fps = g_bgm.frames_per_step;
    steps = g_bgm.track_steps;
    SDL_UnlockAudioDevice(g_bgm.dev);
    if (fps < 1) {
        fps = 1;
    }
    if (per < 1) {
        per = 1;
    }
    raw = ((float)nmi + (1.f - (float)left / (float)per)) / (float)fps;
    if (raw < 0.f) {
        raw = 0.f;
    }
    if (steps > 0) {
        while (raw >= (float)steps) {
            raw -= (float)steps;
        }
    }
    return raw;
}

int r01_bgm_host_track_steps(void) {
    return g_bgm.track_steps > 0 ? g_bgm.track_steps : 0;
}
