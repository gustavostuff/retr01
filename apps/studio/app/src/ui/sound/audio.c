#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"

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
        ui->sound.paused = 0;
        ui->sound.play_pos = 0.f;
        ui->sound.scroll_x = 0;
    }
}

void ui_sound_play_pause(UiState *ui) {
    if (!ui) {
        return;
    }
    if (!ui->sound.playing && !ui->sound.paused) {
        return;
    }
    r01_bgm_host_pause();
    ui->sound.playing = 0;
    ui->sound.paused = 1;
    {
        float pos = r01_bgm_host_position();
        if (pos >= 0.f) {
            ui->sound.play_pos = pos;
        }
    }
}

void ui_sound_play_start(UiState *ui) {
    char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    int t, steps;
    if (!ui) {
        return;
    }
    if (ui->sound.plane != UI_SOUND_PLANE_BGM) {
        ui_toast(ui, "SFX play coming soon", 0);
        return;
    }
    /* Resume from pause without restarting. */
    if (ui->sound.paused) {
        r01_bgm_host_resume();
        ui->sound.paused = 0;
        ui->sound.playing = 1;
        return;
    }
    t = ui->sound.track_idx;
    if (t < 0 || t >= ui->sound.track_count) {
        t = 0;
    }
    steps = ui_bgm_flatten(ui, t, cells, 1);
    if (r01_bgm_host_init() != 0) {
        ui_toast(ui, "audio device unavailable", 1);
        return;
    }
    r01_bgm_host_play_cells(cells, steps);
    ui->sound.playing = 1;
    ui->sound.paused = 0;
    ui->sound.play_pos = 0.f;
}

void ui_sound_play_toggle(UiState *ui) {
    if (!ui) {
        return;
    }
    if (ui->sound.playing) {
        ui_sound_play_stop(ui);
    } else if (ui->sound.paused) {
        ui_sound_play_start(ui); /* resume */
    } else {
        ui_sound_play_start(ui);
    }
}

int ui_sound_play_step(void) {
    return r01_bgm_host_step();
}

void ui_sound_play_poll(UiState *ui) {
    SoundEditorLayout lo;
    float pos;
    int margin;
    int vis;
    if (!ui) {
        return;
    }
    if (!ui->sound.playing && !ui->sound.paused) {
        return;
    }
    if (ui->sound.paused) {
        /* Keep last play_pos; host still reports position. */
        pos = r01_bgm_host_position();
        if (pos >= 0.f) {
            ui->sound.play_pos = pos;
        }
        return;
    }
    if (!r01_bgm_host_playing()) {
        ui->sound.playing = 0;
        ui->sound.paused = 0;
        ui->sound.play_pos = -1.f;
        return;
    }
    pos = r01_bgm_host_position();
    if (pos < 0.f) {
        return;
    }
    ui->sound.play_pos = pos;
    sound_editor_layout(ui, &lo);
    vis = lo.visible_ticks;
    margin = vis / 5;
    if (margin < 2) {
        margin = 2;
    }
    /* Auto-scroll before playhead reaches right edge. */
    if (pos >= (float)(ui->sound.scroll_x + vis - margin)) {
        ui->sound.scroll_x = (int)pos - (vis - margin);
        ui_bgm_clamp_scroll(ui, vis);
    }
    if (pos < (float)ui->sound.scroll_x) {
        /* Loop wrapped */
        ui->sound.scroll_x = (int)pos;
        if (ui->sound.scroll_x < 0) {
            ui->sound.scroll_x = 0;
        }
    }
}
