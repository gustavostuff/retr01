#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_Cursor *g_cursor_arrow;
SDL_Cursor *g_cursor_hand;

int ui_init(UiState *ui) {
    if (!ui) {
        return -1;
    }
    memset(ui, 0, sizeof(*ui));
    if (font_init() != 0) {
        return -1;
    }
    g_cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    g_cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    {
        static const char *const radio_paths[] = {
            R01_STUDIO_ASSETS_DIR "/radio_button.png",
            "app/assets/png/radio_button.png",
            "assets/png/radio_button.png",
            NULL,
        };
        static const char *const dot_paths[] = {
            R01_STUDIO_ASSETS_DIR "/ui_dot.png",
            "app/assets/png/ui_dot.png",
            "assets/png/ui_dot.png",
            NULL,
        };
        static const char *const cross_paths[] = {
            R01_STUDIO_ASSETS_DIR "/ui_cross.png",
            "app/assets/png/ui_cross.png",
            "assets/png/ui_cross.png",
            NULL,
        };
        static const char *const checkbox_paths[] = {
            R01_STUDIO_ASSETS_DIR "/checkbox.png",
            "app/assets/png/checkbox.png",
            "assets/png/checkbox.png",
            NULL,
        };
        static const char *const bg0_paths[] = {
            R01_STUDIO_ASSETS_DIR "/bg0_button.png",
            "app/assets/png/bg0_button.png",
            "assets/png/bg0_button.png",
            NULL,
        };
        static const char *const bg1_paths[] = {
            R01_STUDIO_ASSETS_DIR "/bg1_button.png",
            "app/assets/png/bg1_button.png",
            "assets/png/bg1_button.png",
            NULL,
        };
        static const char *const bg_bank_paths[] = {
            R01_STUDIO_ASSETS_DIR "/bg_bank_button.png",
            "app/assets/png/bg_bank_button.png",
            "assets/png/bg_bank_button.png",
            NULL,
        };
        static const char *const spr_bank_paths[] = {
            R01_STUDIO_ASSETS_DIR "/sprite_bank_button.png",
            "app/assets/png/sprite_bank_button.png",
            "assets/png/sprite_bank_button.png",
            NULL,
        };
        int i;
        for (i = 0; radio_paths[i]; i++) {
            if (ui_load_png_rgba(radio_paths[i], &g_radio_rgba, &g_radio_w, &g_radio_h) == 0) {
                break;
            }
        }
        for (i = 0; dot_paths[i]; i++) {
            if (ui_load_png_rgba(dot_paths[i], &g_dot_rgba, &g_dot_w, &g_dot_h) == 0) {
                break;
            }
        }
        for (i = 0; cross_paths[i]; i++) {
            if (ui_load_png_rgba(cross_paths[i], &g_cross_rgba, &g_cross_w, &g_cross_h) == 0) {
                break;
            }
        }
        for (i = 0; checkbox_paths[i]; i++) {
            if (ui_load_png_rgba(checkbox_paths[i], &g_checkbox_rgba, &g_checkbox_w, &g_checkbox_h) == 0) {
                break;
            }
        }
        for (i = 0; bg0_paths[i]; i++) {
            if (ui_load_png_rgba(bg0_paths[i], &g_bg0_btn_rgba, &g_bg0_btn_w, &g_bg0_btn_h) == 0) {
                break;
            }
        }
        for (i = 0; bg1_paths[i]; i++) {
            if (ui_load_png_rgba(bg1_paths[i], &g_bg1_btn_rgba, &g_bg1_btn_w, &g_bg1_btn_h) == 0) {
                break;
            }
        }
        for (i = 0; bg_bank_paths[i]; i++) {
            if (ui_load_png_rgba(bg_bank_paths[i], &g_bg_bank_btn_rgba, &g_bg_bank_btn_w, &g_bg_bank_btn_h) ==
                0) {
                break;
            }
        }
        for (i = 0; spr_bank_paths[i]; i++) {
            if (ui_load_png_rgba(spr_bank_paths[i], &g_spr_bank_btn_rgba, &g_spr_bank_btn_w,
                                 &g_spr_bank_btn_h) == 0) {
                break;
            }
        }
    }
    ui->project = (R01Project *)calloc(1, sizeof(R01Project));
    if (!ui->project) {
        return -1;
    }
    r01_project_init(ui->project, "untitled");
    ui->project_path[0] = '\0';
    ui->last_click_col = -1;
    ui->last_click_row = -1;
    ui->world_sel_col = -1;
    ui->world_sel_row = -1;
    ui->screen_clip_valid = 0;
    ui->sel_x0 = -1;
    ui->sel_y0 = -1;
    ui->sel_x1 = -1;
    ui->sel_y1 = -1;
    ui->sel_drag = 0;
    ui->inst_drag = 0;
    ui->last_paint_tx = -1;
    ui->last_paint_ty = -1;
    ui->screen_mode = UI_SCREEN_MODE_SEL;
    ui->screen_layer = UI_SCREEN_LAYER_BG;
    ui->banks_idx = 0;
    ui->banks_plane = UI_BANKS_PLANE_SPR;
    ui->accordion_open = UI_ACC_WORLDS;
    accordion_init_heights(ui);
    ui->logic_scale = 1;
    ui->menu.world_screen_idx = -1;
    ui->sel_instance = -1;
    ui->app_mode = UI_APP_GRAPHICS;
    ui_sound_init(ui);
    if (ui_sound_audio_init() != 0) {
        /* Non-fatal: Sounds Play will toast if pressed. */
    }
    return 0;
}

void ui_shutdown(UiState *ui) {
    if (!ui) {
        return;
    }
    ui_sound_play_stop(ui);
    ui_sound_audio_shutdown();
    ui_play_stop(ui);
    free(ui->project);
    ui->project = NULL;
    free(g_radio_rgba);
    g_radio_rgba = NULL;
    g_radio_w = 0;
    g_radio_h = 0;
    free(g_dot_rgba);
    g_dot_rgba = NULL;
    g_dot_w = 0;
    g_dot_h = 0;
    free(g_cross_rgba);
    g_cross_rgba = NULL;
    g_cross_w = 0;
    g_cross_h = 0;
    free(g_checkbox_rgba);
    g_checkbox_rgba = NULL;
    g_checkbox_w = 0;
    g_checkbox_h = 0;
    free(g_bg0_btn_rgba);
    g_bg0_btn_rgba = NULL;
    g_bg0_btn_w = 0;
    g_bg0_btn_h = 0;
    free(g_bg1_btn_rgba);
    g_bg1_btn_rgba = NULL;
    g_bg1_btn_w = 0;
    g_bg1_btn_h = 0;
    free(g_bg_bank_btn_rgba);
    g_bg_bank_btn_rgba = NULL;
    g_bg_bank_btn_w = 0;
    g_bg_bank_btn_h = 0;
    free(g_spr_bank_btn_rgba);
    g_spr_bank_btn_rgba = NULL;
    g_spr_bank_btn_w = 0;
    g_spr_bank_btn_h = 0;
    if (g_cursor_arrow) {
        SDL_FreeCursor(g_cursor_arrow);
        g_cursor_arrow = NULL;
    }
    if (g_cursor_hand) {
        SDL_FreeCursor(g_cursor_hand);
        g_cursor_hand = NULL;
    }
    font_shutdown();
}

void ui_tick(UiState *ui) {
    uint8_t pad = 0;
    if (!ui) {
        return;
    }
    accordion_anim_tick(ui);
    if (!ui->play.active || ui->play.booting || !ui->play.machine) {
        if (ui->play.booting) {
            ui->play.spin++;
        }
        return;
    }
    {
        Uint32 now = SDL_GetTicks();
        if (now - ui->play.last_tick < 16u) {
            return;
        }
        ui->play.last_tick = now;
    }
    if (ui->keys[SDL_SCANCODE_RIGHT] || ui->keys[SDL_SCANCODE_D]) {
        pad |= R01E_PAD_RIGHT;
    }
    if (ui->keys[SDL_SCANCODE_LEFT] || ui->keys[SDL_SCANCODE_A]) {
        pad |= R01E_PAD_LEFT;
    }
    if (ui->keys[SDL_SCANCODE_DOWN] || ui->keys[SDL_SCANCODE_S]) {
        pad |= R01E_PAD_DOWN;
    }
    if (ui->keys[SDL_SCANCODE_UP] || ui->keys[SDL_SCANCODE_W]) {
        pad |= R01E_PAD_UP;
    }
    r01e_machine_set_pad(ui->play.machine, 0, pad);
    (void)r01e_machine_frame(ui->play.machine);
}

void ui_toggle_logic_scale(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->logic_scale = (ui->logic_scale == 1) ? 2 : 1;
    ui_toast(ui, ui->logic_scale == 2 ? "1280x720" : "640x360", 0);
}
