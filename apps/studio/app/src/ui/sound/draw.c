#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"
#include "font/font.h"

#include <stdio.h>
#include <string.h>

static const char *const k_ch_label[UI_SOUND_BGM_CH] = {"Pulse1", "Pulse2", "Tri", "Noise", "DPCM"};
static const char *const k_ch_role[UI_SOUND_BGM_CH] = {
    "Pulse lead", "Pulse harmony", "Triangle bass", "Noise", "DPCM",
};

/* Distinct strip colors per channel (R,G,B). */
static const Uint8 k_ch_col[UI_SOUND_BGM_CH][3] = {
    {70, 140, 220},  /* Pulse1 — blue */
    {90, 190, 120},  /* Pulse2 — green */
    {220, 160, 70},  /* Tri — amber */
    {180, 100, 200}, /* Noise — purple */
    {200, 90, 90},   /* DPCM — red */
};

void draw_sound_editor(UiState *ui, SDL_Renderer *r) {
    SoundEditorLayout lo;
    UiTabsLayout plane;
    int i, ch;
    int lx, ly;
    int tid;
    const char *tname;
    int vis0, vis1;
    UiClipStack clip;

    if (!ui || !r) {
        return;
    }
    ui_sound_play_poll(ui);
    sound_editor_layout(ui, &lo);
    ui_bgm_clamp_scroll(ui, lo.visible_ticks);
    lx = ui->mouse_x;
    ly = ui->mouse_y;

    fill_rect(r, 0, lo.content_y, UI_SIDEBAR_W, ui_logic_h(ui) - lo.content_y, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);
    fill_rect(r, ui_ctrl_x(ui), lo.content_y, UI_CTRL_SIDEBAR_W, ui_logic_h(ui) - lo.content_y, UI_COL_PANEL_R,
              UI_COL_PANEL_G, UI_COL_PANEL_B);

    sound_plane_tabs_prepare(ui, &plane);
    ui_tabs_draw(r, &plane, ui->sound.plane, lx, ly);

    if (ui->sound.plane == UI_SOUND_PLANE_SFX) {
        font_draw_centered(r, UI_SIDEBAR_W, lo.timeline_y, ui_main_w(ui), UI_BTN_H * 2, "SFX editor - coming soon",
                           160, 160, 170);
        return;
    }

    for (i = 0; i < ui->sound.track_count && i < UI_SOUND_TRACKS_MAX; i++) {
        int y = lo.track_list_y + i * lo.track_row_h;
        int sel = (i == ui->sound.track_idx);
        int hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, lo.track_row_h);
        if (sel) {
            fill_rect(r, 0, y, UI_SIDEBAR_W, lo.track_row_h, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else if (hover) {
            fill_rect(r, 0, y, UI_SIDEBAR_W, lo.track_row_h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        font_draw(r, UI_UNIT, y + (lo.track_row_h - 8) / 2, ui->sound.track_name[i], 230, 230, 230);
    }
    draw_button(r, lo.add_x, lo.add_y, lo.add_w, "Add", 1, sound_add_hit(ui, lx, ly));

    tid = ui->sound.track_idx;
    if (tid < 0 || tid >= ui->sound.track_count) {
        tid = 0;
    }
    tname = ui->sound.track_name[tid];
    font_draw(r, lo.timeline_x, lo.hdr_y, tname && tname[0] ? tname : "Track", 240, 240, 240);

    vis0 = ui->sound.scroll_x;
    vis1 = vis0 + lo.visible_ticks;

    /* Ruler */
    fill_rect(r, lo.timeline_x, lo.timeline_y - lo.ruler_h, lo.timeline_w, lo.ruler_h, UI_COL_WELL_R, UI_COL_WELL_G,
              UI_COL_WELL_B);
    for (i = vis0; i <= vis1; i++) {
        int x = lo.timeline_x + (i - vis0) * lo.px_per_tick;
        if (x < lo.timeline_x || x >= lo.timeline_x + lo.timeline_w) {
            continue;
        }
        if ((i % 4) == 0) {
            char lab[16];
            snprintf(lab, sizeof(lab), "%d", i);
            font_draw(r, x + 2, lo.timeline_y - lo.ruler_h + (lo.ruler_h - 8) / 2, lab, 160, 160, 170);
            fill_rect(r, x, lo.timeline_y - 4, 1, 4, 120, 120, 130);
        } else {
            fill_rect(r, x, lo.timeline_y - 2, 1, 2, 90, 90, 100);
        }
    }

    /* Lane wells + labels */
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int y = lo.timeline_y + ch * (lo.lane_h + lo.lane_gap);
        fill_rect(r, lo.timeline_x, y, lo.timeline_w, lo.lane_h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        font_draw(r, lo.lane_label_x, y + (lo.lane_h - 8) / 2, k_ch_label[ch], 200, 200, 210);
        /* Beat grid lines (virtualized to visible range) */
        for (i = vis0; i <= vis1; i++) {
            int x = lo.timeline_x + (i - vis0) * lo.px_per_tick;
            if (x < lo.timeline_x || x >= lo.timeline_x + lo.timeline_w) {
                continue;
            }
            if ((i % 4) == 0) {
                fill_rect(r, x, y, 1, lo.lane_h, 50, 50, 58);
            }
        }
    }

    /* Empty selection pivot */
    if (ui->sound.sel_kind == UI_SOUND_SEL_EMPTY && ui->sound.sel_tick >= vis0 && ui->sound.sel_tick < vis1 &&
        ui->sound.sel_ch >= 0 && ui->sound.sel_ch < UI_SOUND_BGM_CH) {
        int x = lo.timeline_x + (ui->sound.sel_tick - vis0) * lo.px_per_tick;
        int y = lo.timeline_y + ui->sound.sel_ch * (lo.lane_h + lo.lane_gap);
        fill_rect_alpha(r, x, y, lo.px_per_tick, lo.lane_h, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B, 90);
        draw_rect(r, x, y, lo.px_per_tick, lo.lane_h, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
    }

    /* Regions (virtualized) */
    ui_clip_push(r, lo.timeline_x, lo.timeline_y, lo.timeline_w, lo.timeline_h, &clip);
    for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
        int n = ui->sound.region_count[tid][ch];
        int y = lo.timeline_y + ch * (lo.lane_h + lo.lane_gap);
        for (i = 0; i < n; i++) {
            const UiBgmRegion *rg = &ui->sound.region[tid][ch][i];
            int x0, x1, w;
            int sel;
            if (rg->start + rg->len <= vis0 || rg->start >= vis1) {
                continue;
            }
            x0 = lo.timeline_x + (rg->start - vis0) * lo.px_per_tick;
            x1 = lo.timeline_x + (rg->start + rg->len - vis0) * lo.px_per_tick;
            w = x1 - x0;
            if (w < 1) {
                w = 1;
            }
            fill_rect(r, x0, y, w, lo.lane_h, k_ch_col[ch][0], k_ch_col[ch][1], k_ch_col[ch][2]);
            sel = (ui->sound.sel_kind == UI_SOUND_SEL_REGION && ui->sound.sel_ch == ch && ui->sound.sel_region == i);
            if (sel) {
                draw_rect(r, x0, y, w, lo.lane_h, 245, 245, 245);
            }
            font_draw_clipped(r, x0 + 2, y + (lo.lane_h - 8) / 2, x0, y, w, lo.lane_h, rg->tok[0] ? rg->tok : "?",
                              20, 20, 24);
        }
    }
    ui_clip_pop(r, &clip);

    /* Playhead */
    if ((ui->sound.playing || ui->sound.paused) && ui->sound.play_pos >= 0.f) {
        float pos = ui->sound.play_pos;
        int tick_i = (int)pos;
        float frac = pos - (float)tick_i;
        int x = lo.timeline_x + (tick_i - vis0) * lo.px_per_tick + (int)(frac * (float)lo.px_per_tick);
        if (x >= lo.timeline_x && x < lo.timeline_x + lo.timeline_w) {
            fill_rect(r, x, lo.timeline_y - lo.ruler_h, 1, lo.timeline_h + lo.ruler_h, 240, 220, 80);
        }
    }

    /* Minimap: full track overview + viewport marker */
    {
        int map_x = lo.timeline_x;
        int map_y = lo.minimap_y;
        int map_w = lo.timeline_w;
        int map_h = lo.minimap_h;
        int span = UI_SOUND_STEPS_MAX;
        int lane_strip_h;
        int used_h;
        int vx, vw;
        if (span < 1) {
            span = 1;
        }
        /* Equal rows per channel; do not stretch the last lane into leftover pixels. */
        lane_strip_h = map_h / UI_SOUND_BGM_CH;
        if (lane_strip_h < 1) {
            lane_strip_h = 1;
        }
        used_h = lane_strip_h * UI_SOUND_BGM_CH;
        if (used_h > map_h) {
            used_h = map_h;
        }
        fill_rect(r, map_x, map_y, map_w, used_h, 40, 40, 48);
        for (ch = 0; ch < UI_SOUND_BGM_CH; ch++) {
            int n = ui->sound.region_count[tid][ch];
            int ly0 = map_y + ch * lane_strip_h;
            int lh = lane_strip_h;
            if (ly0 + lh > map_y + used_h) {
                break;
            }
            for (i = 0; i < n; i++) {
                const UiBgmRegion *rg = &ui->sound.region[tid][ch][i];
                int x0 = map_x + (rg->start * map_w) / span;
                int x1 = map_x + ((rg->start + rg->len) * map_w) / span;
                int w = x1 - x0;
                if (w < 1) {
                    w = 1;
                }
                fill_rect(r, x0, ly0, w, lh, k_ch_col[ch][0], k_ch_col[ch][1], k_ch_col[ch][2]);
            }
        }
        vx = map_x + (ui->sound.scroll_x * map_w) / span;
        vw = (lo.visible_ticks * map_w) / span;
        if (vw < 2) {
            vw = 2;
        }
        if (vx + vw > map_x + map_w) {
            vx = map_x + map_w - vw;
        }
        if (vx < map_x) {
            vx = map_x;
        }
        fill_rect_alpha(r, vx, map_y, vw, used_h, 255, 255, 255, 70);
    }

    draw_button(r, lo.play_x, lo.play_y, lo.play_w, "Play", 1, sound_play_hit(ui, lx, ly));
    draw_button(r, lo.pause_x, lo.pause_y, lo.pause_w, "Pause", 1, sound_pause_hit(ui, lx, ly));
    draw_button(r, lo.stop_x, lo.stop_y, lo.stop_w, "Stop", 1, sound_stop_hit(ui, lx, ly));
    /* Isolate radios: All on ruler row; channel radios centered on each lane. */
    {
        int all_y = lo.timeline_y - lo.ruler_h;
        int all_cy = all_y + (lo.ruler_h - UI_MODE_ROW_H) / 2;
        int hover_all = point_in_rect(lx, ly, lo.insp_x, all_y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, lo.ruler_h);
        ui_radio_draw(r, lo.insp_x, all_cy + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2,
                      ui->sound.solo_ch == UI_SOUND_SOLO_ALL);
        font_draw(r, lo.insp_x + UI_MODE_RADIO + UI_MODE_GAP, all_cy + (UI_MODE_ROW_H - 8) / 2, "All", 230, 230,
                  230);
        if (hover_all) {
            hover_overlay(r, lo.insp_x, all_y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, lo.ruler_h);
        }
        for (i = 0; i < UI_SOUND_BGM_CH; i++) {
            int y = lo.timeline_y + i * (lo.lane_h + lo.lane_gap);
            int cy = y + (lo.lane_h - UI_MODE_ROW_H) / 2;
            int sel = (i == ui->sound.solo_ch);
            int hover = point_in_rect(lx, ly, lo.insp_x, y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, lo.lane_h);
            ui_radio_draw(r, lo.insp_x, cy + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2, sel);
            font_draw(r, lo.insp_x + UI_MODE_RADIO + UI_MODE_GAP, cy + (UI_MODE_ROW_H - 8) / 2, k_ch_label[i], 230,
                      230, 230);
            if (hover) {
                hover_overlay(r, lo.insp_x, y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, lo.lane_h);
            }
        }
        if (ui->sound.solo_ch >= 0 && ui->sound.solo_ch < UI_SOUND_BGM_CH) {
            int c = ui->sound.solo_ch;
            int y = lo.minimap_y + lo.minimap_h + UI_UNIT;
            font_draw(r, lo.insp_x, y, k_ch_role[c], 160, 160, 170);
        }
    }
}
