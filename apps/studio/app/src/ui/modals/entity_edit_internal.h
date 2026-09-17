#ifndef R01_STUDIO_ENTITY_EDIT_INTERNAL_H
#define R01_STUDIO_ENTITY_EDIT_INTERNAL_H

#include "ui/ui.h"
#include "ui/internal.h"

R01EntityState *entity_edit_state(UiState *ui);
R01EntityFrame *entity_edit_frame(UiState *ui);
int entity_edit_state_unlock_count(const UiState *ui);
int entity_edit_frame_unlock_count(UiState *ui);
int entity_edit_compose_scale(const UiState *ui);
void entity_edit_screen_to_world(const UiState *ui, const EntityModalLayout *lo, int lx, int ly, int *wx, int *wy);
void entity_edit_world_to_screen(const UiState *ui, const EntityModalLayout *lo, int wx, int wy, int *sx, int *sy);
int entity_edit_origin_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityFrame *fr, int lx, int ly);
int entity_edit_hitbox_corner_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityFrame *fr, int lx,
                                  int ly, int *out_corner);
int entity_edit_hitbox_body_hit(const UiState *ui, const EntityModalLayout *lo, const R01EntityFrame *fr, int lx,
                                int ly);
void entity_edit_view_clamp(UiState *ui);
void entity_edit_view_pan(UiState *ui, int dx, int dy);
void entity_edit_set_zoom(UiState *ui, const EntityModalLayout *lo, int new_zoom, int focus_lx, int focus_ly);
void entity_edit_recompute_guides(UiState *ui);
void entity_edit_apply_pal_to_part(UiState *ui, R01EntityPart *pt, int pal);
void entity_edit_select_part(UiState *ui, R01EntityFrame *fr, int idx);
void entity_edit_paint_at(UiState *ui, R01World *w, R01EntityFrame *fr, int idx, int cx, int cy);
int entity_edit_paste_clipboard(UiState *ui);
void entity_edit_save(UiState *ui);

#endif
