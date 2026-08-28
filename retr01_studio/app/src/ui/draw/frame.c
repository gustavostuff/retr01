#include "ui/ui.h"
#include "ui/internal.h"
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
    if (!ui || !ui->project || !r) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, UI_COL_BG_R, UI_COL_BG_G, UI_COL_BG_B, 255);
    SDL_RenderClear(r);

    draw_sidebar(ui, r);
    draw_button(r, play_btn_x(ui), play_btn_y(), play_btn_w(ui), ui->play.active ? "Stop" : "Play", 1,
                play_button_hit(ui, ui->mouse_x, ui->mouse_y));

    if (ui->play.active) {
        draw_play_view(ui, r);
    } else {
        draw_screen_editor(ui, r, r01_project_active_screen(ui->project));
        draw_screen_mode(ui, r);
    }

    if (ui->toast_until > SDL_GetTicks() && ui->toast[0]) {
        int tw = label_width(ui->toast);
        int ty = UI_LOGIC_H - UI_BTN_H - UI_UNIT;
        fill_rect(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast_error ? 60 : 30, ui->toast_error ? 24 : 36,
                  ui->toast_error ? 24 : 42);
        font_draw_centered(r, UI_UNIT, ty, tw, UI_BTN_H, ui->toast, 240, 240, 240);
    }

    draw_menu(ui, r);
    if (ui->pal_edit.open) {
        draw_pal_modal(ui, r);
    } else if (ui->tile_edit.open) {
        draw_tile_modal(ui, r);
    }
}
