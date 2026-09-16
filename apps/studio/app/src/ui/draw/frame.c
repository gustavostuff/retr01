#include "ui/ui.h"
#include "ui/internal.h"
#include "ui/modals/project_io.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ui_draw(UiState *ui, SDL_Renderer *r) {
    int menu_blocks = 0;
    int saved_mx = 0;
    int saved_my = 0;
    if (!ui || !ui->project || !r) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B, 255);
    SDL_RenderClear(r);

    /* Context menus own the pointer: hide hover/hit feedback on chrome underneath. */
    menu_blocks = ui->menu.open;
    if (menu_blocks) {
        saved_mx = ui->mouse_x;
        saved_my = ui->mouse_y;
        ui->mouse_x = -10000;
        ui->mouse_y = -10000;
    }

    draw_app_mode_tabs(ui, r);

    if (ui->app_mode == UI_APP_SOUNDS) {
        draw_sound_editor(ui, r);
    } else if (ui->app_mode == UI_APP_CODE) {
        int cx = UI_SIDEBAR_W + UI_UNIT;
        int cy = UI_APP_CHROME_H + UI_UNIT * 2;
        font_draw(r, cx, cy, "Code editor TBD", 180, 180, 190);
    } else {
        draw_sidebar(ui, r);
        draw_ctrl_sidebar(ui, r);

        if (ui->play.active) {
            draw_play_view(ui, r);
        } else {
            draw_screen_editor(ui, r, ui_edit_map_screen(ui));
        }
    }

    if (ui->toast_until > SDL_GetTicks() && ui->toast[0]) {
        int tw = label_width(ui->toast);
        int ty = ui_logic_h(ui) - UI_BTN_H - UI_UNIT;
        fill_rect(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast_error ? 60 : 30, ui->toast_error ? 24 : 36,
                  ui->toast_error ? 24 : 42);
        font_draw_centered(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast, 240, 240, 240);
    }

    if (ui->app_mode == UI_APP_GRAPHICS) {
        if (ui->pal_edit.open) {
            draw_pal_modal(ui, r);
        } else if (ui->sprite_edit.open) {
            draw_sprite_modal(ui, r);
        } else if (ui->metasprite_edit.open) {
            draw_metasprite_modal(ui, r);
        } else if (ui->entity_edit.open) {
            draw_entity_modal(ui, r);
        } else if (ui->tile_edit.open) {
            draw_tile_modal(ui, r);
        }
        if (menu_blocks) {
            ui->mouse_x = saved_mx;
            ui->mouse_y = saved_my;
            menu_blocks = 0;
        }
        draw_menu(ui, r);
        draw_catalog_drag_ghost(ui, r);
    }
    if (menu_blocks) {
        ui->mouse_x = saved_mx;
        ui->mouse_y = saved_my;
    }
    if (ui_project_io_is_open(ui)) {
        ui_project_io_draw(ui, r);
    }
    draw_tooltip(ui, r);
}
