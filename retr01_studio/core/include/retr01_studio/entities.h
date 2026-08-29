#ifndef retr01_STUDIO_ENTITIES_H
#define retr01_STUDIO_ENTITIES_H

#include "retr01_studio/types.h"

void r01_entity_state_init(R01EntityState *st, const char *name);
void r01_entity_type_init(R01EntityType *e);

/* Append a new entity type (1 state / 1 empty frame). Returns index or -1. */
int r01_world_entity_add(R01World *w);

int r01_world_entity_remove(R01World *w, int type_idx);

R01EntityType *r01_world_entity(R01World *w, int type_idx);
const R01EntityType *r01_world_entity_const(const R01World *w, int type_idx);

R01EntityState *r01_entity_state(R01EntityType *e, int state_idx);
R01EntityFrame *r01_entity_frame(R01EntityType *e, int state_idx, int frame_idx);

/* Default name for state index 0..3 (Idle/Walk/Hurt/Jump). */
const char *r01_entity_default_state_name(int state_idx);

/* Ensure state exists (extends state_count). Returns pointer or NULL. */
R01EntityState *r01_entity_ensure_state(R01EntityType *e, int state_idx);

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

/* Simple 1-state / 1-frame / 1-part entity from a sprite catalog entry (for phase C). */
int r01_world_entity_from_sprite(R01World *w, int sprite_catalog_idx);

/* Placed instances (world pixels). */
int r01_world_instance_add(R01World *w, int type_id, int world_x, int world_y);
int r01_world_instance_remove(R01World *w, int inst_idx);
R01EntityInstance *r01_world_instance(R01World *w, int inst_idx);

/* Drop a catalog sprite: create entity type + place instance. Returns instance idx or -1. */
int r01_world_place_sprite(R01World *w, int sprite_catalog_idx, int world_x, int world_y);

/* Drop an entity type: place instance. Returns instance idx or -1. */
int r01_world_place_entity(R01World *w, int type_id, int world_x, int world_y);

/*
 * Instance world_x/y is the user-defined state origin in world pixels.
 * Part / hitbox authoring coords are relative to the 16x16 compose grid;
 * convert with (coord - origin) before adding to world.
 */
static inline int r01_entity_world_x(int world_x, int origin_x, int ax) {
    return world_x + ax - origin_x;
}
static inline int r01_entity_world_y(int world_y, int origin_y, int ay) {
    return world_y + ay - origin_y;
}

#endif
