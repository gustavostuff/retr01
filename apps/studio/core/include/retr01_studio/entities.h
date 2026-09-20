#ifndef retr01_STUDIO_ENTITIES_H
#define retr01_STUDIO_ENTITIES_H

#include "retr01_studio/types.h"

#include <stddef.h>

void r01_entity_state_init(R01EntityState *st, const char *name);
void r01_entity_type_init(R01EntityType *e);

/* Slugify label for ids: lowercase [a-z0-9_], empty -> "unnamed". */
void r01_id_slugify(char *dst, size_t cap, const char *src);

/* Derived authoring ids (not stored). world_idx is 0-based; formatted as w_01.. */
void r01_entity_type_id(char *dst, size_t cap, int world_idx, const R01EntityType *e);
void r01_entity_state_id(char *dst, size_t cap, int world_idx, const R01EntityType *e, int state_idx);
void r01_entity_frame_id(char *dst, size_t cap, int world_idx, const R01EntityType *e, int state_idx,
                         int frame_idx);
const char *r01_entity_display_name(const R01EntityType *e);

/* Append a new entity type (1 state / 1 empty frame). Returns index or -1. */
int r01_world_entity_add(R01Project *p);

int r01_world_entity_remove(R01Project *p, int type_idx);

/* Mark which entity type is the Play player (-1 = CHR stub). Flag only. */
void r01_world_set_player_entity(R01Project *p, int type_idx);
int r01_world_player_entity(const R01Project *p);

/* Set player mark. Flag only. w is unused. */
int r01_project_set_player_entity(R01Project *p, R01World *w, int type_idx);

R01EntityType *r01_world_entity(R01Project *p, int type_idx);
const R01EntityType *r01_world_entity_const(const R01Project *p, int type_idx);

R01EntityState *r01_entity_state(R01EntityType *e, int state_idx);
R01EntityFrame *r01_entity_frame(R01EntityType *e, int state_idx, int frame_idx);

/* Default name for state index 0..3 (Idle/Walk/Hurt/Jump). */
const char *r01_entity_default_state_name(int state_idx);

/* Ensure state exists (extends state_count). Returns pointer or NULL. */
R01EntityState *r01_entity_ensure_state(R01EntityType *e, int state_idx);

/* First state named crouch/crouching, or -1. */
int r01_entity_crouch_state_index(const R01EntityType *e);

/* Ensure frame exists (extends frame_count). Returns pointer or NULL. */
R01EntityFrame *r01_entity_ensure_frame(R01EntityType *e, int state_idx, int frame_idx);

/*
 * Drop last state/frame when it is empty. Never removes index 0.
 * Returns 1 if trimmed, 0 otherwise.
 */
int r01_entity_trim_last_state(R01EntityType *e);
int r01_entity_trim_last_frame(R01EntityType *e, int state_idx);

int r01_entity_frame_add_part(R01EntityFrame *fr, const R01EntityPart *part);
int r01_entity_frame_remove_part(R01EntityFrame *fr, int part_idx);
/* Move part to end of frame (top of draw/hit z-order). Returns new index or -1. */
int r01_entity_frame_bring_part_front(R01EntityFrame *fr, int part_idx);
/*
 * Recompute one frame's origin from its parts. Empty: origin at 0,0.
 * Else origin = AABB center. Hitbox lives on the state.
 */
void r01_entity_frame_recompute_guides(R01EntityFrame *fr);
/* Clamp the state's compose-space hitbox into the 32x32 grid. */
void r01_entity_state_clamp_hitbox(R01EntityState *st);
/* Recompute every frame origin in the state, then clamp the state hitbox. */
void r01_entity_state_recompute_guides(R01EntityState *st);

/* Simple 1-state / 1-frame / 1-part entity from a sprite catalog entry (for phase C). */
int r01_world_entity_from_sprite(R01Project *p, int sprite_catalog_idx);

/* Placed instances (world pixels). */
int r01_world_instance_add(R01World *w, int type_id, int world_x, int world_y);
int r01_world_instance_remove(R01World *w, int inst_idx);
R01EntityInstance *r01_world_instance(R01World *w, int inst_idx);

/* Drop a catalog sprite: create entity type + place instance. Returns instance idx or -1. */
int r01_world_place_sprite(R01Project *p, R01World *w, int sprite_catalog_idx, int world_x, int world_y);

/* Drop an entity type: place instance. Returns instance idx or -1. */
int r01_world_place_entity(R01World *w, int type_id, int world_x, int world_y);

/* Resolve part draw pose for an instance (optional mirrors around frame origin). */
void r01_entity_part_instance_pose(const R01EntityFrame *fr, const R01EntityPart *pt, int inst_flip_h,
                                   int inst_flip_v, int *out_dx, int *out_dy, int *out_flip_h, int *out_flip_v);

/* Frames with at least one sprite part (empty Studio slots are skipped for animation). */
int r01_entity_state_drawable_frame_count(const R01EntityState *st);
int r01_entity_state_drawable_frame_index(const R01EntityState *st, int slot);

/*
 * Instance world_x/y is the author frame origin in world pixels.
 * Part authoring coords are relative to the 32x32 compose grid.
 * Convert with (coord - origin) before adding to world. Hitbox is the
 * state's compose AABB, origin-relative via the state's first drawable frame.
 */
static inline const R01EntityFrame *r01_entity_state_hitbox_origin_frame(const R01EntityState *st) {
    int fi;
    if (!st || st->frame_count < 1) {
        return NULL;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        if (st->frames[fi].part_count > 0) {
            return &st->frames[fi];
        }
    }
    return &st->frames[0];
}

static inline int r01_entity_world_x(int world_x, int origin_x, int ax) {
    return world_x + ax - origin_x;
}
static inline int r01_entity_world_y(int world_y, int origin_y, int ay) {
    return world_y + ay - origin_y;
}

#endif
