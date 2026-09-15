#ifndef retr01_STUDIO_UI_UNDO_CMDS_H
#define retr01_STUDIO_UI_UNDO_CMDS_H

#include "retr01_studio/types.h"

struct UiState;

typedef struct UiUndoPaintCell {
    uint16_t cell;
    uint8_t old_tile;
    uint8_t old_attr;
    uint8_t new_tile;
    uint8_t new_attr;
} UiUndoPaintCell;

typedef struct UiUndoPaintStroke {
    int world_idx;
    int plane;
    int screen_idx;
    int col;
    int row;
    UiUndoPaintCell *cells;
    int count;
    int cap;
} UiUndoPaintStroke;

int ui_undo_paint_begin(struct UiState *ui);
void ui_undo_paint_end(struct UiState *ui);
void ui_undo_paint_record_cell(struct UiState *ui, int tx, int ty, uint8_t old_tile, uint8_t old_attr,
                               uint8_t new_tile, uint8_t new_attr);

void ui_undo_push_entity_add(struct UiState *ui, int type_idx);
void ui_undo_push_entity_remove(struct UiState *ui, int type_idx, const R01EntityType *removed, int was_player);
void ui_undo_push_sprite_add(struct UiState *ui, int catalog_idx);
void ui_undo_push_metasprite_add(struct UiState *ui, int meta_idx);
void ui_undo_push_metatile_add(struct UiState *ui, int mt_idx);
void ui_undo_push_instance_add(struct UiState *ui, int inst_idx);
void ui_undo_push_instance_remove(struct UiState *ui, int inst_idx, const R01EntityInstance *removed);

void ui_undo_push_screen_create(struct UiState *ui, int plane, int col, int row);
void ui_undo_push_screen_remove(struct UiState *ui, int plane, int col, int row, const R01Screen *removed);
void ui_undo_push_screen_paste(struct UiState *ui, int plane, int screen_idx, const R01Screen *before_or_null);

void ui_undo_push_tile_create(struct UiState *ui, int bank, int tile_id, int old_tile_count, int painted,
                              int paint_tx, int paint_ty, uint8_t old_tile, uint8_t old_attr, uint8_t new_tile,
                              uint8_t new_attr);

#endif
