#include "retr01_studio/entities.h"

#include <stdio.h>
#include <string.h>

void r01_entity_state_init(R01EntityState *st, const char *name) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    if (name && name[0]) {
        strncpy(st->name, name, R01_ENTITY_NAME_MAX - 1);
    } else {
        strncpy(st->name, "State", R01_ENTITY_NAME_MAX - 1);
    }
    st->origin_x = 0;
    st->origin_y = 0;
    st->hitbox_x = 0;
    st->hitbox_y = 0;
    st->hitbox_w = R01_ENTITY_HITBOX_W;
    st->hitbox_h = R01_ENTITY_HITBOX_H;
    st->frame_count = 1;
}

void r01_entity_type_init(R01EntityType *e) {
    if (!e) {
        return;
    }
    memset(e, 0, sizeof(*e));
    e->present = 1;
    e->state_count = 1;
    r01_entity_state_init(&e->states[0], "Idle");
}

int r01_world_entity_add(R01World *w) {
    R01EntityType *e;
    if (!w || w->entity_count >= R01_MAX_ENTITY_TYPES) {
        return -1;
    }
    e = &w->entities[w->entity_count];
    r01_entity_type_init(e);
    w->entity_count++;
    return w->entity_count - 1;
}

int r01_world_entity_remove(R01World *w, int type_idx) {
    int i, j;
    if (!w || type_idx < 0 || type_idx >= w->entity_count) {
        return -1;
    }
    /* Drop instances of this type; remap higher type_ids. */
    for (i = 0; i < w->instance_count;) {
        if (w->instances[i].type_id == type_idx) {
            r01_world_instance_remove(w, i);
            continue;
        }
        if (w->instances[i].type_id > type_idx) {
            w->instances[i].type_id--;
        }
        i++;
    }
    for (j = type_idx; j < w->entity_count - 1; j++) {
        w->entities[j] = w->entities[j + 1];
    }
    w->entity_count--;
    memset(&w->entities[w->entity_count], 0, sizeof(w->entities[0]));
    return 0;
}

R01EntityType *r01_world_entity(R01World *w, int type_idx) {
    if (!w || type_idx < 0 || type_idx >= w->entity_count) {
        return NULL;
    }
    return &w->entities[type_idx];
}

const R01EntityType *r01_world_entity_const(const R01World *w, int type_idx) {
    if (!w || type_idx < 0 || type_idx >= w->entity_count) {
        return NULL;
    }
    return &w->entities[type_idx];
}

R01EntityState *r01_entity_state(R01EntityType *e, int state_idx) {
    if (!e || state_idx < 0 || state_idx >= e->state_count || state_idx >= R01_ENTITY_STATES_MAX) {
        return NULL;
    }
    return &e->states[state_idx];
}

R01EntityFrame *r01_entity_frame(R01EntityType *e, int state_idx, int frame_idx) {
    R01EntityState *st = r01_entity_state(e, state_idx);
    if (!st || frame_idx < 0 || frame_idx >= st->frame_count || frame_idx >= R01_ENTITY_FRAMES_MAX) {
        return NULL;
    }
    return &st->frames[frame_idx];
}

R01EntityFrame *r01_entity_ensure_frame(R01EntityType *e, int state_idx, int frame_idx) {
    R01EntityState *st = r01_entity_state(e, state_idx);
    if (!st || frame_idx < 0 || frame_idx >= R01_ENTITY_FRAMES_MAX) {
        return NULL;
    }
    while (st->frame_count <= frame_idx) {
        memset(&st->frames[st->frame_count], 0, sizeof(st->frames[0]));
        st->frame_count++;
    }
    return &st->frames[frame_idx];
}

int r01_entity_frame_add_part(R01EntityFrame *fr, const R01EntityPart *part) {
    if (!fr || !part || fr->part_count >= R01_ENTITY_PARTS_MAX) {
        return -1;
    }
    fr->parts[fr->part_count] = *part;
    fr->part_count++;
    return fr->part_count - 1;
}

int r01_entity_frame_remove_part(R01EntityFrame *fr, int part_idx) {
    int i;
    if (!fr || part_idx < 0 || part_idx >= fr->part_count) {
        return -1;
    }
    for (i = part_idx; i < fr->part_count - 1; i++) {
        fr->parts[i] = fr->parts[i + 1];
    }
    fr->part_count--;
    memset(&fr->parts[fr->part_count], 0, sizeof(fr->parts[0]));
    return 0;
}

int r01_world_entity_from_sprite(R01World *w, int sprite_catalog_idx) {
    R01EntityType *e;
    R01EntityFrame *fr;
    R01EntityPart part;
    const R01SpriteDef *sp;
    int idx;
    if (!w || sprite_catalog_idx < 0 || sprite_catalog_idx >= w->sprite_count) {
        return -1;
    }
    sp = &w->sprites[sprite_catalog_idx];
    idx = r01_world_entity_add(w);
    if (idx < 0) {
        return -1;
    }
    e = &w->entities[idx];
    fr = r01_entity_frame(e, 0, 0);
    if (!fr) {
        return -1;
    }
    memset(&part, 0, sizeof(part));
    part.bank = sp->bank;
    part.tile_id = sp->tile_id;
    part.pal = sp->pal;
    part.dx = 0;
    part.dy = 0;
    if (r01_entity_frame_add_part(fr, &part) < 0) {
        return -1;
    }
    e->states[0].hitbox_w = R01_ENTITY_HITBOX_W;
    e->states[0].hitbox_h = R01_ENTITY_HITBOX_H;
    return idx;
}

int r01_world_instance_add(R01World *w, int type_id, int world_x, int world_y) {
    R01EntityInstance *inst;
    if (!w || type_id < 0 || type_id >= w->entity_count || w->instance_count >= R01_MAX_ENTITY_INSTANCES) {
        return -1;
    }
    inst = &w->instances[w->instance_count];
    inst->type_id = type_id;
    inst->world_x = world_x;
    inst->world_y = world_y;
    w->instance_count++;
    return w->instance_count - 1;
}

int r01_world_instance_remove(R01World *w, int inst_idx) {
    int i;
    if (!w || inst_idx < 0 || inst_idx >= w->instance_count) {
        return -1;
    }
    for (i = inst_idx; i < w->instance_count - 1; i++) {
        w->instances[i] = w->instances[i + 1];
    }
    w->instance_count--;
    memset(&w->instances[w->instance_count], 0, sizeof(w->instances[0]));
    return 0;
}

R01EntityInstance *r01_world_instance(R01World *w, int inst_idx) {
    if (!w || inst_idx < 0 || inst_idx >= w->instance_count) {
        return NULL;
    }
    return &w->instances[inst_idx];
}

int r01_world_place_sprite(R01World *w, int sprite_catalog_idx, int world_x, int world_y) {
    int type_id = r01_world_entity_from_sprite(w, sprite_catalog_idx);
    if (type_id < 0) {
        return -1;
    }
    return r01_world_instance_add(w, type_id, world_x, world_y);
}

int r01_world_place_entity(R01World *w, int type_id, int world_x, int world_y) {
    return r01_world_instance_add(w, type_id, world_x, world_y);
}
