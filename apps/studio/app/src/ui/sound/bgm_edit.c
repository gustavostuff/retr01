#include "ui/sound/bgm_edit.h"

#include "ui/internal.h"
#include "retr01_studio/paths.h"
#include "r01_custom_logic_scan.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if UI_SOUND_BGM_CH != R01_BGM_CH_COUNT || UI_SOUND_TRACKS_MAX != R01_BGM_TRACKS_MAX || \
    UI_SOUND_REGIONS_MAX != R01_BGM_REGIONS_MAX
#error "Studio BGM sizes must match R01BgmData in types.h"
#endif

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

void ui_bgm_zoom(UiState *ui, int dir) {
    SoundEditorLayout lo;
    int old_vis;
    int center;
    int z;
    if (!ui || dir == 0) {
        return;
    }
    sound_editor_layout(ui, &lo);
    old_vis = lo.visible_ticks;
    center = ui->sound.scroll_x + old_vis / 2;
    z = ui->sound.zoom_h;
    if (z < UI_SOUND_ZOOM_MIN) {
        z = UI_SOUND_ZOOM_MIN;
    }
    if (z > UI_SOUND_ZOOM_MAX) {
        z = UI_SOUND_ZOOM_MAX;
    }
    z += dir;
    if (z < UI_SOUND_ZOOM_MIN) {
        z = UI_SOUND_ZOOM_MIN;
    }
    if (z > UI_SOUND_ZOOM_MAX) {
        z = UI_SOUND_ZOOM_MAX;
    }
    if (z == ui->sound.zoom_h) {
        return;
    }
    ui->sound.zoom_h = z;
    sound_editor_layout(ui, &lo);
    ui->sound.scroll_x = center - lo.visible_ticks / 2;
    ui_bgm_clamp_scroll(ui, lo.visible_ticks);
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

static int cur_track(const UiState *ui) {
    int t;
    t = ui->sound.track_idx;
    if (t < 0 || t >= ui->sound.track_count) {
        t = 0;
    }
    if (t < 0 || t >= UI_SOUND_TRACKS_MAX) {
        t = 0;
    }
    return t;
}

static int gather_sel(const UiState *ui, UiBgmClipItem *out, int cap, int *out_min_ch, int *out_min_start) {
    int track, ch, i, n = 0;
    int min_ch = 0, min_start = 0, init = 0;
    track = cur_track(ui);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int count = ui->sound.region_count[track][ch];
        for (i = 0; i < count; i++) {
            const UiBgmRegion *rg = &ui->sound.region[track][ch][i];
            if (!rg->selected) {
                continue;
            }
            if (out && n < cap) {
                out[n].ch = ch;
                out[n].rg = *rg;
                out[n].rg.selected = 0;
            }
            if (!init || ch < min_ch) {
                min_ch = ch;
            }
            if (!init || rg->start < min_start) {
                min_start = rg->start;
            }
            init = 1;
            n++;
        }
    }
    if (out_min_ch) {
        *out_min_ch = init ? min_ch : 0;
    }
    if (out_min_start) {
        *out_min_start = init ? min_start : 0;
    }
    return n;
}

void ui_bgm_sel_sync(UiState *ui) {
    int track, ch, i;
    int found = 0;
    int first_ch = -1, first_i = -1;
    int primary_ok = 0;
    if (!ui) {
        return;
    }
    track = cur_track(ui);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int count = ui->sound.region_count[track][ch];
        for (i = 0; i < count; i++) {
            if (!ui->sound.region[track][ch][i].selected) {
                continue;
            }
            if (first_ch < 0) {
                first_ch = ch;
                first_i = i;
            }
            if (ch == ui->sound.sel_ch && i == ui->sound.sel_region) {
                primary_ok = 1;
            }
            found++;
        }
    }
    if (found < 1) {
        if (ui->sound.sel_kind != UI_SOUND_SEL_EMPTY) {
            ui->sound.sel_kind = UI_SOUND_SEL_NONE;
        }
        return;
    }
    ui->sound.sel_kind = UI_SOUND_SEL_REGION;
    if (!primary_ok) {
        ui->sound.sel_ch = first_ch;
        ui->sound.sel_region = first_i;
    }
}

void ui_bgm_sel_clear(UiState *ui) {
    int track, ch, i;
    if (!ui) {
        return;
    }
    track = cur_track(ui);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int count = ui->sound.region_count[track][ch];
        for (i = 0; i < count; i++) {
            ui->sound.region[track][ch][i].selected = 0;
        }
    }
}

void ui_bgm_sel_only(UiState *ui, int ch, int idx) {
    if (!ui) {
        return;
    }
    ui_bgm_sel_clear(ui);
    if (ch >= 0 && ch < UI_SOUND_BGM_CH) {
        int track = cur_track(ui);
        if (idx >= 0 && idx < ui->sound.region_count[track][ch]) {
            ui->sound.region[track][ch][idx].selected = 1;
            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
            ui->sound.sel_ch = ch;
            ui->sound.sel_region = idx;
            return;
        }
    }
    ui->sound.sel_kind = UI_SOUND_SEL_NONE;
}

void ui_bgm_sel_toggle(UiState *ui, int ch, int idx) {
    int track;
    UiBgmRegion *rg;
    if (!ui || ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return;
    }
    track = cur_track(ui);
    if (idx < 0 || idx >= ui->sound.region_count[track][ch]) {
        return;
    }
    rg = &ui->sound.region[track][ch][idx];
    rg->selected = rg->selected ? 0 : 1;
    if (rg->selected) {
        ui->sound.sel_ch = ch;
        ui->sound.sel_region = idx;
    }
    ui_bgm_sel_sync(ui);
}

void ui_bgm_sel_all(UiState *ui) {
    int track, ch, i;
    if (!ui) {
        return;
    }
    track = cur_track(ui);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int count = ui->sound.region_count[track][ch];
        for (i = 0; i < count; i++) {
            ui->sound.region[track][ch][i].selected = 1;
        }
    }
    ui->sound.sel_ch = -1;
    ui->sound.sel_region = -1;
    ui_bgm_sel_sync(ui);
}

void ui_bgm_sel_rect(UiState *ui, int ch0, int t0, int ch1, int t1, int add) {
    int track, ch, i;
    int tlo, thi, clo, chi;
    if (!ui) {
        return;
    }
    if (!add) {
        ui_bgm_sel_clear(ui);
    }
    tlo = t0 < t1 ? t0 : t1;
    thi = t0 > t1 ? t0 : t1;
    clo = ch0 < ch1 ? ch0 : ch1;
    chi = ch0 > ch1 ? ch0 : ch1;
    if (clo < 0) {
        clo = 0;
    }
    if (chi >= UI_SOUND_BGM_CH) {
        chi = UI_SOUND_BGM_CH - 1;
    }
    track = cur_track(ui);
    for (ch = clo; ch <= chi; ch++) {
        int count = ui->sound.region_count[track][ch];
        for (i = 0; i < count; i++) {
            UiBgmRegion *rg = &ui->sound.region[track][ch][i];
            int re = rg->start + rg->len;
            if (re <= tlo || rg->start > thi) {
                continue;
            }
            rg->selected = 1;
        }
    }
    ui_bgm_sel_sync(ui);
}

int ui_bgm_sel_count(const UiState *ui) {
    if (!ui) {
        return 0;
    }
    return gather_sel(ui, NULL, 0, NULL, NULL);
}

int ui_bgm_is_sel(const UiState *ui, int ch, int idx) {
    int track;
    if (!ui || ch < 0 || ch >= UI_SOUND_BGM_CH) {
        return 0;
    }
    track = cur_track(ui);
    if (idx < 0 || idx >= ui->sound.region_count[track][ch]) {
        return 0;
    }
    return ui->sound.region[track][ch][idx].selected ? 1 : 0;
}

void ui_bgm_remove_sel(UiState *ui) {
    int track, ch, i;
    if (!ui) {
        return;
    }
    track = cur_track(ui);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        for (i = ui->sound.region_count[track][ch] - 1; i >= 0; i--) {
            if (ui->sound.region[track][ch][i].selected) {
                ui_bgm_remove_region(ui, track, ch, i);
            }
        }
    }
    ui->sound.sel_kind = UI_SOUND_SEL_NONE;
    ui->sound.sel_region = -1;
}

void ui_bgm_nudge_sel(UiState *ui, int dir, int half_step) {
    int track, ch, i;
    if (!ui || dir == 0) {
        return;
    }
    track = cur_track(ui);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int count = ui->sound.region_count[track][ch];
        for (i = 0; i < count; i++) {
            if (ui->sound.region[track][ch][i].selected) {
                ui_bgm_nudge_region(&ui->sound.region[track][ch][i], ch, dir, half_step);
            }
        }
    }
}

static UiBgmClipItem s_grab[UI_SOUND_CLIP_MAX];
static int s_grab_n;
static int s_grab_pri;

void ui_bgm_move_sel_grab(UiState *ui) {
    int i;
    s_grab_n = 0;
    s_grab_pri = 0;
    if (!ui) {
        return;
    }
    s_grab_n = gather_sel(ui, s_grab, UI_SOUND_CLIP_MAX, NULL, NULL);
    if (s_grab_n > UI_SOUND_CLIP_MAX) {
        s_grab_n = UI_SOUND_CLIP_MAX;
    }
    for (i = 0; i < s_grab_n; i++) {
        if (s_grab[i].ch == ui->sound.sel_ch) {
            int track = cur_track(ui);
            int idx = ui->sound.sel_region;
            if (idx >= 0 && idx < ui->sound.region_count[track][s_grab[i].ch] &&
                ui->sound.region[track][s_grab[i].ch][idx].start == s_grab[i].rg.start) {
                s_grab_pri = i;
                break;
            }
        }
    }
}

void ui_bgm_move_sel_apply(UiState *ui, int dt) {
    int track, i;
    int min_s = 0, max_e = 0, init = 0;
    if (!ui || s_grab_n < 1) {
        return;
    }
    for (i = 0; i < s_grab_n; i++) {
        int s = s_grab[i].rg.start + dt;
        int e = s + s_grab[i].rg.len;
        if (!init || s < min_s) {
            min_s = s;
        }
        if (!init || e > max_e) {
            max_e = e;
        }
        init = 1;
    }
    if (init && min_s < 0) {
        dt -= min_s;
    }
    if (init && max_e > UI_SOUND_STEPS_MAX) {
        dt -= (max_e - UI_SOUND_STEPS_MAX);
    }
    track = cur_track(ui);
    ui_bgm_remove_sel(ui);
    for (i = 0; i < s_grab_n; i++) {
        UiBgmRegion rg = s_grab[i].rg;
        int idx;
        rg.start += dt;
        rg.selected = 1;
        idx = ui_bgm_place_region(ui, track, s_grab[i].ch, &rg);
        if (i == s_grab_pri && idx >= 0) {
            ui->sound.sel_ch = s_grab[i].ch;
            ui->sound.sel_region = idx;
            ui->sound.sel_kind = UI_SOUND_SEL_REGION;
        }
    }
    ui_bgm_sel_sync(ui);
}

void ui_bgm_copy_sel(UiState *ui) {
    UiSoundEdit *s;
    int min_ch, min_start, i, n;
    if (!ui) {
        return;
    }
    s = &ui->sound;
    s->clip_valid = 0;
    s->clip_count = 0;
    n = gather_sel(ui, s->clip, UI_SOUND_CLIP_MAX, &min_ch, &min_start);
    if (n < 1) {
        return;
    }
    if (n > UI_SOUND_CLIP_MAX) {
        n = UI_SOUND_CLIP_MAX;
    }
    for (i = 0; i < n; i++) {
        s->clip[i].rg.start -= min_start;
    }
    s->clip_count = n;
    s->clip_ch = min_ch;
    s->clip_valid = 1;
}

void ui_bgm_paste_sel(UiState *ui) {
    UiSoundEdit *s;
    int track;
    int pivot_t, pivot_ch, dch, i;
    if (!ui) {
        return;
    }
    s = &ui->sound;
    if (!s->clip_valid || s->clip_count < 1) {
        return;
    }
    track = cur_track(ui);
    if (s->sel_kind == UI_SOUND_SEL_EMPTY) {
        pivot_t = s->sel_tick;
        pivot_ch = s->sel_ch;
    } else if (s->sel_kind == UI_SOUND_SEL_REGION && s->sel_ch >= 0 && s->sel_ch < UI_SOUND_BGM_CH &&
               s->sel_region >= 0 && s->sel_region < s->region_count[track][s->sel_ch]) {
        pivot_t = s->region[track][s->sel_ch][s->sel_region].start;
        pivot_ch = s->sel_ch;
    } else {
        return;
    }
    dch = pivot_ch - s->clip_ch;
    ui_bgm_sel_clear(ui);
    s->sel_kind = UI_SOUND_SEL_NONE;
    for (i = 0; i < s->clip_count; i++) {
        UiBgmRegion rg = s->clip[i].rg;
        int dest_ch = s->clip[i].ch + dch;
        int idx;
        if (dest_ch < 0 || dest_ch >= UI_SOUND_BGM_CH) {
            continue;
        }
        rg.start = pivot_t + rg.start;
        rg.selected = 1;
        idx = ui_bgm_place_region(ui, track, dest_ch, &rg);
        if (idx >= 0 && s->sel_kind != UI_SOUND_SEL_REGION) {
            s->sel_ch = dest_ch;
            s->sel_region = idx;
            s->sel_kind = UI_SOUND_SEL_REGION;
        }
    }
    ui_bgm_sel_sync(ui);
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

int ui_bgm_flatten_sel(const UiState *ui, char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN],
                       int *out_origin) {
    UiBgmClipItem items[UI_SOUND_CLIP_MAX];
    int n, i, t, k, steps, min_start, max_end;
    if (!ui || !cells) {
        return 0;
    }
    n = gather_sel(ui, items, UI_SOUND_CLIP_MAX, NULL, &min_start);
    if (n < 1) {
        return 0;
    }
    if (n > UI_SOUND_CLIP_MAX) {
        n = UI_SOUND_CLIP_MAX;
    }
    max_end = min_start;
    for (i = 0; i < n; i++) {
        int e = items[i].rg.start + items[i].rg.len;
        if (e > max_end) {
            max_end = e;
        }
    }
    steps = max_end - min_start;
    if (steps < 1) {
        steps = 1;
    }
    if (steps > R01_BGM_STEPS) {
        steps = R01_BGM_STEPS;
    }
    for (t = 0; t < R01_BGM_STEPS; t++) {
        int c;
        for (c = 0; c < R01_BGM_CH; c++) {
            snprintf(cells[t][c], R01_BGM_TOKEN, "--");
        }
    }
    for (i = 0; i < n; i++) {
        int ch = items[i].ch;
        int off = items[i].rg.start - min_start;
        int len = items[i].rg.len;
        if (ch < 0 || ch >= R01_BGM_CH) {
            continue;
        }
        if (len < 1) {
            len = 1;
        }
        for (k = 0; k < len; k++) {
            int at = off + k;
            if (at < 0 || at >= R01_BGM_STEPS) {
                continue;
            }
            snprintf(cells[at][ch], R01_BGM_TOKEN, "%s", items[i].rg.tok[0] ? items[i].rg.tok : "--");
        }
    }
    if (out_origin) {
        *out_origin = min_start;
    }
    return steps;
}

void ui_bgm_write_export_bins(const UiState *ui) {
    char root[R01_PATH_MAX];
    char path[R01_PATH_MAX];
    char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    int t, n, steps;
    if (!ui) {
        return;
    }
    if (r01_path_resolve(R01_OUTPUT_DIR, root, sizeof(root)) != 0) {
        snprintf(root, sizeof(root), "%s", R01_OUTPUT_DIR);
    }
    {
        char data_dir[R01_PATH_MAX];
        if (snprintf(data_dir, sizeof(data_dir), "%s/data", root) < (int)sizeof(data_dir)) {
            mkdir(data_dir, 0755);
        }
    }
    n = ui->sound.track_count;
    if (n < 1) {
        n = 1;
    }
    if (n > UI_SOUND_TRACKS_MAX) {
        n = UI_SOUND_TRACKS_MAX;
    }
    for (t = 0; t < n; t++) {
        FILE *f;
        steps = ui_bgm_flatten(ui, t, cells, 0);
        if (r01_bgm_track_bin_path(root, t + 1, path, sizeof(path)) != 0) {
            continue;
        }
        f = fopen(path, "wb");
        if (!f) {
            continue;
        }
        fwrite(cells, 1, (size_t)steps * R01_BGM_CH * R01_BGM_TOKEN, f);
        fclose(f);
    }
}

void ui_bgm_sync_to_project(UiState *ui) {
    R01BgmData *bgm;
    int t, ch, i;
    if (!ui || !ui->project) {
        return;
    }
    bgm = &ui->project->bgm;
    memset(bgm, 0, sizeof(*bgm));
    bgm->present = 1;
    bgm->track_count = ui->sound.track_count;
    if (bgm->track_count < 1) {
        bgm->track_count = 1;
    }
    if (bgm->track_count > R01_BGM_TRACKS_MAX) {
        bgm->track_count = R01_BGM_TRACKS_MAX;
    }
    for (t = 0; t < bgm->track_count; t++) {
        snprintf(bgm->track_name[t], sizeof(bgm->track_name[t]), "%s",
                 ui->sound.track_name[t][0] ? ui->sound.track_name[t] : "Track");
        for (ch = 0; ch < R01_BGM_CH_COUNT; ch++) {
            int n = ui->sound.region_count[t][ch];
            if (n > R01_BGM_REGIONS_MAX) {
                n = R01_BGM_REGIONS_MAX;
            }
            bgm->region_count[t][ch] = n;
            for (i = 0; i < n; i++) {
                const UiBgmRegion *src = &ui->sound.region[t][ch][i];
                R01BgmRegion *dst = &bgm->region[t][ch][i];
                dst->start = src->start;
                dst->len = src->len;
                dst->midi = src->midi;
                snprintf(dst->tok, sizeof(dst->tok), "%s", src->tok[0] ? src->tok : "--");
            }
        }
    }
}

void ui_bgm_apply_from_project(UiState *ui) {
    const R01BgmData *bgm;
    int t, ch, i;
    if (!ui || !ui->project) {
        return;
    }
    bgm = &ui->project->bgm;
    if (!bgm->present || bgm->track_count < 1) {
        /* No BGM authored: leave empty tracks. */
        ui_sound_init(ui);
        return;
    }
    memset(&ui->sound, 0, sizeof(ui->sound));
    ui->sound.plane = UI_SOUND_PLANE_BGM;
    ui->sound.track_count = bgm->track_count;
    if (ui->sound.track_count > UI_SOUND_TRACKS_MAX) {
        ui->sound.track_count = UI_SOUND_TRACKS_MAX;
    }
    ui->sound.track_idx = 0;
    ui->sound.solo_ch = UI_SOUND_SOLO_ALL;
    ui->sound.scroll_x = 0;
    ui->sound.zoom_h = UI_SOUND_ZOOM_MIN;
    ui->sound.sel_kind = UI_SOUND_SEL_NONE;
    ui->sound.playing = 0;
    ui->sound.paused = 0;
    ui->sound.play_pos = -1.f;
    ui->sound.play_step_tick = -1;
    for (t = 0; t < ui->sound.track_count; t++) {
        snprintf(ui->sound.track_name[t], sizeof(ui->sound.track_name[t]), "%s",
                 bgm->track_name[t][0] ? bgm->track_name[t] : "Track");
        for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
            int n = bgm->region_count[t][ch];
            if (n > UI_SOUND_REGIONS_MAX) {
                n = UI_SOUND_REGIONS_MAX;
            }
            ui->sound.region_count[t][ch] = n;
            for (i = 0; i < n; i++) {
                const R01BgmRegion *src = &bgm->region[t][ch][i];
                UiBgmRegion *dst = &ui->sound.region[t][ch][i];
                dst->start = src->start;
                dst->len = src->len < 1 ? 1 : src->len;
                dst->midi = src->midi;
                snprintf(dst->tok, sizeof(dst->tok), "%s", src->tok[0] ? src->tok : "--");
            }
        }
    }
}
