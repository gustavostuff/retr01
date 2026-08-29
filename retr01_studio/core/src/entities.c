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
    strncpy(e->name, "Entity", R01_ENTITY_NAME_MAX - 1);
    e->state_count = 1;
    r01_entity_state_init(&e->states[0], "Idle");
}

void r01_id_slugify(char *dst, size_t cap, const char *src) {
    size_t di = 0;
    int prev_us = 1;
    if (!dst || cap < 2) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        strncpy(dst, "unnamed", cap - 1);
        dst[cap - 1] = '\0';
        return;
    }
    while (*src && di + 1 < cap) {
        unsigned char c = (unsigned char)*src++;
        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 'a');
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            dst[di++] = (char)c;
            prev_us = 0;
        } else if (!prev_us) {
            dst[di++] = '_';
            prev_us = 1;
        }
    }
    while (di > 0 && dst[di - 1] == '_') {
        di--;
    }
    if (di == 0) {
        strncpy(dst, "unnamed", cap - 1);
        dst[cap - 1] = '\0';
        return;
    }
    dst[di] = '\0';
}

const char *r01_entity_display_name(const R01EntityType *e) {
    if (!e) {
        return "entity";
    }
    if (e->name[0]) {
        return e->name;
    }
    if (e->state_count > 0 && e->states[0].name[0]) {
        return e->states[0].name;
    }
    return "entity";
}

void r01_entity_type_id(char *dst, size_t cap, int world_idx, const R01EntityType *e) {
    char slug[R01_ENTITY_NAME_MAX];
    if (!dst || cap < 1) {
        return;
    }
    r01_id_slugify(slug, sizeof(slug), r01_entity_display_name(e));
    if (world_idx < 0) {
        world_idx = 0;
    }
    /* IDs use 1-based world numbers to match the Worlds accordion labels. */
    snprintf(dst, cap, "w_%02d_%s", world_idx + 1, slug);
}

void r01_entity_state_id(char *dst, size_t cap, int world_idx, const R01EntityType *e, int state_idx) {
    char eslug[R01_ENTITY_NAME_MAX];
    char sslug[R01_ENTITY_NAME_MAX];
    const char *sname = "state";
    if (!dst || cap < 1) {
        return;
    }
    if (e && state_idx >= 0 && state_idx < e->state_count && e->states[state_idx].name[0]) {
        sname = e->states[state_idx].name;
    } else if (e) {
        sname = r01_entity_default_state_name(state_idx);
    }
    r01_id_slugify(eslug, sizeof(eslug), r01_entity_display_name(e));
    r01_id_slugify(sslug, sizeof(sslug), sname);
    if (world_idx < 0) {
        world_idx = 0;
    }
    snprintf(dst, cap, "w_%02d_%s_%s", world_idx + 1, eslug, sslug);
}

void r01_entity_frame_id(char *dst, size_t cap, int world_idx, const R01EntityType *e, int state_idx,
                         int frame_idx) {
    char base[R01_ID_MAX];
    if (!dst || cap < 1) {
        return;
    }
    r01_entity_state_id(base, sizeof(base), world_idx, e, state_idx);
    if (frame_idx < 0) {
        frame_idx = 0;
    }
    snprintf(dst, cap, "%s_frame_%02d", base, frame_idx);
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
    if (w->player_entity == type_idx) {
        w->player_entity = -1;
    } else if (w->player_entity > type_idx) {
        w->player_entity--;
    }
    for (j = type_idx; j < w->entity_count - 1; j++) {
        w->entities[j] = w->entities[j + 1];
    }
    w->entity_count--;
    memset(&w->entities[w->entity_count], 0, sizeof(w->entities[0]));
    return 0;
}

void r01_world_set_player_entity(R01World *w, int type_idx) {
    if (!w) {
        return;
    }
    if (type_idx < 0 || type_idx >= w->entity_count) {
        w->player_entity = -1;
        return;
    }
    w->player_entity = type_idx;
}

int r01_world_player_entity(const R01World *w) {
    if (!w || w->player_entity < 0 || w->player_entity >= w->entity_count) {
        return -1;
    }
    return w->player_entity;
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

const char *r01_entity_default_state_name(int state_idx) {
    static const char *names[R01_ENTITY_STATES_MAX] = {"Idle", "Walk", "Hurt", "Jump"};
    if (state_idx < 0 || state_idx >= R01_ENTITY_STATES_MAX) {
        return "State";
    }
    return names[state_idx];
}

R01EntityState *r01_entity_ensure_state(R01EntityType *e, int state_idx) {
    if (!e || state_idx < 0 || state_idx >= R01_ENTITY_STATES_MAX) {
        return NULL;
    }
    while (e->state_count <= state_idx) {
        r01_entity_state_init(&e->states[e->state_count], r01_entity_default_state_name(e->state_count));
        e->state_count++;
    }
    return &e->states[state_idx];
}

R01EntityFrame *r01_entity_ensure_frame(R01EntityType *e, int state_idx, int frame_idx) {
    R01EntityState *st = r01_entity_ensure_state(e, state_idx);
    if (!st || frame_idx < 0 || frame_idx >= R01_ENTITY_FRAMES_MAX) {
        return NULL;
    }
    while (st->frame_count <= frame_idx) {
        memset(&st->frames[st->frame_count], 0, sizeof(st->frames[0]));
        st->frame_count++;
    }
    return &st->frames[frame_idx];
}

static int frame_is_empty(const R01EntityFrame *fr) {
    return !fr || fr->part_count < 1;
}

static int state_is_empty(const R01EntityState *st) {
    int fi;
    if (!st) {
        return 1;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        if (!frame_is_empty(&st->frames[fi])) {
            return 0;
        }
    }
    return 1;
}

int r01_entity_trim_last_frame(R01EntityType *e, int state_idx) {
    R01EntityState *st = r01_entity_state(e, state_idx);
    if (!st || st->frame_count <= 1) {
        return 0;
    }
    if (!frame_is_empty(&st->frames[st->frame_count - 1])) {
        return 0;
    }
    st->frame_count--;
    memset(&st->frames[st->frame_count], 0, sizeof(st->frames[0]));
    return 1;
}

int r01_entity_trim_last_state(R01EntityType *e) {
    if (!e || e->state_count <= 1) {
        return 0;
    }
    if (!state_is_empty(&e->states[e->state_count - 1])) {
        return 0;
    }
    e->state_count--;
    memset(&e->states[e->state_count], 0, sizeof(e->states[0]));
    return 1;
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
    inst->flip_h = 0;
    inst->flip_v = 0;
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

void r01_entity_part_instance_pose(const R01EntityState *st, const R01EntityPart *pt, int inst_flip_h,
                                   int inst_flip_v, int *out_dx, int *out_dy, int *out_flip_h, int *out_flip_v) {
    int dx = pt ? pt->dx : 0;
    int dy = pt ? pt->dy : 0;
    int fh = pt ? pt->flip_h : 0;
    int fv = pt ? pt->flip_v : 0;
    int ox = st ? st->origin_x : 0;
    int oy = st ? st->origin_y : 0;
    if (inst_flip_h) {
        dx = 2 * ox - dx - 8;
        fh = !fh;
    }
    if (inst_flip_v) {
        dy = 2 * oy - dy - 8;
        fv = !fv;
    }
    if (out_dx) {
        *out_dx = dx;
    }
    if (out_dy) {
        *out_dy = dy;
    }
    if (out_flip_h) {
        *out_flip_h = fh ? 1 : 0;
    }
    if (out_flip_v) {
        *out_flip_v = fv ? 1 : 0;
    }
}
