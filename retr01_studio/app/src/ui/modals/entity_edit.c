#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <string.h>

static R01EntityState *edit_state(UiState *ui) {
    return r01_entity_state(&ui->entity_edit.draft, ui->entity_edit.state);
}

static R01EntityFrame *edit_frame(UiState *ui) {
    return r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, ui->entity_edit.frame);
}

static int state_unlock_count(const UiState *ui) {
    int n = ui->entity_edit.draft.state_count + 1;
    if (n > R01_ENTITY_STATES_MAX) {
        n = R01_ENTITY_STATES_MAX;
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

static int frame_unlock_count(UiState *ui) {
    R01EntityState *st = r01_entity_state(&ui->entity_edit.draft, ui->entity_edit.state);
    int n = 1;
    if (st) {
        n = st->frame_count + 1;
    }
    if (n > R01_ENTITY_FRAMES_MAX) {
        n = R01_ENTITY_FRAMES_MAX;
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

static int entity_zoom(const UiState *ui) {
    int z = ui ? ui->entity_edit.zoom : 1;
    if (z != 1 && z != 2 && z != 4) {
        return 1;
    }
    return z;
}

static int entity_view_cells(int zoom) {
    return R01_ENTITY_COMPOSE_PX / zoom;
}

static int entity_scale(int zoom) {
    return UI_ENTITY_COMPOSE / entity_view_cells(zoom);
}

static void entity_clamp_view(UiState *ui) {
    int z = entity_zoom(ui);
    int vc = entity_view_cells(z);
    int max_v = R01_ENTITY_COMPOSE_PX - vc;
    if (max_v < 0) {
        max_v = 0;
    }
    if (ui->entity_edit.view_x < 0) {
        ui->entity_edit.view_x = 0;
    }
    if (ui->entity_edit.view_y < 0) {
        ui->entity_edit.view_y = 0;
    }
    if (ui->entity_edit.view_x > max_v) {
        ui->entity_edit.view_x = max_v;
    }
    if (ui->entity_edit.view_y > max_v) {
        ui->entity_edit.view_y = max_v;
    }
}

static void entity_screen_to_world(const UiState *ui, const EntityModalLayout *lo, int lx, int ly, int *wx,
                                  int *wy) {
    int z = entity_zoom(ui);
    int sc = entity_scale(z);
    int vx = ui->entity_edit.view_x;
    int vy = ui->entity_edit.view_y;
    if (wx) {
        *wx = vx + (lx - lo->right_grid_x) / sc;
    }
    if (wy) {
        *wy = vy + (ly - lo->right_grid_y) / sc;
    }
}

static void draw_meta_icon(UiState *ui, SDL_Renderer *r, const R01MetaspriteDef *ms, int dx, int dy) {
    const R01World *w = r01_project_active_world_const(ui->project);
    ui_compose_draw_frame_icon(r, ui->project, w, ms ? &ms->frame : NULL, dx, dy, UI_PREVIEW_ICON);
}

static int modal_meta_rows(const EntityModalLayout *lo) {
    return lo->left_list_h / UI_SPRITE_ROW_H;
}

static int modal_meta_list_hit(UiState *ui, int lx, int ly, int *out_idx) {
    EntityModalLayout lo;
    const R01World *w;
    int row, idx;
    entity_modal_layout(ui, &lo);
    w = r01_project_active_world_const(ui->project);
    if (!w || w->metasprite_count < 1) {
        return 0;
    }
    if (lx < lo.left_list_x || lx >= lo.left_list_x + UI_ENTITY_BANK_GRID || ly < lo.left_list_y ||
        ly >= lo.left_list_y + lo.left_list_h) {
        return 0;
    }
    row = (ly - lo.left_list_y) / UI_SPRITE_ROW_H;
    idx = ui->entity_edit.meta_scroll + row;
    if (idx < 0 || idx >= w->metasprite_count) {
        return 0;
    }
    if (out_idx) {
        *out_idx = idx;
    }
    return 1;
}

void entity_edit_open_new(UiState *ui) {
    if (!ui) {
        return;
    }
    memset(&ui->entity_edit, 0, sizeof(ui->entity_edit));
    ui->entity_edit.open = 1;
    ui->entity_edit.is_new = 1;
    ui->entity_edit.type_idx = -1;
    r01_entity_type_init(&ui->entity_edit.draft);
    ui->entity_edit.state = 0;
    ui->entity_edit.frame = 0;
    ui->entity_edit.sel_part = -1;
    ui->entity_edit.paint_color = 1;
    ui->entity_edit.paint_pal = 0;
    ui->entity_edit.show_guides = 1;
    ui->entity_edit.meta_scroll = 0;
    ui->entity_edit.zoom = 1;
    ui->entity_edit.view_x = 0;
    ui->entity_edit.view_y = 0;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
    ui_text_blur(ui);
}

void entity_edit_open(UiState *ui, int type_idx) {
    R01World *w;
    if (!ui) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || type_idx < 0 || type_idx >= w->entity_count) {
        return;
    }
    memset(&ui->entity_edit, 0, sizeof(ui->entity_edit));
    ui->entity_edit.open = 1;
    ui->entity_edit.is_new = 0;
    ui->entity_edit.type_idx = type_idx;
    ui->entity_edit.draft = w->entities[type_idx];
    ui->entity_edit.state = 0;
    ui->entity_edit.frame = 0;
    ui->entity_edit.sel_part = -1;
    ui->entity_edit.paint_color = 1;
    ui->entity_edit.paint_pal = 0;
    ui->entity_edit.show_guides = 1;
    ui->entity_edit.meta_scroll = 0;
    ui->entity_edit.zoom = 1;
    ui->entity_edit.view_x = 0;
    ui->entity_edit.view_y = 0;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
    ui_text_blur(ui);
}

static void entity_edit_save(UiState *ui) {
    R01World *w = r01_project_active_world(ui->project);
    int idx;
    if (!w) {
        return;
    }
    if (ui->entity_edit.draft.state_count < 1) {
        ui->entity_edit.draft.state_count = 1;
    }
    if (ui->entity_edit.is_new || ui->entity_edit.type_idx < 0) {
        idx = r01_world_entity_add(w);
        if (idx < 0) {
            ui_toast(ui, "entity catalog full", 1);
            return;
        }
        w->entities[idx] = ui->entity_edit.draft;
        ui->entity_edit.type_idx = idx;
        ui->entity_edit.is_new = 0;
        ui_toast(ui, "entity created", 0);
    } else {
        if (ui->entity_edit.type_idx >= w->entity_count) {
            ui_toast(ui, "bad entity index", 1);
            return;
        }
        w->entities[ui->entity_edit.type_idx] = ui->entity_edit.draft;
        ui_toast(ui, "entity saved", 0);
    }
    ui->entity_edit.open = 0;
    ui_focus_clear(ui);
    ui_text_blur(ui);
}

void entity_modal_zoom_wheel(UiState *ui, int lx, int ly, int wheel_y) {
    EntityModalLayout lo;
    int z, nz, sc, nsc, wx, wy;
    if (!ui || !ui->entity_edit.open || wheel_y == 0) {
        return;
    }
    entity_modal_layout(ui, &lo);
    z = entity_zoom(ui);
    sc = entity_scale(z);
    /* Prefer cursor as zoom anchor when over the workbench; else viewport center. */
    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        wx = ui->entity_edit.view_x + (lx - lo.right_grid_x) / sc;
        wy = ui->entity_edit.view_y + (ly - lo.right_grid_y) / sc;
    } else {
        wx = ui->entity_edit.view_x + entity_view_cells(z) / 2;
        wy = ui->entity_edit.view_y + entity_view_cells(z) / 2;
        lx = lo.right_grid_x + UI_ENTITY_COMPOSE / 2;
        ly = lo.right_grid_y + UI_ENTITY_COMPOSE / 2;
    }
    if (wheel_y > 0) {
        nz = (z == 1) ? 2 : 4;
    } else {
        nz = (z == 4) ? 2 : 1;
    }
    if (nz == z) {
        return;
    }
    ui->entity_edit.zoom = nz;
    nsc = entity_scale(nz);
    ui->entity_edit.view_x = wx - (lx - lo.right_grid_x) / nsc;
    ui->entity_edit.view_y = wy - (ly - lo.right_grid_y) / nsc;
    entity_clamp_view(ui);
}

static void entity_meta_list_scroll(UiState *ui, int wheel_y) {
    EntityModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    int vis;
    int max_scroll = 0;
    entity_modal_layout(ui, &lo);
    vis = lo.left_list_h / UI_SPRITE_ROW_H;
    if (w && w->metasprite_count > vis) {
        max_scroll = w->metasprite_count - vis;
    }
    ui->entity_edit.meta_scroll -= wheel_y;
    if (ui->entity_edit.meta_scroll < 0) {
        ui->entity_edit.meta_scroll = 0;
    }
    if (ui->entity_edit.meta_scroll > max_scroll) {
        ui->entity_edit.meta_scroll = max_scroll;
    }
}

int entity_modal_wheel(UiState *ui, int lx, int ly, int wheel_y, int shift) {
    EntityModalLayout lo;
    const R01World *w;
    int row;
    int focus;
    if (!ui || !ui->entity_edit.open || wheel_y == 0) {
        return 0;
    }
    entity_modal_layout(ui, &lo);
    w = r01_project_active_world_const(ui->project);
    row = w ? w->default_pal_row : 0;

    /* Hovering a wheelable control focuses it, then dispatches. */
    if (point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        entity_modal_zoom_wheel(ui, lx, ly, wheel_y);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.left_list_x, lo.left_list_y, UI_ENTITY_BANK_GRID, lo.left_list_h)) {
        ui_focus_set(ui, UI_FOCUS_LIST);
        entity_meta_list_scroll(ui, wheel_y);
        return 1;
    }
    if (point_in_rect(lx, ly, lo.pal_x, lo.pal_y, UI_PAL_GRID_SIZE, UI_PAL_GRID_SIZE)) {
        ui_focus_set(ui, UI_FOCUS_PALETTE);
        ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->entity_edit.paint_pal,
                              ui->entity_edit.paint_color, wheel_y, shift);
        return 1;
    }

    focus = ui_focus_get(ui);
    if (focus == UI_FOCUS_WORKBENCH) {
        entity_modal_zoom_wheel(ui, lx, ly, wheel_y);
        return 1;
    }
    if (focus == UI_FOCUS_LIST) {
        entity_meta_list_scroll(ui, wheel_y);
        return 1;
    }
    if (focus == UI_FOCUS_PALETTE) {
        ui_palette_grid_nudge(ui->project, row, UI_PAL_PLANE_SPR, ui->entity_edit.paint_pal,
                              ui->entity_edit.paint_color, wheel_y, shift);
        return 1;
    }
    return 0;
}

void draw_entity_modal(UiState *ui, SDL_Renderer *r) {
    EntityModalLayout lo;
    const R01World *w = r01_project_active_world_const(ui->project);
    R01EntityState *st;
    R01EntityFrame *fr;
    int i;
    int row = w ? w->default_pal_row : 0;
    int z = entity_zoom(ui);
    int sc = entity_scale(z);
    int vx = ui->entity_edit.view_x;
    int vy = ui->entity_edit.view_y;
    int draw_ox;
    int draw_oy;
    UiClipStack clip;
    const char *title = ui->entity_edit.is_new ? "Add entity" : "Edit entity";

    entity_clamp_view(ui);
    vx = ui->entity_edit.view_x;
    vy = ui->entity_edit.view_y;
    entity_modal_layout(ui, &lo);
    ui_modal_scrim(r, ui);
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh, title);

    {
        const char *ename = ui->entity_edit.draft.name[0] ? ui->entity_edit.draft.name : "Entity";
        font_draw(r, lo.left_list_x, lo.left_name_y + 4, "Name", 230, 230, 230);
        ui_text_draw(ui, r, lo.left_name_x, lo.left_name_y, lo.left_name_w, ename, 1);
    }

    font_draw(r, lo.left_list_x, lo.left_label_y + 4, "Metasprites", 230, 230, 230);
    fill_rect(r, lo.left_list_x, lo.left_list_y, UI_ENTITY_BANK_GRID, lo.left_list_h, UI_COL_WELL_R,
              UI_COL_WELL_G, UI_COL_WELL_B);
    if (!w || w->metasprite_count < 1) {
        font_draw_centered(r, lo.left_list_x, lo.left_list_y, UI_ENTITY_BANK_GRID, lo.left_list_h, "empty", 160,
                           160, 170);
    } else {
        int vis = modal_meta_rows(&lo);
        int max_scroll = w->metasprite_count - vis;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (ui->entity_edit.meta_scroll > max_scroll) {
            ui->entity_edit.meta_scroll = max_scroll;
        }
        for (i = 0; i < vis; i++) {
            int idx = ui->entity_edit.meta_scroll + i;
            int y = lo.left_list_y + i * UI_SPRITE_ROW_H;
            const char *label;
            if (idx >= w->metasprite_count) {
                break;
            }
            if (point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_list_x, y, UI_ENTITY_BANK_GRID, UI_SPRITE_ROW_H)) {
                fill_rect(r, lo.left_list_x, y, UI_ENTITY_BANK_GRID, UI_SPRITE_ROW_H, UI_COL_PANEL_R,
                          UI_COL_PANEL_G, UI_COL_PANEL_B);
            }
            draw_meta_icon(ui, r, &w->metasprites[idx], lo.left_list_x,
                           y + (UI_SPRITE_ROW_H - UI_PREVIEW_ICON) / 2);
            label = r01_metasprite_display_name(&w->metasprites[idx]);
            font_draw_clipped(r, lo.left_list_x + UI_PREVIEW_ICON + 2, y + 4,
                              lo.left_list_x + UI_PREVIEW_ICON + 2, y,
                              UI_ENTITY_BANK_GRID - UI_PREVIEW_ICON - 2, UI_SPRITE_ROW_H, label, 230, 230, 230);
        }
    }

    ui_palette_grid_draw(r, ui->project, row, lo.pal_x, lo.pal_y, ui->entity_edit.paint_pal,
                         ui->entity_edit.paint_color, UI_PAL_PLANE_SPR);

    font_draw(r, lo.right_grid_x, lo.right_state_y + 4, "State", 230, 230, 230);
    ui_dot_strip_draw(r, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N, ui->entity_edit.state,
                      state_unlock_count(ui));
    {
        R01EntityState *s0 = edit_state(ui);
        const char *sname = (s0 && s0->name[0]) ? s0->name : "Idle";
        font_draw(r, lo.right_grid_x, lo.right_state_name_y + 4, "State name", 230, 230, 230);
        ui_text_draw(ui, r, lo.right_name_x, lo.right_name_y, lo.right_name_w, sname, 2);
    }

    font_draw(r, lo.right_grid_x, lo.right_frame_y + 4, "Frame", 230, 230, 230);
    ui_dot_strip_draw(r, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, ui->entity_edit.frame,
                      frame_unlock_count(ui));
    {
        char fid[R01_ID_MAX];
        int wi = ui->project ? ui->project->active_world : 0;
        r01_entity_frame_id(fid, sizeof(fid), wi, &ui->entity_edit.draft, ui->entity_edit.state,
                            ui->entity_edit.frame);
        font_draw_clipped(r, lo.right_grid_x, lo.right_id_y + 4, lo.right_grid_x, lo.right_id_y,
                          lo.mx + lo.mw - UI_UNIT * 2 - lo.right_grid_x, UI_BTN_H, fid, 160, 160, 170);
    }

    draw_ox = lo.right_grid_x - vx * sc;
    draw_oy = lo.right_grid_y - vy * sc;
    ui_compose_draw_grid(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, sc);
    st = edit_state(ui);
    fr = edit_frame(ui);
    ui_clip_push(r, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE, &clip);
    ui_compose_draw_frame(r, ui->project, w, fr, draw_ox, draw_oy, sc, ui->entity_edit.sel_part);
    if (st && ui->entity_edit.show_guides) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 220, 40, 40, 90);
        {
            SDL_Rect hb = {draw_ox + st->hitbox_x * sc, draw_oy + st->hitbox_y * sc, st->hitbox_w * sc,
                           st->hitbox_h * sc};
            SDL_RenderFillRect(r, &hb);
        }
        draw_ui_cross(r, draw_ox + st->origin_x * sc, draw_oy + st->origin_y * sc);
    }
    ui_clip_pop(r, &clip);

    if (ui->entity_edit.dragging == 6 && w) {
        const R01MetaspriteDef *ms = r01_world_metasprite_const(w, ui->entity_edit.drag_meta);
        if (ms && ms->frame.part_count > 0) {
            int min_dx = ms->frame.parts[0].dx;
            int min_dy = ms->frame.parts[0].dy;
            int gx = ui->mouse_x - ui->entity_edit.drag_off_x;
            int gy = ui->mouse_y - ui->entity_edit.drag_off_y;
            int pi;
            for (pi = 1; pi < ms->frame.part_count; pi++) {
                if (ms->frame.parts[pi].dx < min_dx) {
                    min_dx = ms->frame.parts[pi].dx;
                }
                if (ms->frame.parts[pi].dy < min_dy) {
                    min_dy = ms->frame.parts[pi].dy;
                }
            }
            for (i = 0; i < ms->frame.part_count; i++) {
                const R01EntityPart *pt = &ms->frame.parts[i];
                R01EntityPart ghost = *pt;
                ghost.dx = pt->dx - min_dx;
                ghost.dy = pt->dy - min_dy;
                ui_compose_draw_part(r, ui->project, w, &ghost, gx, gy, 1, 0);
            }
        }
    }

    ui_checkbox_draw(r, lo.guides_x, lo.guides_y + 4, ui->entity_edit.show_guides);
    font_draw(r, lo.guides_x + UI_CHECKBOX + UI_MODE_GAP, lo.guides_y + 4, "Origin/hitbox", 230, 230, 230);
    font_draw_wrapped(r, lo.help_x, lo.help_y + 2, lo.help_w, "Ctrl + click = drag zoomed viewport", 160, 160,
                      170);

    if (ui->entity_edit.dragging == 5) {
        draw_brush_preview(r, ui->project, row, ui->entity_edit.paint_pal, ui->entity_edit.paint_color, ui->mouse_x,
                           ui->mouse_y);
    }

    ui_modal_save_cancel(r, lo.left_list_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
}

int entity_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    const R01World *w;
    int idx, pal, col;
    int right = (button == SDL_BUTTON_RIGHT);
    int ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
    int z, sc, cx, cy;

    entity_modal_layout(ui, &lo);
    st = edit_state(ui);
    fr = edit_frame(ui);
    w = r01_project_active_world_const(ui->project);
    z = entity_zoom(ui);
    sc = entity_scale(z);

    if (!down) {
        if (!right && ui->entity_edit.dragging == 6 && fr && w &&
            point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
            int drop_x = ui->mouse_x - ui->entity_edit.drag_off_x;
            int drop_y = ui->mouse_y - ui->entity_edit.drag_off_y;
            entity_screen_to_world(ui, &lo, drop_x, drop_y, &cx, &cy);
            {
                const R01MetaspriteDef *ms = r01_world_metasprite_const(w, ui->entity_edit.drag_meta);
                if (ms) {
                    if (r01_entity_frame_add_metasprite(fr, ms, cx, cy) != 0) {
                        ui_toast(ui, "frame part limit", 1);
                    }
                }
            }
        }
        ui->entity_edit.dragging = 0;
        ui_text_mouse_up(ui);
        return 1;
    }

    if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(ui);
        return 1;
    }

    if (point_in_rect(lx, ly, lo.guides_x, lo.guides_y, UI_UNIT * 16, UI_BTN_H)) {
        ui_text_blur(ui);
        ui->entity_edit.show_guides = !ui->entity_edit.show_guides;
        return 1;
    }
    if (ui_palette_grid_hit(lx, ly, lo.pal_x, lo.pal_y, &pal, &col)) {
        ui_text_blur(ui);
        ui_focus_set(ui, UI_FOCUS_PALETTE);
        ui->entity_edit.paint_pal = pal;
        ui->entity_edit.paint_color = col;
        return 1;
    }

    if (ui_text_mouse_down(ui, lx, ly, lo.left_name_x, lo.left_name_y, lo.left_name_w, ui->entity_edit.draft.name,
                           R01_ENTITY_NAME_MAX, 1)) {
        return 1;
    }
    if (st && ui_text_mouse_down(ui, lx, ly, lo.right_name_x, lo.right_name_y, lo.right_name_w, st->name,
                                 R01_ENTITY_NAME_MAX, 2)) {
        return 1;
    }
    ui_text_blur(ui);

    if (ui_dot_strip_hit(lx, ly, lo.right_dots_x, lo.right_dots_y, UI_DOT_STRIP_N, &idx)) {
        int unlock = state_unlock_count(ui);
        if (idx < unlock) {
            if (!r01_entity_ensure_state(&ui->entity_edit.draft, idx)) {
                return 1;
            }
            ui->entity_edit.state = idx;
            ui->entity_edit.frame = 0;
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }
    if (ui_dot_strip_hit(lx, ly, lo.frame_dots_x, lo.frame_dots_y, UI_DOT_STRIP_N, &idx)) {
        int unlock = frame_unlock_count(ui);
        if (idx < unlock) {
            ui->entity_edit.frame = idx;
            (void)r01_entity_ensure_frame(&ui->entity_edit.draft, ui->entity_edit.state, idx);
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }

    if (ui_modal_save_hit(lx, ly, lo.left_list_x, lo.btn_y, lo.save_w)) {
        entity_edit_save(ui);
        return 1;
    }
    if (ui_modal_cancel_hit(lx, ly, lo.left_list_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
        ui->entity_edit.open = 0;
        ui_focus_clear(ui);
        ui_text_blur(ui);
        return 1;
    }

    if (!right && modal_meta_list_hit(ui, lx, ly, &idx)) {
        ui_focus_set(ui, UI_FOCUS_LIST);
        ui->entity_edit.dragging = 6;
        ui->entity_edit.drag_meta = idx;
        ui->entity_edit.drag_off_x = 4;
        ui->entity_edit.drag_off_y = 4;
        return 1;
    }

    if (st && point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        ui_focus_set(ui, UI_FOCUS_WORKBENCH);
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        if (!right && ctrl && z > 1) {
            ui->entity_edit.dragging = 7;
            ui->entity_edit.drag_off_x = lx;
            ui->entity_edit.drag_off_y = ly;
            ui->entity_edit.pan_x0 = ui->entity_edit.view_x;
            ui->entity_edit.pan_y0 = ui->entity_edit.view_y;
            return 1;
        }
        if (right) {
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (idx >= 0) {
                if (idx != ui->entity_edit.sel_part) {
                    ui->entity_edit.sel_part = idx;
                    return 1;
                }
                {
                    R01World *ww = r01_project_active_world(ui->project);
                    if (ww && fr) {
                        ui_compose_paint_part(ui->project, ww, &fr->parts[idx], cx, cy,
                                              ui->entity_edit.paint_color);
                    }
                }
                ui->entity_edit.dragging = 5;
            } else {
                ui->entity_edit.sel_part = -1;
            }
        } else if (ui->entity_edit.show_guides) {
            int ox = st->origin_x;
            int oy = st->origin_y;
            if (cx >= ox - 1 && cx <= ox + 1 && cy >= oy - 1 && cy <= oy + 1) {
                ui->entity_edit.dragging = 3;
                ui->entity_edit.drag_off_x = cx - ox;
                ui->entity_edit.drag_off_y = cy - oy;
                return 1;
            }
            if (cx >= st->hitbox_x && cx < st->hitbox_x + st->hitbox_w && cy >= st->hitbox_y &&
                cy < st->hitbox_y + st->hitbox_h) {
                ui->entity_edit.dragging = 2;
                ui->entity_edit.drag_off_x = cx - st->hitbox_x;
                ui->entity_edit.drag_off_y = cy - st->hitbox_y;
                return 1;
            }
        }
        if (fr) {
            idx = ui_compose_part_at(fr, cx, cy, ui->entity_edit.sel_part);
            if (idx >= 0) {
                ui->entity_edit.sel_part = idx;
                ui->entity_edit.dragging = 1;
                ui->entity_edit.drag_off_x = cx - fr->parts[idx].dx;
                ui->entity_edit.drag_off_y = cy - fr->parts[idx].dy;
                return 1;
            }
            ui->entity_edit.sel_part = -1;
        }
        return 1;
    }
    (void)sc;
    return 1;
}

void entity_modal_drag(UiState *ui, int lx, int ly, Uint32 buttons) {
    EntityModalLayout lo;
    R01EntityState *st;
    R01EntityFrame *fr;
    int z, sc, cx, cy;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    entity_modal_layout(ui, &lo);
    z = entity_zoom(ui);
    sc = entity_scale(z);
    if (ui->text.drag && ui->text.field_id == 1) {
        ui_text_mouse_drag(ui, lx, lo.left_name_x, lo.left_name_w);
        return;
    }
    if (ui->text.drag && ui->text.field_id == 2) {
        ui_text_mouse_drag(ui, lx, lo.right_name_x, lo.right_name_w);
        return;
    }
    if (!ui->entity_edit.dragging) {
        return;
    }
    st = edit_state(ui);
    fr = edit_frame(ui);
    if (ui->entity_edit.dragging == 7 && (buttons & SDL_BUTTON_LMASK)) {
        ui->entity_edit.view_x = ui->entity_edit.pan_x0 - (lx - ui->entity_edit.drag_off_x) / sc;
        ui->entity_edit.view_y = ui->entity_edit.pan_y0 - (ly - ui->entity_edit.drag_off_y) / sc;
        entity_clamp_view(ui);
        return;
    }
    if (ui->entity_edit.dragging == 5 && (buttons & SDL_BUTTON_RMASK) &&
        point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        {
            R01World *ww = r01_project_active_world(ui->project);
            if (ww && fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
                ui_compose_paint_part(ui->project, ww, &fr->parts[ui->entity_edit.sel_part], cx, cy,
                                      ui->entity_edit.paint_color);
            }
        }
    } else if (ui->entity_edit.dragging == 1 && fr && ui->entity_edit.sel_part >= 0 &&
               ui->entity_edit.sel_part < fr->part_count &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        fr->parts[ui->entity_edit.sel_part].dx = ui_compose_clamp_part(cx - ui->entity_edit.drag_off_x);
        fr->parts[ui->entity_edit.sel_part].dy = ui_compose_clamp_part(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 2 && st && ui->entity_edit.show_guides &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        st->hitbox_x = ui_compose_clamp_part(cx - ui->entity_edit.drag_off_x);
        st->hitbox_y = ui_compose_clamp_part(cy - ui->entity_edit.drag_off_y);
    } else if (ui->entity_edit.dragging == 3 && st && ui->entity_edit.show_guides &&
               point_in_rect(lx, ly, lo.right_grid_x, lo.right_grid_y, UI_ENTITY_COMPOSE, UI_ENTITY_COMPOSE)) {
        entity_screen_to_world(ui, &lo, lx, ly, &cx, &cy);
        st->origin_x = ui_compose_clamp_origin(cx - ui->entity_edit.drag_off_x);
        st->origin_y = ui_compose_clamp_origin(cy - ui->entity_edit.drag_off_y);
    }
}

void entity_modal_key(UiState *ui, SDL_Keycode sym) {
    R01EntityFrame *fr;
    R01EntityPart *pt;
    R01EntityState *st;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    if (ui->text.field_id > 0) {
        ui_text_key(ui, sym, SDL_GetModState());
        return;
    }
    st = edit_state(ui);
    if (sym >= SDLK_1 && sym <= SDLK_4) {
        ui->entity_edit.paint_color = (int)(sym - SDLK_1);
        return;
    }
    fr = edit_frame(ui);
    if (fr && ui->entity_edit.sel_part >= 0 && ui->entity_edit.sel_part < fr->part_count) {
        pt = &fr->parts[ui->entity_edit.sel_part];
        if (sym == SDLK_h) {
            pt->flip_h = !pt->flip_h;
            return;
        }
        if (sym == SDLK_v) {
            pt->flip_v = !pt->flip_v;
            return;
        }
        if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
            r01_entity_frame_remove_part(fr, ui->entity_edit.sel_part);
            ui->entity_edit.sel_part = -1;
            return;
        }
        return;
    }
    if (sym == SDLK_DELETE || sym == SDLK_BACKSPACE) {
        R01EntityType *e = &ui->entity_edit.draft;
        if (st && ui->entity_edit.frame == st->frame_count - 1 && st->frame_count > 1) {
            if (r01_entity_trim_last_frame(e, ui->entity_edit.state)) {
                ui->entity_edit.frame = st->frame_count - 1;
                if (ui->entity_edit.frame < 0) {
                    ui->entity_edit.frame = 0;
                }
                return;
            }
        }
        if (ui->entity_edit.state == e->state_count - 1 && e->state_count > 1) {
            if (r01_entity_trim_last_state(e)) {
                ui->entity_edit.state = e->state_count - 1;
                ui->entity_edit.frame = 0;
                return;
            }
        }
    }
}
