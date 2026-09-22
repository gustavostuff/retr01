#include "retr01_studio/bgm_pack.h"

#include "r01_apu_cart.h"
#include "r01_custom_logic_scan.h"

#include <stdio.h>
#include <string.h>

static void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

int r01_bgm_flatten_track(const R01BgmData *bgm, int track,
                          char cells[R01_BGM_FD_STEPS_MAX][R01_BGM_FD_CH][R01_BGM_FD_TOKEN]) {
    int ch, i, t, end, steps;
    int tc;
    if (!bgm || !cells || track < 0 || track >= R01_BGM_TRACKS_MAX) {
        return 1;
    }
    tc = bgm->present ? bgm->track_count : 0;
    if (track >= tc) {
        for (t = 0; t < R01_BGM_FD_STEPS_MAX; t++) {
            for (ch = 0; ch < R01_BGM_FD_CH; ch++) {
                snprintf(cells[t][ch], R01_BGM_FD_TOKEN, "--");
            }
        }
        return 1;
    }
    end = 0;
    for (ch = 0; ch < R01_BGM_CH_COUNT && ch < R01_BGM_FD_CH; ch++) {
        int n = bgm->region_count[track][ch];
        for (i = 0; i < n; i++) {
            const R01BgmRegion *rg = &bgm->region[track][ch][i];
            int e = rg->start + rg->len;
            if (e > end) {
                end = e;
            }
        }
    }
    steps = end > 0 ? end : 1;
    if (steps > R01_BGM_FD_STEPS_MAX) {
        steps = R01_BGM_FD_STEPS_MAX;
    }
    for (t = 0; t < R01_BGM_FD_STEPS_MAX; t++) {
        for (ch = 0; ch < R01_BGM_FD_CH; ch++) {
            snprintf(cells[t][ch], R01_BGM_FD_TOKEN, "--");
        }
    }
    for (ch = 0; ch < R01_BGM_CH_COUNT && ch < R01_BGM_FD_CH; ch++) {
        int n = bgm->region_count[track][ch];
        if (n > R01_BGM_REGIONS_MAX) {
            n = R01_BGM_REGIONS_MAX;
        }
        for (i = 0; i < n; i++) {
            const R01BgmRegion *rg = &bgm->region[track][ch][i];
            int s0 = rg->start;
            int e0 = rg->start + rg->len;
            int k;
            if (s0 < 0 || s0 >= R01_BGM_FD_STEPS_MAX) {
                continue;
            }
            snprintf(cells[s0][ch], R01_BGM_FD_TOKEN, "%s", rg->tok[0] ? rg->tok : "--");
            for (k = s0 + 1; k < e0 && k < R01_BGM_FD_STEPS_MAX; k++) {
                snprintf(cells[k][ch], R01_BGM_FD_TOKEN, "%s", rg->tok[0] ? rg->tok : "--");
            }
        }
    }
    if (steps < 1) {
        steps = 1;
    }
    return steps;
}

void r01_bgm_pack_prg(uint8_t prg[R01_PRG_BYTES], const R01BgmData *bgm, const char *custom_logic_path) {
    uint8_t *blob;
    unsigned o;
    int t;
    int tc;
    int boot = 0;
    uint16_t off[R01_PRG_BGM_TRACKS];
    uint16_t len[R01_PRG_BGM_TRACKS];
    if (!prg) {
        return;
    }
    prg[R01_PRG_BGM_BOOT_OFF] = 0;
    blob = prg + R01_PRG_BGM_OFF;
    memset(blob, 0, (size_t)(R01_PRG_BGM_END - R01_PRG_BGM_OFF));
    blob[0] = R01_PRG_BGM_MAGIC0;
    blob[1] = R01_PRG_BGM_MAGIC1;
    blob[3] = R01_PRG_BGM_INS_VER;
    memset(off, 0, sizeof(off));
    memset(len, 0, sizeof(len));
    tc = (bgm && bgm->present) ? bgm->track_count : 0;
    if (tc < 0) {
        tc = 0;
    }
    if (tc > (int)R01_PRG_BGM_TRACKS) {
        tc = (int)R01_PRG_BGM_TRACKS;
    }
    for (t = 0; t < (int)R01_PRG_BGM_TRACKS; t++) {
        int ch;
        uint8_t *ip = blob + R01_PRG_BGM_HDR + (unsigned)t * R01_PRG_BGM_INS_CH;
        for (ch = 0; ch < (int)R01_PRG_BGM_INS_CH; ch++) {
            int v = 0;
            if (bgm && t < tc && ch < R01_BGM_CH_COUNT) {
                v = bgm->ch_ins[t][ch];
            }
            if (v < 0 || v > (int)R01_PRG_BGM_INS_MAX) {
                v = 0;
            }
            ip[ch] = (uint8_t)v;
        }
    }
    o = R01_PRG_BGM_HDR_V1;
    for (t = 0; t < tc; t++) {
        char cells[R01_BGM_FD_STEPS_MAX][R01_BGM_FD_CH][R01_BGM_FD_TOKEN];
        int steps;
        int n;
        unsigned cap;
        steps = r01_bgm_flatten_track(bgm, t, cells);
        cap = (unsigned)(R01_PRG_BGM_END - R01_PRG_BGM_OFF);
        if (o >= cap) {
            break;
        }
        n = r01_bgm_fd_encode_cells((const char(*)[R01_BGM_FD_CH][R01_BGM_FD_TOKEN])cells, steps, blob + o,
                                    cap - o);
        if (n < 1) {
            continue;
        }
        off[t] = (uint16_t)o;
        len[t] = (uint16_t)n;
        o += (unsigned)n;
    }
    blob[2] = (uint8_t)tc;
    for (t = 0; t < (int)R01_PRG_BGM_TRACKS; t++) {
        put_u16_le(blob + 4 + (unsigned)t * 2u, off[t]);
        put_u16_le(blob + 20 + (unsigned)t * 2u, len[t]);
    }
    if (custom_logic_path && r01_custom_logic_scan_bgm_play(custom_logic_path, &boot) == 0 && boot >= 1 &&
        boot <= tc && len[boot - 1] > 0u) {
        prg[R01_PRG_BGM_BOOT_OFF] = (uint8_t)boot;
    }
}
