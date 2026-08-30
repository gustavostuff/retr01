#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/paths.h"
#include "retr01_studio/project.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void ui_play_stop(UiState *ui) {
    if (!ui) {
        return;
    }
    play_shutdown_machine(&ui->play);
    ui->play.active = 0;
    ui->play.booting = 0;
    ui->play.spin = 0;
    ui->play.err[0] = '\0';
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
    ui->play.active = 1;
    ui->play.booting = 1;
    ui->play.spin = 0;
    ui->play.last_tick = SDL_GetTicks();
    ui->play.err[0] = '\0';
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

    if (r01_path_resolve(R01_DEFAULT_CART_STEM, stem, sizeof(stem)) != 0) {
        snprintf(ui->play.err, sizeof(ui->play.err), "export path failed");
        ui_toast(ui, ui->play.err, 1);
        ui_play_stop(ui);
        return;
    }
    if (r01_export_bundle(ui->project, stem, err, sizeof(err)) != 0) {
        snprintf(ui->play.err, sizeof(ui->play.err), "%s", err);
        ui_toast(ui, ui->play.err, 1);
        ui_play_stop(ui);
        return;
    }
    snprintf(cart, sizeof(cart), "%s.retr01", stem);

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
}

int ui_play_screen_mark(const UiState *ui) {
    const R01eMachine *m;
    int col, row;
    const R01World *w;
    if (!ui || !ui->play.active || ui->play.booting || !ui->play.machine) {
        return -1;
    }
    m = ui->play.machine;
    if (!m->play.enabled) {
        return -1;
    }
    w = r01_project_active_world_const(ui->project);
    if (!w) {
        return -1;
    }
    col = (m->play.player_x + R01E_PLAY_PLAYER_W / 2) / R01E_SCREEN_PX_W;
    row = (m->play.player_y + R01E_PLAY_PLAYER_H / 2) / R01E_SCREEN_PX_H;
    return r01_world_screen_index(w, col, row);
}
