#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/sprites.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void draw_worlds_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    R01World *w = r01_project_active_world(ui->project);
    int i, col, row;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    for (i = 0; i < R01_MAX_WORLDS; i++) {
        char num[4];
        int x, y;
        int on = (i == ui->project->active_world);
        int hover;

        ui_world_btn_pos(i, lo->worlds_btns_y, &x, &y);
        hover = point_in_rect(lx, ly, x, y, UI_WORLD_BTN, UI_WORLD_BTN);

        if (on) {
            fill_rect(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        snprintf(num, sizeof(num), "%d", i + 1);
        font_draw(r, x + 2, y + 2, num, 240, 240, 240);
        if (hover) {
            hover_overlay(r, x, y, UI_WORLD_BTN, UI_WORLD_BTN);
        }
    }
    draw_chess_grid(r, UI_WORLDS_X, lo->worlds_grid_y, R01_GRID_MAX, R01_GRID_MAX, UI_WORLD_CELL);
    if (!w || !w->present) {
        return;
    }
    {
        int mark_idx = ui->play.active ? r01_play_screen_index(&ui->play, w) : w->default_screen;
        if (mark_idx < 0 || mark_idx >= w->screen_count || !w->screens[mark_idx].present) {
            mark_idx = r01_world_default_screen(w);
        }
        for (row = 0; row < R01_GRID_MAX; row++) {
            for (col = 0; col < R01_GRID_MAX; col++) {
                int idx = r01_world_screen_index(w, col, row);
                int x = UI_WORLDS_X + col * UI_WORLD_CELL;
                int y = lo->worlds_grid_y + row * UI_WORLD_CELL;
                int present = (idx >= 0 && w->screens[idx].present);
                int marked = present && idx == mark_idx;
                int hover = point_in_rect(lx, ly, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                if (present && !marked) {
                    fill_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_PRESENT_R, UI_COL_PRESENT_G,
                              UI_COL_PRESENT_B);
                }
                if (marked) {
                    fill_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_MARK_R, UI_COL_MARK_G, UI_COL_MARK_B);
                }
                if (hover) {
                    hover_overlay(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                }
            }
        }
        if (!ui->play.active) {
            int sel = ui->project->active_screen;
            if (sel >= 0 && sel < w->screen_count && w->screens[sel].present) {
                int x = UI_WORLDS_X + w->screens[sel].col * UI_WORLD_CELL;
                int y = lo->worlds_grid_y + w->screens[sel].row * UI_WORLD_CELL;
                draw_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, 255, 255, 255);
            }
        }
    }
}

static void draw_palettes(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int row = ui->pal_edit.open ? ui->pal_edit.row : (w ? w->default_pal_row : 0);
    int pal, c, i;
    int bg_strip_y = lo->pals_body_y;
    int spr_strip_y = lo->pals_body_y + UI_PAL_SWATCH;
    int row_btns_y = lo->pals_body_y + UI_PAL_SWATCH * 2;
    if (row < 0) {
        row = 0;
    }
    if (row >= R01_PAL_ROWS) {
        row = R01_PAL_ROWS - 1;
    }
    for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
        for (c = 0; c < R01_PAL_COLORS; c++) {
            uint8_t cr, cg, cb;
            r01_kit_rgb(ui->project->global_pal_bg[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, UI_WORLDS_X + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, bg_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
            r01_kit_rgb(ui->project->global_pal_spr[row][pal].idx[c], &cr, &cg, &cb);
            fill_rect(r, UI_WORLDS_X + (pal * R01_PAL_COLORS + c) * UI_PAL_SWATCH, spr_strip_y, UI_PAL_SWATCH,
                      UI_PAL_SWATCH, cr, cg, cb);
        }
    }
    for (i = 0; i < R01_PAL_ROWS; i++) {
        char num[4];
        int x = UI_WORLDS_X + i * UI_WORLD_BTN;
        int on = (i == row);
        int hover = point_in_rect(ui->mouse_x, ui->mouse_y, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H);
        if (on) {
            fill_rect(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, UI_COL_ACTIVE_R, UI_COL_ACTIVE_G, UI_COL_ACTIVE_B);
        } else {
            fill_rect(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        }
        snprintf(num, sizeof(num), "%d", i);
        font_draw_centered(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H, num, 240, 240, 240);
        if (hover) {
            hover_overlay(r, x, row_btns_y, UI_WORLD_BTN, UI_BTN_H);
        }
    }
}

static void draw_sprite_icon(UiState *ui, SDL_Renderer *r, const R01World *w, const R01SpriteDef *sp, int dx,
                             int dy) {
    const uint8_t *tile;
    int row = w->default_pal_row;
    int sy, sx;
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    tile = r01_chr_spr_tile(w, sp->bank, sp->tile_id);
    if (!tile) {
        fill_rect(r, dx, dy, UI_SPRITE_ICON, UI_SPRITE_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        return;
    }
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(tile, sx, sy);
            uint8_t cr, cg, cb;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_spr[row][sp->pal & 3].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, dx + sx, dy + sy, 1, 1, cr, cg, cb);
        }
    }
}

static void draw_sprites_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    int add_y = lo->sprites_body_y + UI_SPRITES_BODY_H - UI_BTN_H;
    int add_w = label_width("Add");
    int add_hover = point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H);
    int vis = (UI_SPRITES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
    int i;

    fill_rect(r, 0, lo->sprites_body_y, UI_SIDEBAR_W, UI_SPRITES_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (!w || w->sprite_count < 1) {
        font_draw_centered(r, 0, lo->sprites_body_y, UI_SIDEBAR_W, UI_BTN_H * 2, "empty", 160, 160, 170);
    } else {
        int max_scroll = w->sprite_count - vis;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (ui->sprites_scroll > max_scroll) {
            ui->sprites_scroll = max_scroll;
        }
        if (ui->sprites_scroll < 0) {
            ui->sprites_scroll = 0;
        }
        for (i = 0; i < vis; i++) {
            int idx = ui->sprites_scroll + i;
            int y = lo->sprites_body_y + i * UI_SPRITE_ROW_H;
            char label[16];
            int hover;
            if (idx >= w->sprite_count) {
                break;
            }
            hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H);
            if (hover) {
                fill_rect(r, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            }
            draw_sprite_icon(ui, r, w, &w->sprites[idx], UI_UNIT, y + (UI_SPRITE_ROW_H - UI_SPRITE_ICON) / 2);
            snprintf(label, sizeof(label), "%d", w->sprites[idx].tile_id);
            font_draw(r, UI_UNIT + UI_SPRITE_ICON + 4, y + 4, label, 230, 230, 230);
        }
    }

    draw_button(r, UI_WORLDS_X + UI_UNIT, add_y, add_w, "Add", 1, add_hover);
}

static void draw_entity_icon(UiState *ui, SDL_Renderer *r, const R01World *w, const R01EntityType *ent, int dx,
                             int dy) {
    const R01EntityFrame *fr;
    const R01EntityPart *pt;
    const uint8_t *tile;
    int row = w->default_pal_row;
    int sy, sx;
    fill_rect(r, dx, dy, UI_SPRITE_ICON, UI_SPRITE_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!ent || ent->state_count < 1 || ent->states[0].frame_count < 1) {
        return;
    }
    fr = &ent->states[0].frames[0];
    if (fr->part_count < 1) {
        return;
    }
    pt = &fr->parts[0];
    if (row < 0 || row >= R01_PAL_ROWS) {
        row = 0;
    }
    tile = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
    if (!tile) {
        return;
    }
    for (sy = 0; sy < 8; sy++) {
        for (sx = 0; sx < 8; sx++) {
            uint8_t col = r01_tile_pixel_color(tile, sx, sy);
            uint8_t cr, cg, cb;
            if (col == 0) {
                continue;
            }
            r01_kit_rgb(ui->project->global_pal_spr[row][pt->pal & 3].idx[col & 3u], &cr, &cg, &cb);
            fill_rect(r, dx + sx, dy + sy, 1, 1, cr, cg, cb);
        }
    }
}

static void draw_metasprite_icon(UiState *ui, SDL_Renderer *r, const R01World *w, const R01MetaspriteDef *ms,
                                 int dx, int dy) {
    int i;
    fill_rect(r, dx, dy, UI_SPRITE_ICON, UI_SPRITE_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!ms || !w) {
        return;
    }
    for (i = 0; i < ms->frame.part_count; i++) {
        const R01EntityPart *pt = &ms->frame.parts[i];
        const uint8_t *tile;
        int row = w->default_pal_row;
        int sy, sx;
        if (row < 0 || row >= R01_PAL_ROWS) {
            row = 0;
        }
        tile = r01_chr_spr_tile(w, pt->bank, pt->tile_id);
        if (!tile) {
            continue;
        }
        for (sy = 0; sy < 8; sy++) {
            for (sx = 0; sx < 8; sx++) {
                uint8_t col = r01_tile_pixel_color(tile, sx, sy);
                uint8_t cr, cg, cb;
                if (col == 0) {
                    continue;
                }
                r01_kit_rgb(ui->project->global_pal_spr[row][pt->pal & 3].idx[col & 3u], &cr, &cg, &cb);
                fill_rect(r, dx + sx + pt->dx, dy + sy + pt->dy, 1, 1, cr, cg, cb);
            }
        }
    }
}

static void draw_metasprites_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    int add_y = lo->metasprites_body_y + UI_METASPRITES_BODY_H - UI_BTN_H;
    int add_w = label_width("Add");
    int add_hover = point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H);
    int vis = (UI_METASPRITES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
    int i;

    fill_rect(r, 0, lo->metasprites_body_y, UI_SIDEBAR_W, UI_METASPRITES_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (!w || w->metasprite_count < 1) {
        font_draw_centered(r, 0, lo->metasprites_body_y, UI_SIDEBAR_W, UI_BTN_H * 2, "empty", 160, 160, 170);
    } else {
        int max_scroll = w->metasprite_count - vis;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (ui->metasprites_scroll > max_scroll) {
            ui->metasprites_scroll = max_scroll;
        }
        if (ui->metasprites_scroll < 0) {
            ui->metasprites_scroll = 0;
        }
        for (i = 0; i < vis; i++) {
            int idx = ui->metasprites_scroll + i;
            int y = lo->metasprites_body_y + i * UI_SPRITE_ROW_H;
            const char *label;
            int hover;
            if (idx >= w->metasprite_count) {
                break;
            }
            hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H);
            if (hover) {
                fill_rect(r, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            }
            draw_metasprite_icon(ui, r, w, &w->metasprites[idx], UI_UNIT, y + (UI_SPRITE_ROW_H - UI_SPRITE_ICON) / 2);
            label = w->metasprites[idx].name[0] ? w->metasprites[idx].name : "meta";
            font_draw(r, UI_UNIT + UI_SPRITE_ICON + 4, y + 4, label, 230, 230, 230);
        }
    }

    draw_button(r, UI_WORLDS_X + UI_UNIT, add_y, add_w, "Add", 1, add_hover);
}

static void draw_entities_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    int add_y = lo->entities_body_y + UI_ENTITIES_BODY_H - UI_BTN_H;
    int add_w = label_width("Add");
    int add_hover = point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H);
    int vis = (UI_ENTITIES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
    int i;

    fill_rect(r, 0, lo->entities_body_y, UI_SIDEBAR_W, UI_ENTITIES_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (!w || w->entity_count < 1) {
        font_draw_centered(r, 0, lo->entities_body_y, UI_SIDEBAR_W, UI_BTN_H * 2, "empty", 160, 160, 170);
    } else {
        int max_scroll = w->entity_count - vis;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (ui->entities_scroll > max_scroll) {
            ui->entities_scroll = max_scroll;
        }
        if (ui->entities_scroll < 0) {
            ui->entities_scroll = 0;
        }
        for (i = 0; i < vis; i++) {
            int idx = ui->entities_scroll + i;
            int y = lo->entities_body_y + i * UI_SPRITE_ROW_H;
            const char *label;
            int hover;
            if (idx >= w->entity_count) {
                break;
            }
            hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H);
            if (hover) {
                fill_rect(r, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            }
            draw_entity_icon(ui, r, w, &w->entities[idx], UI_UNIT, y + (UI_SPRITE_ROW_H - UI_SPRITE_ICON) / 2);
            label = w->entities[idx].states[0].name;
            if (!label[0]) {
                label = "entity";
            }
            font_draw(r, UI_UNIT + UI_SPRITE_ICON + 4, y + 4, label, 230, 230, 230);
        }
    }

    draw_button(r, UI_WORLDS_X + UI_UNIT, add_y, add_w, "Add", 1, add_hover);
}

void draw_sidebar(UiState *ui, SDL_Renderer *r) {
    AccordionLayout lo;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    accordion_layout(ui, &lo);
    fill_rect(r, 0, 0, UI_SIDEBAR_W, UI_LOGIC_H, UI_COL_PANEL_R, UI_COL_PANEL_G, UI_COL_PANEL_B);

    draw_accordion_header(r, lo.worlds_hdr_y, "Worlds", lo.worlds_open,
                          point_in_rect(lx, ly, 0, lo.worlds_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.worlds_open) {
        draw_worlds_body(ui, r, &lo);
    }

    draw_accordion_header(r, lo.pals_hdr_y, "Palettes", lo.pals_open,
                          point_in_rect(lx, ly, 0, lo.pals_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.pals_open) {
        draw_palettes(ui, r, &lo);
    }

    draw_accordion_header(r, lo.sprites_hdr_y, "Sprites", lo.sprites_open,
                          point_in_rect(lx, ly, 0, lo.sprites_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.sprites_open) {
        draw_sprites_body(ui, r, &lo);
    }

    draw_accordion_header(r, lo.metasprites_hdr_y, "Metasprites", lo.metasprites_open,
                          point_in_rect(lx, ly, 0, lo.metasprites_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.metasprites_open) {
        draw_metasprites_body(ui, r, &lo);
    }

    draw_accordion_header(r, lo.entities_hdr_y, "Entities", lo.entities_open,
                          point_in_rect(lx, ly, 0, lo.entities_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.entities_open) {
        draw_entities_body(ui, r, &lo);
    }
}
