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
            "retr01_studio/assets/radio_button.png",
            "assets/radio_button.png",
            NULL,
        };
        static const char *const dot_paths[] = {
            R01_STUDIO_ASSETS_DIR "/ui_dot.png",
            "retr01_studio/assets/ui_dot.png",
            "assets/ui_dot.png",
            NULL,
        };
        static const char *const cross_paths[] = {
            R01_STUDIO_ASSETS_DIR "/ui_cross.png",
            "retr01_studio/assets/ui_cross.png",
            "assets/ui_cross.png",
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
    }
    ui->project = (R01Project *)calloc(1, sizeof(R01Project));
    if (!ui->project) {
        return -1;
    }
    r01_project_init(ui->project, "untitled");
    ui->project_path[0] = '\0';
    ui->last_click_col = -1;
    ui->last_click_row = -1;
    ui->sel_x0 = -1;
    ui->sel_y0 = -1;
    ui->sel_x1 = -1;
    ui->sel_y1 = -1;
    ui->sel_drag = 0;
    ui->last_paint_tx = -1;
    ui->last_paint_ty = -1;
    ui->screen_mode = UI_SCREEN_MODE_SEL;
    ui->accordion_open = UI_ACC_WORLDS;
    ui->menu.world_screen_idx = -1;
    return 0;
}

void ui_shutdown(UiState *ui) {
    if (!ui) {
        return;
    }
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
    int dx = 0, dy = 0;
    if (!ui || !ui->play.active) {
        return;
    }
    {
        Uint32 now = SDL_GetTicks();
        if (now - ui->play_last_tick < 16u) {
            return;
        }
        ui->play_last_tick = now;
    }
    if (ui->keys[SDL_SCANCODE_W] || ui->keys[SDL_SCANCODE_UP]) {
        dy = -1;
    }
    if (ui->keys[SDL_SCANCODE_S] || ui->keys[SDL_SCANCODE_DOWN]) {
        dy = 1;
    }
    if (ui->keys[SDL_SCANCODE_A] || ui->keys[SDL_SCANCODE_LEFT]) {
        dx = -1;
    }
    if (ui->keys[SDL_SCANCODE_D] || ui->keys[SDL_SCANCODE_RIGHT]) {
        dx = 1;
    }
    r01_play_tick(&ui->play, ui->project, dx, dy);
}
