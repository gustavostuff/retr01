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

/* Ensure frame exists (extends frame_count). Returns pointer or NULL. */
R01EntityFrame *r01_entity_ensure_frame(R01EntityType *e, int state_idx, int frame_idx);

int r01_entity_frame_add_part(R01EntityFrame *fr, const R01EntityPart *part);
int r01_entity_frame_remove_part(R01EntityFrame *fr, int part_idx);

/* Simple 1-state / 1-frame / 1-part entity from a sprite catalog entry (for phase C). */
int r01_world_entity_from_sprite(R01World *w, int sprite_catalog_idx);

#endif
