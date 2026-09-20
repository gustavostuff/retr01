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
    st->frame_count = 1;
    st->frames[0].delay = 1;
    st->hitbox_w = R01_ENTITY_HITBOX_W;
    st->hitbox_h = R01_ENTITY_HITBOX_H;
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

int r01_world_entity_add(R01Project *p) {
    R01EntityType *e;
    if (!p || p->entity_count >= R01_MAX_ENTITY_TYPES) {
        return -1;
    }
    e = &p->entities[p->entity_count];
    r01_entity_type_init(e);
    p->entity_count++;
    return p->entity_count - 1;
}

int r01_world_entity_remove(R01Project *p, int type_idx) {
    int i, j, wi;
    if (!p || type_idx < 0 || type_idx >= p->entity_count) {
        return -1;
    }
    for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
        R01World *w = &p->worlds[wi];
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
    }
    if (p->player_entity == type_idx) {
        p->player_entity = -1;
    } else if (p->player_entity > type_idx) {
        p->player_entity--;
    }
    for (j = type_idx; j < p->entity_count - 1; j++) {
        p->entities[j] = p->entities[j + 1];
    }
    p->entity_count--;
    memset(&p->entities[p->entity_count], 0, sizeof(p->entities[0]));
    return 0;
}

void r01_world_set_player_entity(R01Project *p, int type_idx) {
    if (!p) {
        return;
    }
    if (type_idx < 0 || type_idx >= p->entity_count) {
        p->player_entity = -1;
        return;
    }
    p->player_entity = type_idx;
}

int r01_world_player_entity(const R01Project *p) {
    if (!p || p->player_entity < 0 || p->player_entity >= p->entity_count) {
        return -1;
    }
    return p->player_entity;
}

int r01_project_set_player_entity(R01Project *p, R01World *w, int type_idx) {
    (void)w;
    if (!p) {
        return -1;
    }
    r01_world_set_player_entity(p, type_idx);
    return 0;
}

R01EntityType *r01_world_entity(R01Project *p, int type_idx) {
    if (!p || type_idx < 0 || type_idx >= p->entity_count) {
        return NULL;
    }
    return &p->entities[type_idx];
}

const R01EntityType *r01_world_entity_const(const R01Project *p, int type_idx) {
    if (!p || type_idx < 0 || type_idx >= p->entity_count) {
        return NULL;
    }
    return &p->entities[type_idx];
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

int r01_entity_crouch_state_index(const R01EntityType *e) {
    int i;
    char slug[R01_ENTITY_NAME_MAX];
    if (!e) {
        return -1;
    }
    for (i = 0; i < e->state_count && i < R01_ENTITY_STATES_MAX; i++) {
        r01_id_slugify(slug, sizeof(slug), e->states[i].name);
        if (strcmp(slug, "crouch") == 0 || strcmp(slug, "crouching") == 0) {
            return i;
        }
    }
    return -1;
}

R01EntityState *r01_entity_ensure_state(R01EntityType *e, int state_idx) {
    if (!e || state_idx < 0 || state_idx >= R01_ENTITY_STATES_MAX) {
        return NULL;
    }
    if (e->state_count < 0) {
        e->state_count = 0;
    }
    if (e->state_count > R01_ENTITY_STATES_MAX) {
        e->state_count = R01_ENTITY_STATES_MAX;
    }
    while (e->state_count <= state_idx && e->state_count < R01_ENTITY_STATES_MAX) {
        r01_entity_state_init(&e->states[e->state_count], r01_entity_default_state_name(e->state_count));
        e->state_count++;
    }
    if (state_idx >= e->state_count) {
        return NULL;
    }
    return &e->states[state_idx];
}

R01EntityFrame *r01_entity_ensure_frame(R01EntityType *e, int state_idx, int frame_idx) {
    R01EntityState *st = r01_entity_ensure_state(e, state_idx);
    if (!st || frame_idx < 0 || frame_idx >= R01_ENTITY_FRAMES_MAX) {
        return NULL;
    }
    while (st->frame_count <= frame_idx) {
        R01EntityFrame *fr = &st->frames[st->frame_count];
        memset(fr, 0, sizeof(*fr));
        fr->delay = 1;
        if (st->frame_count > 0) {
            const R01EntityFrame *prev = &st->frames[st->frame_count - 1];
            fr->origin_x = prev->origin_x;
            fr->origin_y = prev->origin_y;
        }
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

int r01_entity_state_drawable_frame_count(const R01EntityState *st) {
    int fi, n = 0;
    if (!st) {
        return 0;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        if (st->frames[fi].part_count > 0) {
            n++;
        }
    }
    return n;
}

int r01_entity_state_drawable_frame_index(const R01EntityState *st, int slot) {
    int fi, n = 0;
    if (!st || slot < 0) {
        return 0;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        if (st->frames[fi].part_count > 0) {
            if (n == slot) {
                return fi;
            }
            n++;
        }
    }
    return 0;
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

int r01_entity_frame_bring_part_front(R01EntityFrame *fr, int part_idx) {
    R01EntityPart tmp;
    int i;
    if (!fr || part_idx < 0 || part_idx >= fr->part_count) {
        return -1;
    }
    if (part_idx == fr->part_count - 1) {
        return part_idx;
    }
    tmp = fr->parts[part_idx];
    for (i = part_idx; i < fr->part_count - 1; i++) {
        fr->parts[i] = fr->parts[i + 1];
    }
    fr->parts[fr->part_count - 1] = tmp;
    return fr->part_count - 1;
}

void r01_entity_frame_recompute_guides(R01EntityFrame *fr) {
    int pi;
    int have = 0;
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    int ox, oy;
    if (!fr) {
        return;
    }
    for (pi = 0; pi < fr->part_count; pi++) {
        const R01EntityPart *pt = &fr->parts[pi];
        int x0 = pt->dx;
        int y0 = pt->dy;
        int x1 = pt->dx + 8;
        int y1 = pt->dy + 8;
        if (!have) {
            min_x = x0;
            min_y = y0;
            max_x = x1;
            max_y = y1;
            have = 1;
        } else {
            if (x0 < min_x) {
                min_x = x0;
            }
            if (y0 < min_y) {
                min_y = y0;
            }
            if (x1 > max_x) {
                max_x = x1;
            }
            if (y1 > max_y) {
                max_y = y1;
            }
        }
    }
    if (!have) {
        fr->origin_x = 0;
        fr->origin_y = 0;
        return;
    }
    ox = (min_x + max_x) / 2;
    oy = (min_y + max_y) / 2;
    if (ox < 0) {
        ox = 0;
    }
    if (oy < 0) {
        oy = 0;
    }
    if (ox > R01_ENTITY_COMPOSE_PX) {
        ox = R01_ENTITY_COMPOSE_PX;
    }
    if (oy > R01_ENTITY_COMPOSE_PX) {
        oy = R01_ENTITY_COMPOSE_PX;
    }
    fr->origin_x = ox;
    fr->origin_y = oy;
}

void r01_entity_state_clamp_hitbox(R01EntityState *st) {
    int hx, hy, hw, hh;
    if (!st) {
        return;
    }
    hw = st->hitbox_w > 0 ? st->hitbox_w : R01_ENTITY_HITBOX_W;
    hh = st->hitbox_h > 0 ? st->hitbox_h : R01_ENTITY_HITBOX_H;
    hx = st->hitbox_x;
    hy = st->hitbox_y;
    if (hw > R01_ENTITY_COMPOSE_PX) {
        hw = R01_ENTITY_COMPOSE_PX;
    }
    if (hh > R01_ENTITY_COMPOSE_PX) {
        hh = R01_ENTITY_COMPOSE_PX;
    }
    if (hx < 0) {
        hx = 0;
    }
    if (hy < 0) {
        hy = 0;
    }
    if (hx > R01_ENTITY_COMPOSE_PX - hw) {
        hx = R01_ENTITY_COMPOSE_PX - hw;
    }
    if (hy > R01_ENTITY_COMPOSE_PX - hh) {
        hy = R01_ENTITY_COMPOSE_PX - hh;
    }
    st->hitbox_w = hw;
    st->hitbox_h = hh;
    st->hitbox_x = hx;
    st->hitbox_y = hy;
}

void r01_entity_state_recompute_guides(R01EntityState *st) {
    int fi;
    if (!st) {
        return;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        r01_entity_frame_recompute_guides(&st->frames[fi]);
    }
    r01_entity_state_clamp_hitbox(st);
}

int r01_world_entity_from_sprite(R01Project *p, int sprite_catalog_idx) {
    R01EntityType *e;
    R01EntityFrame *fr;
    R01EntityPart part;
    const R01SpriteDef *sp;
    int idx;
    if (!p || sprite_catalog_idx < 0 || sprite_catalog_idx >= p->sprite_count) {
        return -1;
    }
    sp = &p->sprites[sprite_catalog_idx];
    idx = r01_world_entity_add(p);
    if (idx < 0) {
        return -1;
    }
    e = &p->entities[idx];
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
    r01_entity_frame_recompute_guides(fr);
    return idx;
}

int r01_world_instance_add(R01World *w, int type_id, int world_x, int world_y) {
    R01EntityInstance *inst;
    if (!w || type_id < 0 || type_id >= R01_MAX_ENTITY_TYPES || w->instance_count >= R01_MAX_ENTITY_INSTANCES) {
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

int r01_world_place_sprite(R01Project *p, R01World *w, int sprite_catalog_idx, int world_x, int world_y) {
    int type_id = r01_world_entity_from_sprite(p, sprite_catalog_idx);
    if (type_id < 0) {
        return -1;
    }
    return r01_world_instance_add(w, type_id, world_x, world_y);
}

int r01_world_place_entity(R01World *w, int type_id, int world_x, int world_y) {
    return r01_world_instance_add(w, type_id, world_x, world_y);
}

void r01_entity_part_instance_pose(const R01EntityFrame *fr, const R01EntityPart *pt, int inst_flip_h,
                                   int inst_flip_v, int *out_dx, int *out_dy, int *out_flip_h, int *out_flip_v) {
    int dx = pt ? pt->dx : 0;
    int dy = pt ? pt->dy : 0;
    int fh = pt ? pt->flip_h : 0;
    int fv = pt ? pt->flip_v : 0;
    int ox = fr ? fr->origin_x : 0;
    int oy = fr ? fr->origin_y : 0;
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
