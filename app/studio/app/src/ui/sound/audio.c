#include "ui/ui.h"
#include "ui/internal.h"

#include "r01_bgm_host.h"

#include <string.h>

int ui_sound_audio_init(void) {
    return r01_bgm_host_init();
}

void ui_sound_audio_shutdown(void) {
    r01_bgm_host_shutdown();
}

void ui_sound_play_stop(UiState *ui) {
    r01_bgm_host_stop();
    if (ui) {
        ui->sound.playing = 0;
        ui->sound.play_row = -1;
    }
}

void ui_sound_play_start(UiState *ui) {
    char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    int t, r, c;
    if (!ui) {
        return;
    }
    if (ui->sound.plane != UI_SOUND_PLANE_BGM) {
        ui_toast(ui, "SFX play coming soon", 0);
        return;
    }
    t = ui->sound.track_idx;
    if (t < 0 || t >= ui->sound.track_count) {
        t = 0;
    }
    memset(cells, 0, sizeof(cells));
    for (r = 0; r < R01_BGM_STEPS && r < UI_SOUND_STEPS; r++) {
        for (c = 0; c < R01_BGM_CH && c < UI_SOUND_BGM_CH; c++) {
            memcpy(cells[r][c], ui->sound.cell[t][r][c], R01_BGM_TOKEN);
        }
    }
    if (r01_bgm_host_init() != 0) {
        ui_toast(ui, "audio device unavailable", 1);
        return;
    }
    r01_bgm_host_play_cells(cells);
    ui->sound.playing = 1;
    ui->sound.play_row = 0;
}

void ui_sound_play_toggle(UiState *ui) {
    if (!ui) {
        return;
    }
    if (ui->sound.playing) {
        ui_sound_play_stop(ui);
    } else {
        ui_sound_play_start(ui);
    }
}

int ui_sound_play_step(void) {
    return r01_bgm_host_step();
}

void ui_sound_play_poll(UiState *ui) {
    int step;
    if (!ui || !ui->sound.playing) {
        return;
    }
    if (!r01_bgm_host_playing()) {
        ui->sound.playing = 0;
        ui->sound.play_row = -1;
        return;
    }
    step = r01_bgm_host_step();
    if (step < 0) {
        return;
    }
    ui->sound.play_row = step;
    if (step < ui->sound.scroll) {
        ui->sound.scroll = step;
    } else if (step >= ui->sound.scroll + UI_SOUND_VISIBLE_ROWS) {
        ui->sound.scroll = step - UI_SOUND_VISIBLE_ROWS + 1;
        if (ui->sound.scroll < 0) {
            ui->sound.scroll = 0;
        }
    }
}
