#include "ui/sound/bgm_edit.h"

#include <stdio.h>
#include <string.h>

static int clampi(int v, int lo, int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

int ui_bgm_content_end(const UiState *ui, int track) {
    int ch, i, end = 0;
    if (!ui || track < 0 || track >= UI_SOUND_TRACKS_MAX) {
        return 0;
    }
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int n = ui->sound.region_count[track][ch];
        for (i = 0; i < n; i++) {
            const UiBgmRegion *rg = &ui->sound.region[track][ch][i];
            int e = rg->start + rg->len;
            if (e > end) {
                end = e;
            }
        }
    }
    return end;
}

int ui_bgm_scroll_max(const UiState *ui, int visible_ticks) {
    int max_scroll;
    (void)ui;
    /* Allow scrolling through the full step range, including empty space. */
    max_scroll = UI_SOUND_STEPS_MAX - visible_ticks;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    return max_scroll;
}

void ui_bgm_clamp_scroll(UiState *ui, int visible_ticks) {
    int max_scroll;
    if (!ui) {
        return;
    }
    max_scroll = ui_bgm_scroll_max(ui, visible_ticks);
    if (ui->sound.scroll_x < 0) {
        ui->sound.scroll_x = 0;
    }
    if (ui->sound.scroll_x > max_scroll) {
        ui->sound.scroll_x = max_scroll;
    }
}

void ui_bgm_midi_to_tok(int midi, char tok[5]) {
    static const char *const names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int pc;
    int oct;
    if (!tok) {
        return;
    }
    midi = clampi(midi, 12, 119); /* C0..B8 */
    pc = midi % 12;
    oct = midi / 12 - 1;
    if (oct < 0) {
        oct = 0;
    }
    if (oct > 9) {
        oct = 9;
    }
    snprintf(tok, 5, "%s%d", names[pc], oct);
}

int ui_bgm_tok_to_midi(const char *tok) {
    float hz;
    int pc = -1;
    int octave = 4;
    int sharp = 0;
    int i = 0;
    char L;
    if (!tok || !tok[0] || (tok[0] == '-' && tok[1] == '-')) {
        return -1;
    }
    L = tok[0];
    if (L >= 'a' && L <= 'z') {
        L = (char)(L - 'a' + 'A');
    }
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
        return -1;
    }
    i = 1;
    if (tok[i] == '#' || tok[i] == 's' || tok[i] == 'S') {
        sharp = 1;
        i++;
    } else if ((tok[i] == 'b' || tok[i] == 'B') && tok[i + 1] >= '0' && tok[i + 1] <= '9') {
        sharp = -1;
        i++;
    }
    if (!tok[i] || tok[i] < '0' || tok[i] > '9') {
        return -1;
    }
    octave = tok[i] - '0';
    (void)hz;
    return (octave + 1) * 12 + pc + sharp;
}

void ui_bgm_default_tok(int ch, char tok[5], int *out_midi) {
    if (ch == 3) {
        snprintf(tok, 5, "8F");
        if (out_midi) {
            *out_midi = 0xF;
        }
    } else if (ch == 4) {
        snprintf(tok, 5, "FD");
        if (out_midi) {
            *out_midi = 0xFD;
        }
    } else {
        ui_bgm_midi_to_tok(60, tok); /* C4 */
        if (out_midi) {
            *out_midi = 60;
        }
    }
}

void ui_bgm_nudge_region(UiBgmRegion *rg, int ch, int dir, int half_step) {
    int step;
    if (!rg || dir == 0) {
        return;
    }
    if (dir > 0) {
        dir = 1;
    } else {
        dir = -1;
    }
    if (ch == 3) {
        int p = rg->midi & 0x0f;
        p = (p + dir + 16) & 0x0f;
        rg->midi = p;
        snprintf(rg->tok, sizeof(rg->tok), "8%X", p);
        return;
    }
    if (ch == 4) {
        if (rg->midi == 0xFD) {
            rg->midi = 0xFE;
            snprintf(rg->tok, sizeof(rg->tok), "FE");
        } else {
            rg->midi = 0xFD;
            snprintf(rg->tok, sizeof(rg->tok), "FD");
        }
        (void)dir;
        return;
    }
    step = half_step ? 1 : 2;
    rg->midi = clampi(rg->midi + dir * step, 12, 119);
    ui_bgm_midi_to_tok(rg->midi, rg->tok);
}

int ui_bgm_find_at(const UiState *ui, int track, int ch, int tick) {
    int i, n;
    if (!ui || track < 0 || track >= UI_SOUND_TRACKS_MAX || ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return -1;
    }
    n = ui->sound.region_count[track][ch];
    for (i = 0; i < n; i++) {
        const UiBgmRegion *rg = &ui->sound.region[track][ch][i];
        if (tick >= rg->start && tick < rg->start + rg->len) {
            return i;
        }
    }
    return -1;
}

static void sort_regions(UiBgmRegion *arr, int n) {
    int i, j;
    for (i = 1; i < n; i++) {
        UiBgmRegion key = arr[i];
        j = i - 1;
        while (j >= 0 && arr[j].start > key.start) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/* Clear [start, start+len) on channel, splitting/trimming overlaps. */
static void clear_span(UiState *ui, int track, int ch, int start, int len) {
    UiBgmRegion tmp[UI_SOUND_REGIONS_MAX];
    int n, i, out = 0;
    int end = start + len;
    if (!ui || len < 1) {
        return;
    }
    n = ui->sound.region_count[track][ch];
    for (i = 0; i < n; i++) {
        UiBgmRegion rg = ui->sound.region[track][ch][i];
        int re = rg.start + rg.len;
        if (re <= start || rg.start >= end) {
            if (out < UI_SOUND_REGIONS_MAX) {
                tmp[out++] = rg;
            }
            continue;
        }
        if (rg.start < start) {
            UiBgmRegion left = rg;
            left.len = start - rg.start;
            if (left.len >= 1 && out < UI_SOUND_REGIONS_MAX) {
                tmp[out++] = left;
            }
        }
        if (re > end) {
            UiBgmRegion right = rg;
            right.start = end;
            right.len = re - end;
            if (right.len >= 1 && out < UI_SOUND_REGIONS_MAX) {
                tmp[out++] = right;
            }
        }
    }
    memcpy(ui->sound.region[track][ch], tmp, (size_t)out * sizeof(UiBgmRegion));
    ui->sound.region_count[track][ch] = out;
}

int ui_bgm_place_region(UiState *ui, int track, int ch, const UiBgmRegion *src) {
    UiBgmRegion rg;
    int n;
    if (!ui || !src || track < 0 || track >= UI_SOUND_TRACKS_MAX || ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return -1;
    }
    rg = *src;
    if (rg.len < 1) {
        rg.len = 1;
    }
    if (rg.start < 0) {
        rg.start = 0;
    }
    if (rg.start + rg.len > UI_SOUND_STEPS_MAX) {
        rg.len = UI_SOUND_STEPS_MAX - rg.start;
        if (rg.len < 1) {
            return -1;
        }
    }
    clear_span(ui, track, ch, rg.start, rg.len);
    n = ui->sound.region_count[track][ch];
    if (n >= UI_SOUND_REGIONS_MAX) {
        return -1;
    }
    ui->sound.region[track][ch][n] = rg;
    ui->sound.region_count[track][ch] = n + 1;
    sort_regions(ui->sound.region[track][ch], n + 1);
    return ui_bgm_find_at(ui, track, ch, rg.start);
}

void ui_bgm_remove_region(UiState *ui, int track, int ch, int idx) {
    int n, i;
    if (!ui || track < 0 || track >= UI_SOUND_TRACKS_MAX || ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return;
    }
    n = ui->sound.region_count[track][ch];
    if (idx < 0 || idx >= n) {
        return;
    }
    for (i = idx; i < n - 1; i++) {
        ui->sound.region[track][ch][i] = ui->sound.region[track][ch][i + 1];
    }
    ui->sound.region_count[track][ch] = n - 1;
}

int ui_bgm_resize_region(UiState *ui, int track, int ch, int idx, int new_start, int new_len) {
    UiBgmRegion rg;
    int n;
    if (!ui || track < 0 || track >= UI_SOUND_TRACKS_MAX || ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return -1;
    }
    n = ui->sound.region_count[track][ch];
    if (idx < 0 || idx >= n) {
        return -1;
    }
    rg = ui->sound.region[track][ch][idx];
    ui_bgm_remove_region(ui, track, ch, idx);
    rg.start = new_start;
    rg.len = new_len;
    return ui_bgm_place_region(ui, track, ch, &rg);
}

void ui_bgm_copy_sel(UiState *ui) {
    UiSoundEdit *s;
    int track;
    if (!ui) {
        return;
    }
    s = &ui->sound;
    track = s->track_idx;
    s->clip_valid = 0;
    s->clip_count = 0;
    if (s->sel_kind != UI_SOUND_SEL_REGION) {
        return;
    }
    if (s->sel_ch < 0 || s->sel_ch >= UI_SOUND_BGM_CH) {
        return;
    }
    if (s->sel_region < 0 || s->sel_region >= s->region_count[track][s->sel_ch]) {
        return;
    }
    s->clip[0] = s->region[track][s->sel_ch][s->sel_region];
    s->clip[0].start = 0; /* normalize */
    s->clip_count = 1;
    s->clip_ch = s->sel_ch;
    s->clip_valid = 1;
}

void ui_bgm_paste_sel(UiState *ui) {
    UiSoundEdit *s;
    int track;
    int pivot;
    int i;
    if (!ui) {
        return;
    }
    s = &ui->sound;
    if (!s->clip_valid || s->clip_count < 1) {
        return;
    }
    track = s->track_idx;
    if (s->sel_kind == UI_SOUND_SEL_EMPTY) {
        pivot = s->sel_tick;
    } else if (s->sel_kind == UI_SOUND_SEL_REGION && s->sel_ch == s->clip_ch &&
               s->sel_region >= 0 && s->sel_region < s->region_count[track][s->sel_ch]) {
        pivot = s->region[track][s->sel_ch][s->sel_region].start;
    } else {
        return; /* need empty pivot or same-channel region */
    }
    if (s->clip_ch < 0 || s->clip_ch >= UI_SOUND_BGM_CH) {
        return;
    }
    for (i = 0; i < s->clip_count; i++) {
        UiBgmRegion rg = s->clip[i];
        rg.start = pivot + rg.start;
        (void)ui_bgm_place_region(ui, track, s->clip_ch, &rg);
    }
    s->sel_kind = UI_SOUND_SEL_REGION;
    s->sel_ch = s->clip_ch;
    s->sel_region = ui_bgm_find_at(ui, track, s->clip_ch, pivot);
}

int ui_bgm_flatten(const UiState *ui, int track,
                   char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN], int honor_solo) {
    int ch, i, t, end, steps;
    if (!ui || !cells || track < 0 || track >= UI_SOUND_TRACKS_MAX) {
        return 1;
    }
    end = ui_bgm_content_end(ui, track);
    steps = end > 0 ? end : 1;
    if (steps > R01_BGM_STEPS) {
        steps = R01_BGM_STEPS;
    }
    for (t = 0; t < R01_BGM_STEPS; t++) {
        for (ch = 0; ch < R01_BGM_CH; ch++) {
            snprintf(cells[t][ch], R01_BGM_TOKEN, "--");
        }
    }
    for (ch = 0; ch < UI_SOUND_BGM_CH && ch < R01_BGM_CH; ch++) {
        int n;
        if (honor_solo && ui->sound.solo_ch >= 0 && ch != ui->sound.solo_ch) {
            continue; /* channel isolation (preview only) */
        }
        n = ui->sound.region_count[track][ch];
        for (i = 0; i < n; i++) {
            const UiBgmRegion *rg = &ui->sound.region[track][ch][i];
            int s0 = rg->start;
            int e0 = rg->start + rg->len;
            if (s0 < 0 || s0 >= R01_BGM_STEPS) {
                continue;
            }
            snprintf(cells[s0][ch], R01_BGM_TOKEN, "%s", rg->tok[0] ? rg->tok : "--");
            if (e0 < R01_BGM_STEPS && e0 > s0) {
                int covered = 0;
                int j;
                for (j = 0; j < n; j++) {
                    if (ui->sound.region[track][ch][j].start == e0) {
                        covered = 1;
                        break;
                    }
                }
                if (!covered) {
                    snprintf(cells[e0][ch], R01_BGM_TOKEN, "--");
                    if (e0 + 1 > steps && e0 + 1 <= R01_BGM_STEPS) {
                        steps = e0 + 1;
                    }
                }
            }
            {
                int k;
                for (k = s0 + 1; k < e0 && k < R01_BGM_STEPS; k++) {
                    snprintf(cells[k][ch], R01_BGM_TOKEN, "%s", rg->tok[0] ? rg->tok : "--");
                }
            }
        }
    }
    if (steps < 1) {
        steps = 1;
    }
    return steps;
}
