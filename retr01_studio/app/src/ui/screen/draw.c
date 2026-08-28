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

void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s) {
    int ox, oy, y, x;
    R01World *w = r01_project_active_world(ui->project);
    screen_origin(ui, &ox, &oy);
    fill_rect(r, ox, oy, UI_SCREEN_W, UI_SCREEN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!s || !w) {
        font_draw_centered(r, ox, oy, UI_SCREEN_W, UI_SCREEN_H, "No screen", 160, 160, 170);
        return;
    }
    for (y = 0; y < R01_SCREEN_PX_H; y++) {
        for (x = 0; x < R01_SCREEN_PX_W; x++) {
            uint8_t cr, cg, cb;
            SDL_Rect px;
            r01_screen_pixel_rgb(ui->project, w, s, x, y, &cr, &cg, &cb);
            px.x = ox + x * UI_SCREEN_SCALE;
            px.y = oy + y * UI_SCREEN_SCALE;
            px.w = UI_SCREEN_SCALE;
            px.h = UI_SCREEN_SCALE;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    if (screen_sel_valid(ui) && ui->screen_mode == UI_SCREEN_MODE_SEL) {
        int min_x, min_y, max_x, max_y;
        int sx, sy, sw, sh;
        screen_sel_bounds(ui, &min_x, &min_y, &max_x, &max_y);
        sx = ox + min_x * 8 * UI_SCREEN_SCALE;
        sy = oy + min_y * 8 * UI_SCREEN_SCALE;
        sw = (max_x - min_x + 1) * 8 * UI_SCREEN_SCALE;
        sh = (max_y - min_y + 1) * 8 * UI_SCREEN_SCALE;
        draw_rect(r, sx, sy, sw, sh, 255, 255, 255);
    }
}

void draw_play_view(UiState *ui, SDL_Renderer *r) {
    int ox, oy, vy, vx;
    screen_origin(ui, &ox, &oy);
    for (vy = 0; vy < R01_SCREEN_PX_H; vy++) {
        for (vx = 0; vx < R01_SCREEN_PX_W; vx++) {
            uint8_t cr = 0, cg = 0, cb = 0;
            SDL_Rect px;
            r01_play_sample_bg(ui->project, &ui->play, vx, vy, &cr, &cg, &cb);
            px.x = ox + vx * UI_SCREEN_SCALE;
            px.y = oy + vy * UI_SCREEN_SCALE;
            px.w = UI_SCREEN_SCALE;
            px.h = UI_SCREEN_SCALE;
            SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
            SDL_RenderFillRect(r, &px);
        }
    }
    {
        int pcx, pcy;
        uint8_t pr, pg, pb;
        r01_project_player_rgb(ui->project, &pr, &pg, &pb);
        for (pcy = 0; pcy < R01_PLAY_PLAYER_H; pcy++) {
            for (pcx = 0; pcx < R01_PLAY_PLAYER_W; pcx++) {
                int wx = ui->play.player_x + pcx;
                int wy = ui->play.player_y + pcy;
                int vx2 = wx - ui->play.cam_x;
                int vy2 = wy - ui->play.cam_y;
                SDL_Rect px;
                if (vx2 < 0 || vy2 < 0 || vx2 >= R01_SCREEN_PX_W || vy2 >= R01_SCREEN_PX_H) {
                    continue;
                }
                px.x = ox + vx2 * UI_SCREEN_SCALE;
                px.y = oy + vy2 * UI_SCREEN_SCALE;
                px.w = UI_SCREEN_SCALE;
                px.h = UI_SCREEN_SCALE;
                SDL_SetRenderDrawColor(r, pr, pg, pb, 255);
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

