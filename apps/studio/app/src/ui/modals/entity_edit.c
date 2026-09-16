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
        bank = R01_PLAYER_CHR_BANK;
        tile_id = r01_player_bank_alloc_tile(ui->project);
        if (tile_id < 0) {
            ui_toast(ui, "player bank full", 1);
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
    ui->entity_edit.sel_part = idx;
    entity_edit_recompute_guides(ui);
    ui_undo_push_entity_part_add(ui, ui->entity_edit.state, ui->entity_edit.frame, idx, &part, cat);
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
