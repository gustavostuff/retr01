#include "retr01_studio/metatiles.h"

#include "retr01_studio/entities.h"

#include <stdio.h>
#include <string.h>

void r01_metatile_init(R01MetatileDef *mt, const char *name) {
    if (!mt) {
        return;
    }
    memset(mt, 0, sizeof(*mt));
    if (name && name[0]) {
        strncpy(mt->name, name, R01_ENTITY_NAME_MAX - 1);
    } else {
        strncpy(mt->name, "Metatile", R01_ENTITY_NAME_MAX - 1);
    }
}

const char *r01_metatile_display_name(const R01MetatileDef *mt) {
    if (!mt || !mt->name[0]) {
        return "metatile";
    }
    return mt->name;
}

void r01_metatile_id(char *dst, size_t cap, int world_idx, const R01MetatileDef *mt) {
    char slug[R01_ENTITY_NAME_MAX];
    if (!dst || cap < 1) {
        return;
    }
    r01_id_slugify(slug, sizeof(slug), r01_metatile_display_name(mt));
    if (world_idx < 0) {
        world_idx = 0;
    }
    snprintf(dst, cap, "w_%02d_%s", world_idx + 1, slug);
}

int r01_world_metatile_add(R01World *w) {
    R01MetatileDef *mt;
    if (!w || w->metatile_count >= R01_MAX_METATILES) {
        return -1;
    }
    mt = &w->metatiles[w->metatile_count];
    r01_metatile_init(mt, "Metatile");
    w->metatile_count++;
    return w->metatile_count - 1;
}

int r01_world_metatile_remove(R01World *w, int idx) {
    int i;
    if (!w || idx < 0 || idx >= w->metatile_count) {
        return -1;
    }
    for (i = idx; i < w->metatile_count - 1; i++) {
        w->metatiles[i] = w->metatiles[i + 1];
    }
    w->metatile_count--;
    memset(&w->metatiles[w->metatile_count], 0, sizeof(w->metatiles[0]));
    return 0;
}

R01MetatileDef *r01_world_metatile(R01World *w, int idx) {
    if (!w || idx < 0 || idx >= w->metatile_count) {
        return NULL;
    }
    return &w->metatiles[idx];
}

const R01MetatileDef *r01_world_metatile_const(const R01World *w, int idx) {
    if (!w || idx < 0 || idx >= w->metatile_count) {
        return NULL;
    }
    return &w->metatiles[idx];
}
