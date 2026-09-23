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
#include "retr01_studio/metatiles.h"
#include "retr01_studio/sprites.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void accordion_body_clip(SDL_Renderer *r, int body_y, int body_h, UiClipStack *stack) {
    ui_clip_push(r, 0, body_y, UI_SIDEBAR_W, body_h, stack);
}

static void accordion_body_clip_pop(SDL_Renderer *r, const UiClipStack *stack) {
    ui_clip_pop(r, stack);
}

static void draw_worlds_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    R01World *w = r01_project_active_world(ui->project);
    R01Project *p = ui->project;
    int col, row;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    UiTabPager pg;
    int plane_bg0 = (ui->worlds_plane == UI_WORLDS_PLANE_BG0);
    static const char *const plane_labs[2] = {"BG1", "BG0"};

    worlds_pager_prepare(ui, &pg);
    ui_tab_pager_draw(r, &pg, lx, ly);
    ui_multi_state_draw(r, pg.plane_x, pg.y, pg.plane_w, plane_labs, 2, ui->worlds_plane, lx, ly);

    draw_chess_grid(r, UI_WORLDS_X, lo->worlds_grid_y, R01_GRID_MAX, R01_GRID_MAX, UI_WORLD_CELL);

    if (!w || (!plane_bg0 && !w->present)) {
        return;
    }
    if (!plane_bg0) {
        int mark_idx = w->default_screen;
        if (ui->play.active) {
            mark_idx = ui_play_screen_mark(ui);
        }
        if (mark_idx < 0 || mark_idx >= w->screen_count || !w->screens[mark_idx].present) {
            if (!ui->play.active || ui->play.booting) {
                mark_idx = r01_world_default_screen(w);
            }
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
                    fill_rect_alpha(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_PRESENT_R, UI_COL_PRESENT_G,
                                    UI_COL_PRESENT_B, 204);
                }
                if (marked) {
                    fill_rect_alpha(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_MARK_R, UI_COL_MARK_G,
                                    UI_COL_MARK_B, 204);
                }
                if (hover) {
                    hover_overlay(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
                }
            }
        }
        if (!ui->play.active && ui->world_sel_col >= 0 && ui->world_sel_row >= 0 &&
            ui->world_sel_col < R01_GRID_MAX && ui->world_sel_row < R01_GRID_MAX) {
            int x = UI_WORLDS_X + ui->world_sel_col * UI_WORLD_CELL;
            int y = lo->worlds_grid_y + ui->world_sel_row * UI_WORLD_CELL;
            draw_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, 255, 255, 255);
        }
        return;
    }

    for (row = 0; row < R01_GRID_MAX; row++) {
        for (col = 0; col < R01_GRID_MAX; col++) {
            int idx = r01_world_bg0_screen_index(w, col, row);
            int x = UI_WORLDS_X + col * UI_WORLD_CELL;
            int y = lo->worlds_grid_y + row * UI_WORLD_CELL;
            int present = (idx >= 0);
            int marked = present && idx == w->bg0_active_screen;
            int hover = point_in_rect(lx, ly, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
            if (present && !marked) {
                fill_rect_alpha(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_PRESENT_R, UI_COL_PRESENT_G,
                                UI_COL_PRESENT_B, 204);
            }
            if (marked) {
                fill_rect_alpha(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, UI_COL_MARK_R, UI_COL_MARK_G, UI_COL_MARK_B,
                                204);
            }
            if (hover) {
                hover_overlay(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL);
            }
        }
    }
    if (!ui->play.active && ui->world_sel_col >= 0 && ui->world_sel_row >= 0 &&
        ui->world_sel_col < R01_GRID_MAX && ui->world_sel_row < R01_GRID_MAX) {
        int x = UI_WORLDS_X + ui->world_sel_col * UI_WORLD_CELL;
        int y = lo->worlds_grid_y + ui->world_sel_row * UI_WORLD_CELL;
        draw_rect(r, x, y, UI_WORLD_CELL, UI_WORLD_CELL, 255, 255, 255);
    }
}

static void draw_palettes(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
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

static void draw_bank_sel_overlay(UiState *ui, SDL_Renderer *r, int plane, int bank, int grid_y) {
    int id;
    if (!ui) {
        return;
    }
    if (bank_sel_valid(ui) && ui->bank_sel_plane == plane && ui->bank_sel_bank == bank) {
        for (id = 0; id < R01_TILES_PER_BANK; id++) {
            int sx, sy;
            if (!bank_sel_has(ui, id)) {
                continue;
            }
            sx = UI_WORLDS_X + (id % 16) * 8;
            sy = grid_y + (id / 16) * 8;
            draw_marching_ants(r, sx, sy, 8, 8);
        }
    }
    if (ui->bank_sel_drag && ui->bank_sel_plane == plane && ui->bank_sel_bank == bank) {
        int a = ui->bank_sel_anchor;
        int b = ui->bank_sel_drag_tile;
        int x0 = a % 16;
        int y0 = a / 16;
        int x1 = b % 16;
        int y1 = b / 16;
        int rx, ry, rw, rh;
        if (x0 > x1) {
            int t = x0;
            x0 = x1;
            x1 = t;
        }
        if (y0 > y1) {
            int t = y0;
            y0 = y1;
            y1 = t;
        }
        rx = UI_WORLDS_X + x0 * 8;
        ry = grid_y + y0 * 8;
        rw = (x1 - x0 + 1) * 8;
        rh = (y1 - y0 + 1) * 8;
        draw_marching_ants(r, rx, ry, rw, rh);
    }
}

static void draw_banks_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    UiTabPager pg;
    int bank = ui->banks_idx;
    int grid_y = lo->sprites_body_y + UI_WORLDS_TAB_STACK_H;
    int tx, ty;
    int row = w ? w->default_pal_row : 0;

    fill_rect(r, 0, lo->sprites_body_y, UI_SIDEBAR_W, UI_BANKS_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (bank < 0) {
        bank = 0;
    }
    if (bank >= UI_BANKS_N) {
        bank = UI_BANKS_N - 1;
    }
    banks_pager_prepare(ui, &pg);
    ui_tab_pager_draw(r, &pg, lx, ly);

    fill_rect(r, UI_WORLDS_X, grid_y, UI_BANKS_GRID, UI_BANKS_GRID, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!w) {
        return;
    }
    for (ty = 0; ty < 16; ty++) {
        for (tx = 0; tx < 16; tx++) {
            int tile_id = ty * 16 + tx;
            int dx = UI_WORLDS_X + tx * 8;
            int dy = grid_y + ty * 8;
            int sx, sy;
            int blank = 1;
            const uint8_t *tile = NULL;
            if (tile_id < p->chr_banks[bank].tile_count) {
                tile = p->chr_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
            }
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
                    blank = 0;
                    r01_kit_rgb(ui->project->global_pal_bg[row][0].idx[col & 3u], &cr, &cg, &cb);
                    fill_rect(r, dx + sx, dy + sy, 1, 1, cr, cg, cb);
                }
            }
            /* Allocated blank (tile 0 fallback): chess so it is not well-colored empty. */
            if (blank) {
                draw_chess_grid(r, dx, dy, 4, 4, 2);
            }
        }
    }
    if (banks_cell_hit(ui, lx, ly, NULL)) {
        int tid;
        int hx, hy;
        banks_cell_hit(ui, lx, ly, &tid);
        hx = UI_WORLDS_X + (tid % 16) * 8;
        hy = grid_y + (tid / 16) * 8;
        hover_overlay(r, hx, hy, 8, 8);
    }
    draw_bank_sel_overlay(ui, r, ui->banks_plane, bank, grid_y);
}

static void draw_global_banks_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    UiTabsLayout tabs;
    int bank = ui->global_banks_idx;
    int grid_y = lo->global_banks_body_y + UI_WORLDS_TAB_STACK_H;
    int tx, ty;
    int row = w ? w->default_pal_row : 0;
    int sel_plane = UI_BANKS_PLANE_GLOBAL_BG;

    fill_rect(r, 0, lo->global_banks_body_y, UI_SIDEBAR_W, UI_GLOBAL_BANKS_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (bank < 0) {
        bank = 0;
    }
    if (bank >= UI_BANKS_N) {
        bank = UI_BANKS_N - 1;
    }
    global_banks_tabs_prepare(ui, &tabs);
    ui_tabs_draw(r, &tabs, bank, lx, ly);

    fill_rect(r, UI_WORLDS_X, grid_y, UI_BANKS_GRID, UI_BANKS_GRID, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    if (!ui->project) {
        return;
    }
    for (ty = 0; ty < 16; ty++) {
        for (tx = 0; tx < 16; tx++) {
            int tile_id = ty * 16 + tx;
            int dx = UI_WORLDS_X + tx * 8;
            int dy = grid_y + ty * 8;
            int sx, sy;
            int blank = 1;
            const uint8_t *tile = NULL;
            tile = r01_other_bg_tile(ui->project, bank, tile_id);
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
                    blank = 0;
                    r01_kit_rgb(ui->project->global_pal_bg[row][0].idx[col & 3u], &cr, &cg, &cb);
                    fill_rect(r, dx + sx, dy + sy, 1, 1, cr, cg, cb);
                }
            }
            if (blank) {
                draw_chess_grid(r, dx, dy, 4, 4, 2);
            }
        }
    }
    if (global_banks_cell_hit(ui, lx, ly, NULL)) {
        int tid;
        int hx, hy;
        global_banks_cell_hit(ui, lx, ly, &tid);
        hx = UI_WORLDS_X + (tid % 16) * 8;
        hy = grid_y + (tid / 16) * 8;
        hover_overlay(r, hx, hy, 8, 8);
    }
    draw_bank_sel_overlay(ui, r, sel_plane, bank, grid_y);
}

static void draw_sprites_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    draw_banks_body(ui, r, lo);
}

static void draw_entity_icon(UiState *ui, SDL_Renderer *r, const R01World *w, const R01EntityType *ent, int dx,
                             int dy) {
    const R01EntityFrame *fr;
    if (!ent || ent->state_count < 1 || ent->states[0].frame_count < 1) {
        fill_rect(r, dx, dy, UI_PREVIEW_ICON, UI_PREVIEW_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        return;
    }
    fr = &ent->states[0].frames[0];
    ui_compose_draw_frame_icon(r, ui->project, w, fr, dx, dy, UI_PREVIEW_ICON);
}

static void draw_metasprite_icon(UiState *ui, SDL_Renderer *r, const R01World *w, const R01MetaspriteDef *ms,
                                 int dx, int dy) {
    if (!ms || !w) {
        fill_rect(r, dx, dy, UI_PREVIEW_ICON, UI_PREVIEW_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
        return;
    }
    ui_compose_draw_frame_icon(r, ui->project, w, &ms->frame, dx, dy, UI_PREVIEW_ICON);
}

static void draw_metatiles_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    int add_y = lo->metatiles_body_y + UI_METATILES_BODY_H - UI_BTN_H;
    int add_w = label_width("Add");
    int add_hover = point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H);
    int vis = (UI_METATILES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
    int i;

    fill_rect(r, 0, lo->metatiles_body_y, UI_SIDEBAR_W, UI_METATILES_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (!w || p->metatile_count < 1) {
        font_draw_centered(r, 0, lo->metatiles_body_y, UI_SIDEBAR_W, UI_BTN_H * 2, "empty", 160, 160, 170);
    } else {
        int max_scroll = p->metatile_count - vis;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (ui->metatiles_scroll > max_scroll) {
            ui->metatiles_scroll = max_scroll;
        }
        if (ui->metatiles_scroll < 0) {
            ui->metatiles_scroll = 0;
        }
        for (i = 0; i < vis; i++) {
            int idx = ui->metatiles_scroll + i;
            int y = lo->metatiles_body_y + i * UI_SPRITE_ROW_H;
            const char *label;
            int hover;
            if (idx >= p->metatile_count) {
                break;
            }
            hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H);
            if (hover) {
                fill_rect(r, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            }
            fill_rect(r, 0, y, UI_PREVIEW_ICON, UI_PREVIEW_ICON, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            label = r01_metatile_display_name(&p->metatiles[idx]);
            font_draw_clipped(r, UI_PREVIEW_ICON + 2, y + 4, UI_PREVIEW_ICON + 2, y,
                              UI_SIDEBAR_W - (UI_PREVIEW_ICON + 2), UI_SPRITE_ROW_H, label, 230, 230, 230);
            if (hover) {
                char id[R01_ID_MAX];
                int wi = ui->project ? ui->project->active_world : 0;
                r01_metatile_id(id, sizeof(id), wi, &p->metatiles[idx]);
                ui_tooltip_hover(ui, lx, ly, label, id);
            }
        }
    }

    draw_button(r, UI_WORLDS_X + UI_UNIT, add_y, add_w, "Add", 1, add_hover);
}

static void draw_metasprites_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    int add_y = lo->metasprites_body_y + UI_METASPRITES_BODY_H - UI_BTN_H;
    int add_w = label_width("Add");
    int add_hover = point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H);
    int vis = (UI_METASPRITES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
    int i;

    fill_rect(r, 0, lo->metasprites_body_y, UI_SIDEBAR_W, UI_METASPRITES_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (!w || p->metasprite_count < 1) {
        font_draw_centered(r, 0, lo->metasprites_body_y, UI_SIDEBAR_W, UI_BTN_H * 2, "empty", 160, 160, 170);
    } else {
        int max_scroll = p->metasprite_count - vis;
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
            if (idx >= p->metasprite_count) {
                break;
            }
            hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H);
            if (hover) {
                fill_rect(r, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            }
            draw_metasprite_icon(ui, r, w, &p->metasprites[idx], 0, y);
            label = r01_metasprite_display_name(&p->metasprites[idx]);
            font_draw_clipped(r, UI_PREVIEW_ICON + 2, y + 4, UI_PREVIEW_ICON + 2, y,
                              UI_SIDEBAR_W - (UI_PREVIEW_ICON + 2), UI_SPRITE_ROW_H, label, 230, 230, 230);
            if (hover) {
                char id[R01_ID_MAX];
                int wi = ui->project ? ui->project->active_world : 0;
                r01_metasprite_id(id, sizeof(id), wi, &p->metasprites[idx]);
                ui_tooltip_hover(ui, lx, ly, label, id);
            }
        }
    }

    draw_button(r, UI_WORLDS_X + UI_UNIT, add_y, add_w, "Add", 1, add_hover);
}

static void draw_entities_body(UiState *ui, SDL_Renderer *r, const AccordionLayout *lo) {
    const R01World *w = r01_project_active_world_const(ui->project);
    const R01Project *p = ui->project;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;
    int add_y = lo->entities_body_y + UI_ENTITIES_BODY_H - UI_BTN_H;
    int add_w = label_width("Add");
    int imp_w = label_width("Import");
    int imp_x = UI_WORLDS_X + UI_UNIT + add_w + UI_UNIT;
    int add_hover = point_in_rect(lx, ly, UI_WORLDS_X + UI_UNIT, add_y, add_w, UI_BTN_H);
    int imp_hover = point_in_rect(lx, ly, imp_x, add_y, imp_w, UI_BTN_H);
    int vis = (UI_ENTITIES_BODY_H - UI_BTN_H) / UI_SPRITE_ROW_H;
    int i;

    fill_rect(r, 0, lo->entities_body_y, UI_SIDEBAR_W, UI_ENTITIES_BODY_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (!w || p->entity_count < 1) {
        font_draw_centered(r, 0, lo->entities_body_y, UI_SIDEBAR_W, UI_BTN_H * 2, "empty", 160, 160, 170);
    } else {
        int max_scroll = p->entity_count - vis;
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
            if (idx >= p->entity_count) {
                break;
            }
            hover = point_in_rect(lx, ly, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H);
            if (hover) {
                fill_rect(r, 0, y, UI_SIDEBAR_W, UI_SPRITE_ROW_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
            }
            draw_entity_icon(ui, r, w, &p->entities[idx], 0, y);
            label = r01_entity_display_name(&p->entities[idx]);
            font_draw_clipped(r, UI_PREVIEW_ICON + 2, y + 4, UI_PREVIEW_ICON + 2, y,
                              UI_SIDEBAR_W - (UI_PREVIEW_ICON + 2), UI_SPRITE_ROW_H, label, 230, 230, 230);
            if (hover) {
                char id[R01_ID_MAX];
                int wi = ui->project ? ui->project->active_world : 0;
                r01_entity_type_id(id, sizeof(id), wi, &p->entities[idx]);
                ui_tooltip_hover(ui, lx, ly, label, id);
            }
        }
    }

    draw_button(r, UI_WORLDS_X + UI_UNIT, add_y, add_w, "Add", 1, add_hover);
    draw_button(r, imp_x, add_y, imp_w, "Import", 1, imp_hover);
    if (imp_hover) {
        ui_tooltip_hover(ui, lx, ly, "Import from aseprite_entities", NULL);
    }
}

void draw_sidebar(UiState *ui, SDL_Renderer *r) {
    AccordionLayout lo;
    UiClipStack clip;
    int lx = ui->mouse_x;
    int ly = ui->mouse_y;

    accordion_layout(ui, &lo);
    ui_tooltip_frame_begin(ui);
    fill_rect(r, 0, UI_APP_CHROME_H, UI_SIDEBAR_W, ui_logic_h(ui) - UI_APP_CHROME_H, UI_COL_PANEL_R, UI_COL_PANEL_G,
              UI_COL_PANEL_B);

    if (lo.worlds_body_h > 0) {
        accordion_body_clip(r, lo.worlds_btns_y, lo.worlds_body_h, &clip);
        draw_worlds_body(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }
    if (lo.pals_body_h > 0) {
        accordion_body_clip(r, lo.pals_body_y, lo.pals_body_h, &clip);
        draw_palettes(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }
    if (lo.sprites_body_h > 0) {
        accordion_body_clip(r, lo.sprites_body_y, lo.sprites_body_h, &clip);
        draw_sprites_body(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }
    if (lo.global_banks_body_h > 0) {
        accordion_body_clip(r, lo.global_banks_body_y, lo.global_banks_body_h, &clip);
        draw_global_banks_body(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }
    if (lo.metatiles_body_h > 0) {
        accordion_body_clip(r, lo.metatiles_body_y, lo.metatiles_body_h, &clip);
        draw_metatiles_body(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }
    if (lo.metasprites_body_h > 0) {
        accordion_body_clip(r, lo.metasprites_body_y, lo.metasprites_body_h, &clip);
        draw_metasprites_body(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }
    if (lo.entities_body_h > 0) {
        accordion_body_clip(r, lo.entities_body_y, lo.entities_body_h, &clip);
        draw_entities_body(ui, r, &lo);
        accordion_body_clip_pop(r, &clip);
    }

    draw_accordion_header(r, lo.worlds_hdr_y, "Worlds", lo.worlds_open,
                          point_in_rect(lx, ly, 0, lo.worlds_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    draw_accordion_header(r, lo.sprites_hdr_y, "Banks", lo.sprites_open,
                          point_in_rect(lx, ly, 0, lo.sprites_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    if (lo.global_banks_hdr_y >= 0) {
        draw_accordion_header(r, lo.global_banks_hdr_y, "Global banks", lo.global_banks_open,
                              point_in_rect(lx, ly, 0, lo.global_banks_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    }
    if (UI_SHOW_METATILES) {
        draw_accordion_header(r, lo.metatiles_hdr_y, "Metatiles", lo.metatiles_open,
                              point_in_rect(lx, ly, 0, lo.metatiles_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    }
    if (UI_SHOW_METASPRITES) {
        draw_accordion_header(r, lo.metasprites_hdr_y, "Metasprites", lo.metasprites_open,
                              point_in_rect(lx, ly, 0, lo.metasprites_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    }
    draw_accordion_header(r, lo.entities_hdr_y, "Entities", lo.entities_open,
                          point_in_rect(lx, ly, 0, lo.entities_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    draw_accordion_header(r, lo.pals_hdr_y, "Palettes", lo.pals_open,
                          point_in_rect(lx, ly, 0, lo.pals_hdr_y, UI_SIDEBAR_W, UI_BTN_H));
    ui_tooltip_frame_end(ui);
}
