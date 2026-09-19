#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"

#include "r01_bgm_host.h"

#include <stdio.h>
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
        ui->sound.play_sel = 0;
        ui->sound.play_once = 0;
        ui->sound.play_origin = 0;
        ui->sound.play_pos = 0.f;
        ui->sound.play_step_tick = -1;
        ui->sound.play_span_last = 0.f;
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
            if (ui->sound.play_sel) {
                pos += (float)ui->sound.play_origin;
            }
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
    ui->sound.play_sel = 0;
    ui->sound.play_once = 0;
    ui->sound.play_origin = 0;
    ui->sound.play_pos = 0.f;
    ui->sound.play_step_tick = -1;
    ui->sound.play_span_last = 0.f;
}

void ui_sound_play_start_sel(UiState *ui) {
    char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    int steps;
    int origin = 0;
    if (!ui) {
        return;
    }
    if (ui->sound.plane != UI_SOUND_PLANE_BGM) {
        return;
    }
    steps = ui_bgm_flatten_sel(ui, cells, &origin);
    if (steps < 1) {
        return;
    }
    if (r01_bgm_host_init() != 0) {
        ui_toast(ui, "audio device unavailable", 1);
        return;
    }
    r01_bgm_host_play_cells(cells, steps);
    ui->sound.playing = 1;
    ui->sound.paused = 0;
    ui->sound.play_sel = 1;
    ui->sound.play_once = 1;
    ui->sound.play_origin = origin;
    ui->sound.play_pos = (float)origin;
    ui->sound.play_span_last = 0.f;
}

void ui_sound_play_refresh(UiState *ui) {
    if (!ui || !ui->sound.playing || ui->sound.paused) {
        return;
    }
    if (ui->sound.play_sel) {
        ui_sound_play_start_sel(ui);
        return;
    }
    ui_sound_play_start(ui);
}

void ui_sound_play_section(UiState *ui, int dir) {
    char full[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN];
    SoundEditorLayout lo;
    int t, start, len, i, ch;
    int bar = UI_SOUND_SECTION_TICKS;
    int content;
    if (!ui || !UI_SOUND_SECTION_PLAY) {
        return;
    }
    if (ui->sound.plane != UI_SOUND_PLANE_BGM) {
        return;
    }
    if (bar < 1) {
        bar = 1;
    }
    if (dir > 0) {
        dir = 1;
    } else {
        dir = -1;
    }
    t = ui->sound.track_idx;
    if (t < 0 || t >= ui->sound.track_count) {
        t = 0;
    }
    if (ui->sound.playing && ui->sound.play_once) {
        start = ui->sound.play_origin + dir * bar;
    } else if (ui->sound.play_step_tick >= 0) {
        start = ui->sound.play_step_tick + dir * bar;
    } else if ((ui->sound.playing || ui->sound.paused) && ui->sound.play_pos >= 0.f) {
        int cur = (int)ui->sound.play_pos;
        start = (cur / bar) * bar + dir * bar;
    } else if (dir > 0) {
        start = 0;
    } else {
        content = ui_bgm_content_end(ui, t);
        if (content < 1) {
            content = bar;
        }
        start = ((content - 1) / bar) * bar;
    }
    if (start < 0 || start >= UI_SOUND_STEPS_MAX) {
        return;
    }
    len = bar;
    if (start + len > UI_SOUND_STEPS_MAX) {
        len = UI_SOUND_STEPS_MAX - start;
    }
    if (len < 1) {
        return;
    }
    (void)ui_bgm_flatten(ui, t, full, 1);
    for (i = 0; i < R01_BGM_STEPS; i++) {
        for (ch = 0; ch < R01_BGM_CH; ch++) {
            snprintf(cells[i][ch], R01_BGM_TOKEN, "--");
        }
    }
    for (i = 0; i < len; i++) {
        for (ch = 0; ch < R01_BGM_CH; ch++) {
            snprintf(cells[i][ch], R01_BGM_TOKEN, "%s", full[start + i][ch]);
        }
    }
    if (r01_bgm_host_init() != 0) {
        ui_toast(ui, "audio device unavailable", 1);
        return;
    }
    r01_bgm_host_play_cells(cells, len);
    ui->sound.playing = 1;
    ui->sound.paused = 0;
    ui->sound.play_sel = 1;
    ui->sound.play_once = 1;
    ui->sound.play_origin = start;
    ui->sound.play_step_tick = start;
    ui->sound.play_pos = (float)start;
    ui->sound.play_span_last = 0.f;
    sound_editor_layout(ui, &lo);
    ui->sound.scroll_x = start;
    ui_bgm_clamp_scroll(ui, lo.visible_ticks);
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
    float hpos;
    int margin;
    int vis;
    int steps;
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
            if (ui->sound.play_sel) {
                pos += (float)ui->sound.play_origin;
            }
            ui->sound.play_pos = pos;
        }
        return;
    }
    if (!r01_bgm_host_playing()) {
        if (ui->sound.play_once) {
            steps = r01_bgm_host_track_steps();
            if (steps < 1) {
                steps = UI_SOUND_SECTION_TICKS;
            }
            ui->sound.play_pos = (float)(ui->sound.play_origin + steps);
            ui->sound.playing = 0;
            ui->sound.paused = 0;
            ui->sound.play_sel = 0;
            ui->sound.play_once = 0;
            ui->sound.play_span_last = 0.f;
            return;
        }
        ui->sound.playing = 0;
        ui->sound.paused = 0;
        ui->sound.play_sel = 0;
        ui->sound.play_origin = 0;
        ui->sound.play_pos = -1.f;
        return;
    }
    hpos = r01_bgm_host_position();
    if (hpos < 0.f) {
        return;
    }
    if (ui->sound.play_once) {
        steps = r01_bgm_host_track_steps();
        if (steps < 1) {
            steps = UI_SOUND_SECTION_TICKS;
        }
        if (ui->sound.play_span_last > (float)steps * 0.25f && hpos < ui->sound.play_span_last) {
            r01_bgm_host_stop();
            ui->sound.playing = 0;
            ui->sound.paused = 0;
            ui->sound.play_sel = 0;
            ui->sound.play_once = 0;
            ui->sound.play_pos = (float)(ui->sound.play_origin + steps);
            ui->sound.play_span_last = 0.f;
            return;
        }
        ui->sound.play_span_last = hpos;
    }
    pos = hpos;
    if (ui->sound.play_sel) {
        pos += (float)ui->sound.play_origin;
    }
    ui->sound.play_pos = pos;
    if (ui->sound.play_sel) {
        return;
    }
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
