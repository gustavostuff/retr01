#ifndef retr01_STUDIO_METASPRITES_H
#define retr01_STUDIO_METASPRITES_H

#include "retr01_studio/entities.h"
#include "retr01_studio/types.h"

void r01_metasprite_init(R01MetaspriteDef *ms, const char *name);

int r01_world_metasprite_add(R01World *w);
int r01_world_metasprite_remove(R01World *w, int idx);

R01MetaspriteDef *r01_world_metasprite(R01World *w, int idx);
const R01MetaspriteDef *r01_world_metasprite_const(const R01World *w, int idx);

int r01_metasprite_add_part(R01MetaspriteDef *ms, const R01EntityPart *part);
int r01_metasprite_remove_part(R01MetaspriteDef *ms, int part_idx);

/* Copy all parts into an entity frame at compose drop (cx,cy) = top-left anchor. */
int r01_entity_frame_add_metasprite(R01EntityFrame *fr, const R01MetaspriteDef *ms, int drop_cx, int drop_cy);

/* Create a 1-state entity type from a metasprite catalog entry. Returns type idx or -1. */
int r01_world_entity_from_metasprite(R01World *w, int meta_idx);

/* Drop a metasprite: create entity type + place instance. Returns instance idx or -1. */
int r01_world_place_metasprite(R01World *w, int meta_idx, int world_x, int world_y);

#endif
