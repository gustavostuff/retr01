#include "ui/modals/entity_edit_internal.h"
#include "ui/undo/undo_cmds.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <string.h>

int entity_edit_add_sprite_at(UiState *ui, int wx, int wy) {
    R01World *w;
    R01EntityFrame *fr;
    R01EntityPart part;
    int bank;
    int tile_id;
    int idx;
    int cat;

    if (!ui || !ui->entity_edit.open) {
        return -1;
    }
    fr = entity_edit_frame(ui);
    if (!fr) {
        return -1;
    }
    if (fr->part_count >= R01_ENTITY_PARTS_MAX) {
        ui_toast(ui, "frame part limit (4)", 1);
        return -1;
    }
    w = r01_project_active_world(ui->project);
    if (!w) {
        return -1;
    }
    if (r01_world_player_entity(w) == ui->entity_edit.type_idx && ui->entity_edit.type_idx >= 0) {
        int gbank = ui->global_banks_idx;
        if (ui->global_banks_plane != UI_BANKS_PLANE_GLOBAL_SPR || gbank < 0 || gbank >= R01_SPR_BANKS) {
            gbank = 0;
        }
        bank = R01_GLOBAL_SPR_BANK_BASE + gbank;
        tile_id = r01_other_spr_alloc_tile(ui->project, gbank);
        if (tile_id < 0) {
            ui_toast(ui, "other SPR bank full", 1);
            return -1;
        }
    } else {
        bank = r01_chr_find_spr_bank_space(w);
        if (bank < 0) {
            ui_toast(ui, "sprite banks full", 1);
            return -1;
        }
        tile_id = r01_chr_alloc_spr_tile(w, bank);
        if (tile_id < 0) {
            ui_toast(ui, "sprite banks full", 1);
            return -1;
        }
    }
    cat = r01_world_sprite_add(w, bank, tile_id, ui->entity_edit.paint_pal);
    memset(&part, 0, sizeof(part));
    part.bank = bank;
    part.tile_id = tile_id;
    part.pal = ui->entity_edit.paint_pal & 3;
    part.dx = ui_compose_clamp_part(wx - 4);
    part.dy = ui_compose_clamp_part(wy - 4);
    idx = r01_entity_frame_add_part(fr, &part);
    if (idx < 0) {
        ui_toast(ui, "frame part limit (4)", 1);
        return -1;
    }
    entity_edit_select_part(ui, fr, idx);
    entity_edit_recompute_guides(ui);
    ui_undo_push_entity_part_add(ui, ui->entity_edit.state, ui->entity_edit.frame, idx, &part, cat);
    return idx;
}

int entity_edit_add_existing_sprite_at(UiState *ui, int wx, int wy, int catalog_idx) {
    R01World *w;
    R01EntityFrame *fr;
    R01EntityPart part;
    const R01SpriteDef *sp;
    int idx;

    if (!ui || !ui->entity_edit.open) {
        return -1;
    }
    fr = entity_edit_frame(ui);
    if (!fr) {
        return -1;
    }
    if (fr->part_count >= R01_ENTITY_PARTS_MAX) {
        ui_toast(ui, "frame part limit (4)", 1);
        return -1;
    }
    w = r01_project_active_world(ui->project);
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    sp = &w->sprites[catalog_idx];
    memset(&part, 0, sizeof(part));
    part.bank = sp->bank;
    part.tile_id = sp->tile_id;
    part.pal = sp->pal & 3;
    part.dx = ui_compose_clamp_part(wx - 4);
    part.dy = ui_compose_clamp_part(wy - 4);
    idx = r01_entity_frame_add_part(fr, &part);
    if (idx < 0) {
        ui_toast(ui, "frame part limit (4)", 1);
        return -1;
    }
    entity_edit_select_part(ui, fr, idx);
    entity_edit_recompute_guides(ui);
    ui_undo_push_entity_part_add(ui, ui->entity_edit.state, ui->entity_edit.frame, idx, &part, -1);
    return idx;
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
    ui->entity_edit.brush_size = UI_BRUSH_SIZE_MIN;
    ui->entity_edit.tool = UI_ENTITY_TOOL_SELECT;
    ui->entity_edit.show_part_outlines = 0;
    ui->entity_edit.zoom = UI_ENTITY_ZOOM_MIN;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
    ui_text_blur(&ui->text);
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
    ui->entity_edit.brush_size = UI_BRUSH_SIZE_MIN;
    ui->entity_edit.tool = UI_ENTITY_TOOL_SELECT;
    ui->entity_edit.show_part_outlines = 0;
    ui->entity_edit.zoom = UI_ENTITY_ZOOM_MIN;
    ui_focus_set(ui, UI_FOCUS_WORKBENCH);
    ui_text_blur(&ui->text);
}

static int entity_edit_next_preview_frame(const R01EntityState *st, int from) {
    int n;
    int i;
    if (!st || st->frame_count < 1) {
        return 0;
    }
    n = st->frame_count;
    if (from < 0) {
        from = 0;
    }
    for (i = 1; i <= n; i++) {
        int idx = (from + i) % n;
        if (st->frames[idx].part_count > 0) {
            return idx;
        }
    }
    return from % n;
}

void entity_edit_preview_set(UiState *ui, int playing) {
    if (!ui) {
        return;
    }
    if (playing && !entity_edit_preview_can_play(ui)) {
        playing = 0;
    }
    ui->entity_edit.preview_playing = playing ? 1 : 0;
    ui->entity_edit.preview_ctr = 0;
    ui->entity_edit.preview_last_ms = playing ? SDL_GetTicks() : 0;
}

int entity_edit_preview_can_play(const UiState *ui) {
    const R01EntityType *e;
    int si;
    if (!ui) {
        return 0;
    }
    e = &ui->entity_edit.draft;
    si = ui->entity_edit.state;
    if (si < 0 || si >= e->state_count) {
        return 0;
    }
    return r01_entity_state_drawable_frame_count(&e->states[si]) >= 2;
}

void entity_edit_preview_tick(UiState *ui) {
    R01EntityState *st;
    R01EntityFrame *fr;
    Uint32 now;
    int delay;
    if (!ui || !ui->entity_edit.open || !ui->entity_edit.preview_playing) {
        return;
    }
    if (!entity_edit_preview_can_play(ui)) {
        entity_edit_preview_set(ui, 0);
        return;
    }
    now = SDL_GetTicks();
    if (ui->entity_edit.preview_last_ms == 0) {
        ui->entity_edit.preview_last_ms = now;
        return;
    }
    if (now - ui->entity_edit.preview_last_ms < 16u) {
        return;
    }
    ui->entity_edit.preview_last_ms = now;
    st = entity_edit_state(ui);
    if (!st) {
        return;
    }
    fr = entity_edit_frame(ui);
    if (!fr || fr->part_count < 1) {
        int nxt = entity_edit_next_preview_frame(st, ui->entity_edit.frame);
        if (nxt != ui->entity_edit.frame) {
            ui->entity_edit.frame = nxt;
            entity_edit_clear_sel(ui);
            ui->entity_edit.preview_ctr = 0;
        }
        return;
    }
    delay = fr->delay < 1 ? 1 : fr->delay;
    ui->entity_edit.preview_ctr++;
    if (ui->entity_edit.preview_ctr < delay) {
        return;
    }
    ui->entity_edit.preview_ctr = 0;
    {
        int nxt = entity_edit_next_preview_frame(st, ui->entity_edit.frame);
        if (nxt != ui->entity_edit.frame) {
            ui->entity_edit.frame = nxt;
            entity_edit_clear_sel(ui);
        }
    }
}

void entity_edit_save(UiState *ui) {
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
        ui_undo_push_entity_add(ui, idx);
        ui_toast(ui, "entity created", 0);
    } else {
        if (ui->entity_edit.type_idx >= w->entity_count) {
            ui_toast(ui, "bad entity index", 1);
            return;
        }
        w->entities[ui->entity_edit.type_idx] = ui->entity_edit.draft;
        ui_toast(ui, "entity saved", 0);
    }
    ui_undo_spr_paint_end(ui);
    ui->entity_edit.open = 0;
    ui_focus_clear(ui);
    ui_text_blur(&ui->text);
}
