#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/paths.h"
#include "retr01_studio/project.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "r01_bgm_host.h"
#include "r01_pad_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static void play_destroy_textures(UiPlaySession *pl) {
    if (!pl) {
        return;
    }
    if (pl->fb_tex) {
        SDL_DestroyTexture(pl->fb_tex);
        pl->fb_tex = NULL;
    }
}

static void play_shutdown_machine(UiPlaySession *pl) {
    if (!pl) {
        return;
    }
    if (pl->machine) {
        r01e_machine_shutdown(pl->machine);
        free(pl->machine);
        pl->machine = NULL;
    }
    play_destroy_textures(pl);
}

static void ui_play_start_bgm(UiState *ui, R01eMachine *m) {
    ui_sound_play_stop(ui);
    if (!m) {
        return;
    }
    (void)r01e_machine_apu_tracker_start_cart(m);
    r01_bgm_host_attach_window(m->io.apu);
}

void ui_play_stop(UiState *ui) {
    if (!ui) {
        return;
    }
    r01_pad_host_menu_set_open(0);
    r01_bgm_host_attach_window(NULL);
    r01_bgm_host_stop();
    if (ui->play.machine) {
        r01e_machine_apu_tracker_stop((R01eMachine *)ui->play.machine);
    }
    play_shutdown_machine(&ui->play);
    ui->play.active = 0;
    ui->play.booting = 0;
    ui->play.spin = 0;
    ui->play.err[0] = '\0';
    ui->play.screen_mark = -1;
}

void ui_toggle_play(UiState *ui) {
    R01World *w;
    if (!ui || !ui->project) {
        return;
    }
    if (ui->play.active) {
        ui_play_stop(ui);
        return;
    }
    r01_project_begin_play(ui->project);
    w = r01_project_active_world(ui->project);
    if (!w || w->screen_count < 1) {
        ui_toast(ui, "no screens - create one first", 1);
        return;
    }
    menu_close(ui);
    ui->pal_edit.open = 0;
    ui->tile_edit.open = 0;
    ui->sprite_edit.open = 0;
    ui->metasprite_edit.open = 0;
    ui->entity_edit.open = 0;
    ui_text_blur(&ui->text);
    ui->play.active = 1;
    ui->play.booting = 1;
    ui->play.spin = 0;
    ui->play.last_tick = SDL_GetTicks();
    ui->play.err[0] = '\0';
    ui->play.screen_mark = -1;
}

/* After first boot frame is presented: export cart + init emu. */
void ui_play_boot_finish(UiState *ui, SDL_Renderer *ren) {
    char stem[R01_PATH_MAX];
    char cart[R01_PATH_MAX];
    char err[256];
    R01eMachine *m;

    if (!ui || !ui->play.booting || !ren) {
        return;
    }

    if (r01_export_stem(ui->project_path, (ui->project && ui->project->name[0]) ? ui->project->name : "project",
                        stem, sizeof(stem)) != 0) {
        snprintf(ui->play.err, sizeof(ui->play.err), "export path failed");
        ui_toast(ui, ui->play.err, 1);
        ui_play_stop(ui);
        return;
    }
    ui_bgm_sync_to_project(ui);
    if (r01_export_bundle(ui->project, stem, err, sizeof(err)) != 0) {
        snprintf(ui->play.err, sizeof(ui->play.err), "%s", err);
        ui_toast(ui, ui->play.err, 1);
        ui_play_stop(ui);
        return;
    }
    ui_bgm_write_export_bins(ui);
    if (snprintf(cart, sizeof(cart), "%s.retr01", stem) >= (int)sizeof(cart)) {
        snprintf(ui->play.err, sizeof(ui->play.err), "cart path too long");
        ui_toast(ui, ui->play.err, 1);
        ui_play_stop(ui);
        return;
    }

    m = (R01eMachine *)calloc(1, sizeof(R01eMachine));
    if (!m) {
        ui_toast(ui, "out of memory", 1);
        ui_play_stop(ui);
        return;
    }
    if (r01e_machine_init(m, cart, err, sizeof(err)) != 0) {
        free(m);
        snprintf(ui->play.err, sizeof(ui->play.err), "%s", err[0] ? err : "emu init failed");
        ui_toast(ui, ui->play.err, 1);
        ui_play_stop(ui);
        return;
    }

    ui->play.fb_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                        R01E_VISIBLE_W, R01E_VISIBLE_H);
    if (!ui->play.fb_tex) {
        play_destroy_textures(&ui->play);
        r01e_machine_shutdown(m);
        free(m);
        ui_toast(ui, "SDL texture failed", 1);
        ui_play_stop(ui);
        return;
    }
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(ui->play.fb_tex, SDL_ScaleModeNearest);
#endif

    ui->play.machine = m;
    ui->play.booting = 0;
    ui->play.last_tick = SDL_GetTicks();
    ui_play_start_bgm(ui, m);
}

void ui_play_reset(UiState *ui) {
    if (!ui || !ui->play.machine) {
        return;
    }
    r01e_machine_reset(ui->play.machine);
    ui_play_start_bgm(ui, ui->play.machine);
    ui->play.last_tick = SDL_GetTicks();
}

int ui_play_screen_mark(UiState *ui) {
    const R01eMachine *m;
    int idx;
    int pw;
    int ph;
    const R01World *w;
    if (!ui || !ui->play.active || ui->play.booting || !ui->play.machine) {
        return ui ? ui->play.screen_mark : -1;
    }
    m = ui->play.machine;
    if (!m->play.enabled) {
        return ui->play.screen_mark;
    }
    w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
    if (!w) {
        return ui->play.screen_mark;
    }
    pw = m->play.player_w > 0 ? m->play.player_w : R01E_PLAY_PLAYER_W;
    ph = m->play.player_h > 0 ? m->play.player_h : R01E_PLAY_PLAYER_H;
    idx = r01_world_find_screen_overlapping(w, m->play.player_x, m->play.player_y, pw, ph);
    if (idx >= 0) {
        ui->play.screen_mark = idx;
        return idx;
    }
    return ui->play.screen_mark;
}
