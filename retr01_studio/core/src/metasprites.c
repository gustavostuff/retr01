#include "retr01_studio/metasprites.h"

#include <string.h>

void r01_metasprite_init(R01MetaspriteDef *ms, const char *name) {
    if (!ms) {
        return;
    }
    memset(ms, 0, sizeof(*ms));
    if (name && name[0]) {
        strncpy(ms->name, name, R01_ENTITY_NAME_MAX - 1);
    } else {
        strncpy(ms->name, "Meta", R01_ENTITY_NAME_MAX - 1);
    }
}

int r01_world_metasprite_add(R01World *w) {
    R01MetaspriteDef *ms;
    if (!w || w->metasprite_count >= R01_MAX_METASPRITES) {
        return -1;
    }
    ms = &w->metasprites[w->metasprite_count];
    r01_metasprite_init(ms, "Meta");
    w->metasprite_count++;
    return w->metasprite_count - 1;
}

int r01_world_metasprite_remove(R01World *w, int idx) {
    int i;
    if (!w || idx < 0 || idx >= w->metasprite_count) {
        return -1;
    }
    for (i = idx; i < w->metasprite_count - 1; i++) {
        w->metasprites[i] = w->metasprites[i + 1];
    }
    w->metasprite_count--;
    memset(&w->metasprites[w->metasprite_count], 0, sizeof(w->metasprites[0]));
    return 0;
}

R01MetaspriteDef *r01_world_metasprite(R01World *w, int idx) {
    if (!w || idx < 0 || idx >= w->metasprite_count) {
        return NULL;
    }
    return &w->metasprites[idx];
}

const R01MetaspriteDef *r01_world_metasprite_const(const R01World *w, int idx) {
    if (!w || idx < 0 || idx >= w->metasprite_count) {
        return NULL;
    }
    return &w->metasprites[idx];
}

int r01_metasprite_add_part(R01MetaspriteDef *ms, const R01EntityPart *part) {
    if (!ms || !part) {
        return -1;
    }
    return r01_entity_frame_add_part(&ms->frame, part);
}

int r01_metasprite_remove_part(R01MetaspriteDef *ms, int part_idx) {
    if (!ms) {
        return -1;
    }
    return r01_entity_frame_remove_part(&ms->frame, part_idx);
}

int r01_entity_frame_add_metasprite(R01EntityFrame *fr, const R01MetaspriteDef *ms, int drop_cx, int drop_cy) {
    int i;
    int min_dx = 0;
    int min_dy = 0;
    if (!fr || !ms || ms->frame.part_count < 1) {
        return -1;
    }
    min_dx = ms->frame.parts[0].dx;
    min_dy = ms->frame.parts[0].dy;
    for (i = 1; i < ms->frame.part_count; i++) {
        if (ms->frame.parts[i].dx < min_dx) {
            min_dx = ms->frame.parts[i].dx;
        }
        if (ms->frame.parts[i].dy < min_dy) {
            min_dy = ms->frame.parts[i].dy;
        }
    }
    for (i = 0; i < ms->frame.part_count; i++) {
        R01EntityPart part = ms->frame.parts[i];
        part.dx = drop_cx + (part.dx - min_dx);
        part.dy = drop_cy + (part.dy - min_dy);
        if (part.dx < 0 || part.dy < 0 || part.dx > R01_ENTITY_COMPOSE_PX - 8 ||
            part.dy > R01_ENTITY_COMPOSE_PX - 8) {
            continue;
        }
        if (r01_entity_frame_add_part(fr, &part) < 0) {
            return -1;
        }
    }
    return 0;
}
