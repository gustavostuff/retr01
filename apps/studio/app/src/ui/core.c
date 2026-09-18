#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/modals/project_io.h"
#include "ui/undo/undo.h"
#include "ui/undo/undo_cmds.h"
#include "font/font.h"
#include "retr01_ui/chrome.h"

#include "retr01_studio/project.h"

#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "r01_pad_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(R01_HAS_XCURSOR)
#include <X11/Xcursor/Xcursor.h>
#endif

SDL_Cursor *g_cursor_arrow;
SDL_Cursor *g_cursor_hand;
SDL_Cursor *g_cursor_sizewe;
SDL_Cursor *g_cursor_sizens;
SDL_Cursor *g_cursor_sizenwse;
SDL_Cursor *g_cursor_sizenesw;
SDL_Cursor *g_cursor_sizese;
SDL_Cursor *g_cursor_sizesw;
SDL_Cursor *g_cursor_sizeall;
SDL_Cursor *g_cursor_no;

#if defined(R01_HAS_XCURSOR)
static int cursor_theme_size(void) {
    const char *e = getenv("XCURSOR_SIZE");
    int v;
    if (!e || !e[0]) {
        return 24;
    }
    v = atoi(e);
    if (v < 16) {
        return 16;
    }
    if (v > 64) {
        return 64;
    }
    return v;
}

/* Load a named theme cursor (se-resize, bottom_right_corner, ...). */
static SDL_Cursor *cursor_from_theme(const char *const *names) {
    int size = cursor_theme_size();
    int i;
    for (i = 0; names && names[i]; i++) {
        XcursorImage *img = XcursorLibraryLoadImage(names[i], NULL, size);
        SDL_Surface *surf;
        SDL_Cursor *cur;
        int hot_x;
        int hot_y;
        int y;
        if (!img || !img->pixels || img->width < 1 || img->height < 1) {
            if (img) {
                XcursorImageDestroy(img);
            }
            continue;
        }
        surf = SDL_CreateRGBSurfaceWithFormat(0, (int)img->width, (int)img->height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surf || !surf->pixels) {
            if (surf) {
                SDL_FreeSurface(surf);
            }
            XcursorImageDestroy(img);
            continue;
        }
        for (y = 0; y < (int)img->height; y++) {
            memcpy((uint8_t *)surf->pixels + y * surf->pitch, (const uint8_t *)img->pixels + y * (int)img->width * 4,
                   (size_t)img->width * 4u);
        }
        hot_x = (int)img->xhot;
        hot_y = (int)img->yhot;
        if (hot_x < 0) {
            hot_x = 0;
        }
        if (hot_y < 0) {
            hot_y = 0;
        }
        if (hot_x >= (int)img->width) {
            hot_x = (int)img->width - 1;
        }
        if (hot_y >= (int)img->height) {
            hot_y = (int)img->height - 1;
        }
        cur = SDL_CreateColorCursor(surf, hot_x, hot_y);
        SDL_FreeSurface(surf);
        XcursorImageDestroy(img);
        if (cur) {
            return cur;
        }
    }
    return NULL;
}
#endif

static SDL_Cursor *cursor_corner(const char *const *names, SDL_SystemCursor fallback) {
    SDL_Cursor *cur = NULL;
#if defined(R01_HAS_XCURSOR)
    cur = cursor_from_theme(names);
#else
    (void)names;
#endif
    if (cur) {
        return cur;
    }
    return SDL_CreateSystemCursor(fallback);
}

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
    g_cursor_sizewe = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    g_cursor_sizens = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    {
        static const char *const nw_names[] = {"nw-resize", "top_left_corner", NULL};
        static const char *const ne_names[] = {"ne-resize", "top_right_corner", NULL};
        static const char *const se_names[] = {"se-resize", "bottom_right_corner", NULL};
        static const char *const sw_names[] = {"sw-resize", "bottom_left_corner", NULL};
        g_cursor_sizenwse = cursor_corner(nw_names, SDL_SYSTEM_CURSOR_SIZENWSE);
        g_cursor_sizenesw = cursor_corner(ne_names, SDL_SYSTEM_CURSOR_SIZENESW);
        g_cursor_sizese = cursor_corner(se_names, SDL_SYSTEM_CURSOR_SIZENWSE);
        g_cursor_sizesw = cursor_corner(sw_names, SDL_SYSTEM_CURSOR_SIZENESW);
    }
    g_cursor_sizeall = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    g_cursor_no = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
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
        {
            R01UiChrome chrome;
            chrome.radio.rgba = g_radio_rgba;
            chrome.radio.w = g_radio_w;
            chrome.radio.h = g_radio_h;
            chrome.checkbox.rgba = g_checkbox_rgba;
            chrome.checkbox.w = g_checkbox_w;
            chrome.checkbox.h = g_checkbox_h;
            chrome.dot.rgba = g_dot_rgba;
            chrome.dot.w = g_dot_w;
            chrome.dot.h = g_dot_h;
            r01_ui_chrome_set(&chrome);
        }
    }
    ui->project = (R01Project *)calloc(1, sizeof(R01Project));
    if (!ui->project) {
        return -1;
    }
    r01_project_init(ui->project, "untitled");
    ui->project_path[0] = '\0';
    ui_project_io_init(ui);
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
    ui->tile_clip_valid = 0;
    ui->tile_clip_w = 0;
    ui->tile_clip_h = 0;
    ui->screen_layer = UI_SCREEN_LAYER_BOTH;
    ui->hide_bg_layer = 0;
    ui->hide_spr_layer = 0;
    ui->banks_idx = 0;
    ui->global_banks_idx = 0;
    ui->global_banks_plane = UI_BANKS_PLANE_GLOBAL_SPR;
    ui->banks_plane = UI_BANKS_PLANE_SPR;
    ui->bank_sel_tile = -1;
    ui->bank_sel_bank = 0;
    ui->bank_sel_plane = UI_BANKS_PLANE_SPR;
    memset(ui->bank_sel_mask, 0, sizeof(ui->bank_sel_mask));
    memset(ui->bank_sel_mask_before, 0, sizeof(ui->bank_sel_mask_before));
    ui->bank_sel_drag = 0;
    ui->bank_sel_drag_moved = 0;
    ui->bank_sel_anchor = 0;
    ui->bank_sel_drag_tile = 0;
    ui->bank_sel_add = 0;
    ui->accordion_open = UI_ACC_WORLDS;
    ui->region_focus = UI_REGION_WORLDS;
    accordion_init_heights(ui);
    ui->logic_scale = 1;
    ui->menu.world_screen_idx = -1;
    ui->sel_instance = -1;
    ui->preview_inspect[0] = '\0';
    ui->app_mode = UI_APP_GRAPHICS;
    ui_undo_init(&ui->undo);
    ui->undo_paint = NULL;
    ui->undo_spr_paint = NULL;
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
    ui_undo_paint_end(ui);
    ui_undo_spr_paint_end(ui);
    ui_undo_shutdown(&ui->undo);
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
    r01_ui_chrome_set(NULL);
    if (g_cursor_arrow) {
        SDL_FreeCursor(g_cursor_arrow);
        g_cursor_arrow = NULL;
    }
    if (g_cursor_hand) {
        SDL_FreeCursor(g_cursor_hand);
        g_cursor_hand = NULL;
    }
    if (g_cursor_sizewe) {
        SDL_FreeCursor(g_cursor_sizewe);
        g_cursor_sizewe = NULL;
    }
    if (g_cursor_sizens) {
        SDL_FreeCursor(g_cursor_sizens);
        g_cursor_sizens = NULL;
    }
    if (g_cursor_sizenwse) {
        SDL_FreeCursor(g_cursor_sizenwse);
        g_cursor_sizenwse = NULL;
    }
    if (g_cursor_sizenesw) {
        SDL_FreeCursor(g_cursor_sizenesw);
        g_cursor_sizenesw = NULL;
    }
    if (g_cursor_sizese) {
        SDL_FreeCursor(g_cursor_sizese);
        g_cursor_sizese = NULL;
    }
    if (g_cursor_sizesw) {
        SDL_FreeCursor(g_cursor_sizesw);
        g_cursor_sizesw = NULL;
    }
    if (g_cursor_sizeall) {
        SDL_FreeCursor(g_cursor_sizeall);
        g_cursor_sizeall = NULL;
    }
    if (g_cursor_no) {
        SDL_FreeCursor(g_cursor_no);
        g_cursor_no = NULL;
    }
    font_shutdown();
}

void ui_tick(UiState *ui) {
    const Uint8 *keys;
    if (!ui) {
        return;
    }
    accordion_anim_tick(ui);
    entity_edit_preview_tick(ui);
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
    keys = SDL_GetKeyboardState(NULL);
    r01e_machine_set_pad(ui->play.machine, 0, r01_pad_bits_p1(keys));
    r01e_machine_set_pad(ui->play.machine, 1, r01_pad_bits_p2(keys));
    (void)r01e_machine_frame(ui->play.machine);
}

void ui_toggle_logic_scale(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->logic_scale = (ui->logic_scale == 1) ? 2 : 1;
    ui_toast(ui, ui->logic_scale == 2 ? "1280x720" : "640x360", 0);
}
