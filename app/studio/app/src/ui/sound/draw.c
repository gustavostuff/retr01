#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include <stdio.h>
#include <string.h>

static const char *const k_ch_label[UI_SOUND_BGM_CH] = {"Pulse1", "Pulse2", "Tri", "Noise", "DPCM"};
static const char *const k_ch_role[UI_SOUND_BGM_CH] = {
    "Pulse lead", "Pulse harmony", "Triangle bass", "Noise", "DPCM",
};

void draw_sound_editor(UiState *ui, SDL_Renderer *r) {
    SoundEditorLayout lo;
    UiTabsLayout plane;
    int i, row, col;
    int lx, ly;
    int tid;
    const char *tname;

    if (!ui || !r) {
        return;
    }
    ui_sound_play_poll(ui);
    sound_editor_layout(ui, &lo);
    lx = ui->mouse_x;
    ly = ui->mouse_y;

    fill_rect(r, 0, lo.content_y, UI_SIDEBAR_W, ui_logic_h(ui) - lo.content_y, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);
    fill_rect(r, ui_ctrl_x(ui), lo.content_y, UI_CTRL_SIDEBAR_W, ui_logic_h(ui) - lo.content_y, UI_COL_PANEL_R,
              UI_COL_PANEL_G, UI_COL_PANEL_B);

    sound_plane_tabs_prepare(ui, &plane);
    ui_tabs_draw(r, &plane, ui->sound.plane, lx, ly);

    if (ui->sound.plane == UI_SOUND_PLANE_SFX) {
        font_draw_centered(r, UI_SIDEBAR_W, lo.grid_y, ui_main_w(ui), UI_BTN_H * 2, "SFX editor — coming soon", 160,
                           160, 170);
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
    font_draw(r, lo.grid_x, lo.hdr_y, tname && tname[0] ? tname : "Track", 240, 240, 240);
    font_draw(r, lo.grid_x + label_width(tname && tname[0] ? tname : "Track") + UI_UNIT * 2, lo.hdr_y,
              "NMI / FE delay", 120, 120, 130);

    for (col = 0; col < UI_SOUND_BGM_CH; col++) {
        int cx = lo.grid_x + col * lo.cell_w;
        fill_rect(r, cx, lo.col_hdr_y, lo.cell_w - 1, UI_BTN_H - 2, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        font_draw_centered(r, cx, lo.col_hdr_y, lo.cell_w - 1, UI_BTN_H - 2, k_ch_label[col], 200, 200, 210);
    }

    fill_rect(r, lo.grid_x, lo.grid_y, lo.grid_w, lo.grid_h, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    for (row = 0; row < UI_SOUND_VISIBLE_ROWS; row++) {
        int step = ui->sound.scroll + row;
        int cy = lo.grid_y + row * lo.row_h;
        char step_lab[16];
        int play_here;
        if (step < 0 || step >= UI_SOUND_STEPS) {
            break;
        }
        play_here = ui->sound.playing && step == ui->sound.play_row;
        snprintf(step_lab, sizeof(step_lab), "%02d", step);
        font_draw(r, lo.grid_x - UI_UNIT * 3, cy + 2, step_lab, play_here ? 180 : 100, play_here ? 220 : 100,
                  play_here ? 140 : 110);
        if (play_here) {
            fill_rect(r, lo.grid_x, cy, lo.grid_w, lo.row_h - 1, 36, 52, 40);
        }
        for (col = 0; col < UI_SOUND_BGM_CH; col++) {
            int cx = lo.grid_x + col * lo.cell_w;
            int sel = (step == ui->sound.sel_row && col == ui->sound.sel_col);
            const char *tok = ui->sound.cell[tid][step][col];
            if (sel) {
                fill_rect(r, cx, cy, lo.cell_w - 1, lo.row_h - 1, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
            }
            font_draw_centered(r, cx, cy, lo.cell_w - 1, lo.row_h - 1, tok && tok[0] ? tok : "--", 230, 230, 230);
        }
    }

    draw_button(r, lo.play_x, lo.play_y, lo.play_w, ui->sound.playing ? "Stop" : "Play", 1,
                sound_play_hit(ui, lx, ly));
    font_draw(r, lo.insp_x, lo.play_y + UI_BTN_H + 2, "Channel", 200, 200, 210);
    for (i = 0; i < UI_SOUND_BGM_CH; i++) {
        int y = lo.ch_radio_y0 + i * (UI_MODE_ROW_H + 2);
        int sel = (i == ui->sound.channel);
        int hover = point_in_rect(lx, ly, lo.insp_x, y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, UI_MODE_ROW_H);
        ui_radio_draw(r, lo.insp_x, y + (UI_MODE_ROW_H - UI_MODE_RADIO) / 2, sel);
        font_draw(r, lo.insp_x + UI_MODE_RADIO + UI_MODE_GAP, y + (UI_MODE_ROW_H - 8) / 2, k_ch_label[i], 230, 230,
                  230);
        if (hover) {
            hover_overlay(r, lo.insp_x, y, UI_CTRL_SIDEBAR_W - UI_UNIT * 2, UI_MODE_ROW_H);
        }
    }
    {
        int ch = ui->sound.channel;
        int y = lo.ch_radio_y0 + UI_SOUND_BGM_CH * (UI_MODE_ROW_H + 2) + UI_UNIT;
        if (ch < 0 || ch >= UI_SOUND_BGM_CH) {
            ch = 0;
        }
        font_draw(r, lo.insp_x, y, k_ch_role[ch], 160, 160, 170);
        y += UI_BTN_H;
        font_draw(r, lo.insp_x, y, "Vol  --", 140, 140, 150);
        y += UI_BTN_H;
        font_draw(r, lo.insp_x, y, "Duty --", 140, 140, 150);
    }
}
