#ifndef RETR01_STUDIO_METATILES_H
#define RETR01_STUDIO_METATILES_H

#include "retr01_studio/types.h"

#include <stddef.h>

void r01_metatile_init(R01MetatileDef *mt, const char *name);
void r01_metatile_id(char *dst, size_t cap, int world_idx, const R01MetatileDef *mt);
const char *r01_metatile_display_name(const R01MetatileDef *mt);

int r01_world_metatile_add(R01World *w);
int r01_world_metatile_remove(R01World *w, int idx);

R01MetatileDef *r01_world_metatile(R01World *w, int idx);
const R01MetatileDef *r01_world_metatile_const(const R01World *w, int idx);

#endif
