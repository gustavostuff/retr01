#include "retr01_studio/entities.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/sprites.h"

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

typedef struct R01PlayerBankRemap {
    int old_bank;
    int old_tile;
    int new_bank;
    int new_tile;
} R01PlayerBankRemap;

static int entity_collect_unique_tiles(const R01EntityType *ent, int want_global_spr, R01PlayerBankRemap *out,
                                       int cap) {
    int n = 0;
    int si, fi, pi, i;
    if (!ent || !out || cap < 1) {
        return 0;
    }
    for (si = 0; si < ent->state_count && si < R01_ENTITY_STATES_MAX; si++) {
        const R01EntityState *st = &ent->states[si];
        for (fi = 0; fi < st->frame_count && fi < R01_ENTITY_FRAMES_MAX; fi++) {
            const R01EntityFrame *fr = &st->frames[fi];
            for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                const R01EntityPart *pt = &fr->parts[pi];
                int is_gs = r01_is_global_spr_bank(pt->bank);
                if (want_global_spr ? !is_gs : is_gs) {
                    continue;
                }
                if (pt->tile_id < 0 || pt->tile_id >= R01_TILES_PER_BANK) {
                    continue;
                }
                for (i = 0; i < n; i++) {
                    if (out[i].old_bank == pt->bank && out[i].old_tile == pt->tile_id) {
                        break;
                    }
                }
                if (i < n) {
                    continue;
                }
                if (n >= cap) {
                    return -1;
                }
                out[n].old_bank = pt->bank;
                out[n].old_tile = pt->tile_id;
                out[n].new_bank = -1;
                out[n].new_tile = -1;
                n++;
            }
        }
    }
    /* Stable sheet order: low bank, then low tile id. */
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++) {
            if (out[j].old_bank < out[i].old_bank ||
                (out[j].old_bank == out[i].old_bank && out[j].old_tile < out[i].old_tile)) {
                R01PlayerBankRemap tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return n;
}

static int remap_dest_taken(const R01PlayerBankRemap *map, int assigned, int dest_bank, int dest) {
    int i;
    for (i = 0; i < assigned; i++) {
        if (map[i].new_bank == dest_bank && map[i].new_tile == dest) {
            return 1;
        }
    }
    return 0;
}

static int pick_other_spr_dest_tile(const R01Project *p, int spr_bank, const R01PlayerBankRemap *map,
                                    int assigned) {
    int dest;
    int start = 0;
    if (!p || spr_bank < 0 || spr_bank >= R01_SPR_BANKS) {
        return -1;
    }
    start = p->other_spr_banks[spr_bank].tile_count;
    for (dest = 0; dest < start; dest++) {
        if (!remap_dest_taken(map, assigned, R01_GLOBAL_SPR_BANK_BASE + spr_bank, dest)) {
            const uint8_t *t = r01_other_spr_tile(p, spr_bank, dest);
            int blank = 1;
            int b;
            if (t) {
                for (b = 0; b < R01_TILE_BYTES; b++) {
                    if (t[b]) {
                        blank = 0;
                        break;
                    }
                }
            }
            if (blank) {
                return dest;
            }
        }
    }
    for (dest = start; dest < R01_TILES_PER_BANK; dest++) {
        if (!remap_dest_taken(map, assigned, R01_GLOBAL_SPR_BANK_BASE + spr_bank, dest)) {
            return dest;
        }
    }
    return -1;
}

static void entity_remap_parts(R01EntityType *ent, const R01PlayerBankRemap *map, int n) {
    int si, fi, pi, i;
    if (!ent || !map || n < 1) {
        return;
    }
    for (si = 0; si < ent->state_count && si < R01_ENTITY_STATES_MAX; si++) {
        R01EntityState *st = &ent->states[si];
        for (fi = 0; fi < st->frame_count && fi < R01_ENTITY_FRAMES_MAX; fi++) {
            R01EntityFrame *fr = &st->frames[fi];
            for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                R01EntityPart *pt = &fr->parts[pi];
                for (i = 0; i < n; i++) {
                    if (pt->bank == map[i].old_bank && pt->tile_id == map[i].old_tile) {
                        pt->bank = map[i].new_bank;
                        pt->tile_id = map[i].new_tile;
                        break;
                    }
                }
            }
        }
    }
}

static void catalog_remap_tiles(R01World *w, const R01PlayerBankRemap *map, int n) {
    int ci, i;
    if (!w || !map || n < 1) {
        return;
    }
    for (ci = 0; ci < w->sprite_count; ci++) {
        R01SpriteDef *s = &w->sprites[ci];
        for (i = 0; i < n; i++) {
            if (s->bank == map[i].old_bank && s->tile_id == map[i].old_tile) {
                s->bank = map[i].new_bank;
                s->tile_id = map[i].new_tile;
                break;
            }
        }
    }
}

/* Clear one world SPR tile slot (after exclusive move to global other SPR). */
static void clear_world_spr_tile(R01World *w, int bank, int tile_id) {
    uint8_t *dst;
    if (!w || bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return;
    }
    if (tile_id >= w->spr_banks[bank].tile_count) {
        return;
    }
    dst = w->spr_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
    memset(dst, 0, R01_TILE_BYTES);
}

static void clear_other_spr_tile(R01Project *p, int auth_bank, int tile_id) {
    int spr_bank;
    uint8_t *dst;
    if (!p || !r01_is_global_spr_bank(auth_bank) || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return;
    }
    spr_bank = r01_global_spr_index(auth_bank);
    if (tile_id >= p->other_spr_banks[spr_bank].tile_count) {
        return;
    }
    dst = p->other_spr_banks[spr_bank].chr + (size_t)tile_id * R01_TILE_BYTES;
    memset(dst, 0, R01_TILE_BYTES);
}

static void metasprite_remap_tiles(R01World *w, const R01PlayerBankRemap *map, int n) {
    int mi, pi, i;
    if (!w || !map || n < 1) {
        return;
    }
    for (mi = 0; mi < w->metasprite_count; mi++) {
        R01EntityFrame *fr = &w->metasprites[mi].frame;
        for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
            R01EntityPart *pt = &fr->parts[pi];
            for (i = 0; i < n; i++) {
                if (pt->bank == map[i].old_bank && pt->tile_id == map[i].old_tile) {
                    pt->bank = map[i].new_bank;
                    pt->tile_id = map[i].new_tile;
                    break;
                }
            }
        }
    }
}

static void apply_remap_all(R01World *w, R01EntityType *ent, int type_idx, const R01PlayerBankRemap *map,
                            int n) {
    int ei;
    entity_remap_parts(ent, map, n);
    for (ei = 0; ei < w->entity_count; ei++) {
        if (ei == type_idx) {
            continue;
        }
        entity_remap_parts(&w->entities[ei], map, n);
    }
    catalog_remap_tiles(w, map, n);
    metasprite_remap_tiles(w, map, n);
}

static int entity_to_other_spr(R01Project *p, R01World *w, int type_idx) {
    R01EntityType *ent;
    R01PlayerBankRemap map[R01_ENTITY_STATES_MAX * R01_ENTITY_FRAMES_MAX * R01_ENTITY_PARTS_MAX];
    int n;
    int i;
    if (!p || !w || type_idx < 0 || type_idx >= w->entity_count) {
        return -1;
    }
    ent = &w->entities[type_idx];
    n = entity_collect_unique_tiles(ent, 0, map, (int)(sizeof(map) / sizeof(map[0])));
    if (n < 0) {
        return -1;
    }
    /* Preserve world bank index: world SPR B -> other_spr_banks[B], authoring bank BASE+B. */
    for (i = 0; i < n; i++) {
        const uint8_t *src = r01_chr_spr_tile(w, map[i].old_bank, map[i].old_tile);
        uint8_t blank[R01_TILE_BYTES];
        int spr_bank = map[i].old_bank;
        int dest;
        if (spr_bank < 0 || spr_bank >= R01_SPR_BANKS) {
            return -1;
        }
        dest = pick_other_spr_dest_tile(p, spr_bank, map, i);
        if (dest < 0) {
            return -1;
        }
        memset(blank, 0, sizeof(blank));
        if (r01_other_spr_write_tile(p, spr_bank, dest, src ? src : blank) != 0) {
            return -1;
        }
        map[i].new_bank = R01_GLOBAL_SPR_BANK_BASE + spr_bank;
        map[i].new_tile = dest;
    }
    apply_remap_all(w, ent, type_idx, map, n);
    /* Exclusive move: clear world slots, then densify so Banks stay continuous. */
    for (i = 0; i < n; i++) {
        clear_world_spr_tile(w, map[i].old_bank, map[i].old_tile);
    }
    {
        int touched[R01_SPR_BANKS];
        memset(touched, 0, sizeof(touched));
        for (i = 0; i < n; i++) {
            if (map[i].old_bank >= 0 && map[i].old_bank < R01_SPR_BANKS) {
                touched[map[i].old_bank] = 1;
            }
        }
        for (i = 0; i < R01_SPR_BANKS; i++) {
            if (touched[i]) {
                r01_chr_densify_spr_bank(w, i);
            }
        }
    }
    return 0;
}

static int entity_from_other_spr(R01Project *p, R01World *w, int type_idx) {
    R01EntityType *ent;
    R01PlayerBankRemap map[R01_ENTITY_STATES_MAX * R01_ENTITY_FRAMES_MAX * R01_ENTITY_PARTS_MAX];
    int n;
    int i;
    int touched[R01_SPR_BANKS];
    if (!p || !w || type_idx < 0 || type_idx >= w->entity_count) {
        return -1;
    }
    ent = &w->entities[type_idx];
    n = entity_collect_unique_tiles(ent, 1, map, (int)(sizeof(map) / sizeof(map[0])));
    if (n < 0) {
        return -1;
    }
    memset(touched, 0, sizeof(touched));
    for (i = 0; i < n; i++) {
        int gidx = r01_global_spr_index(map[i].old_bank);
        const uint8_t *src = r01_other_spr_tile(p, gidx, map[i].old_tile);
        uint8_t blank[R01_TILE_BYTES];
        int bank;
        int dest;
        memset(blank, 0, sizeof(blank));
        bank = r01_chr_find_spr_bank_space(w);
        if (bank < 0) {
            return -1;
        }
        dest = r01_chr_alloc_spr_tile(w, bank);
        if (dest < 0) {
            return -1;
        }
        if (r01_chr_write_spr_tile(w, bank, dest, src ? src : blank) != 0) {
            return -1;
        }
        map[i].new_bank = bank;
        map[i].new_tile = dest;
        if (gidx >= 0 && gidx < R01_SPR_BANKS) {
            touched[gidx] = 1;
        }
    }
    apply_remap_all(w, ent, type_idx, map, n);
    for (i = 0; i < n; i++) {
        clear_other_spr_tile(p, map[i].old_bank, map[i].old_tile);
    }
    for (i = 0; i < R01_SPR_BANKS; i++) {
        if (touched[i]) {
            r01_project_densify_other_spr_bank(p, i);
        }
    }
    return 0;
}

int r01_project_set_player_entity(R01Project *p, R01World *w, int type_idx) {
    int cur;
    if (!p || !w) {
        return -1;
    }
    cur = r01_world_player_entity(w);
    if (type_idx < 0 || type_idx >= w->entity_count) {
        if (cur >= 0) {
            if (entity_from_other_spr(p, w, cur) != 0) {
                return -1;
            }
        }
        r01_world_set_player_entity(w, -1);
        return 0;
    }
    if (cur == type_idx) {
        return 0;
    }
    if (cur >= 0) {
        if (entity_from_other_spr(p, w, cur) != 0) {
            return -1;
        }
        r01_world_set_player_entity(w, -1);
    }
    if (entity_to_other_spr(p, w, type_idx) != 0) {
        return -1;
    }
    r01_world_set_player_entity(w, type_idx);
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

void r01_entity_state_recompute_guides(R01EntityState *st) {
    int fi, pi;
    int have = 0;
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    int ox, oy, hx, hy;
    if (!st) {
        return;
    }
    for (fi = 0; fi < st->frame_count; fi++) {
        const R01EntityFrame *fr = &st->frames[fi];
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
    }
    if (!have) {
        st->origin_x = 0;
        st->origin_y = 0;
        st->hitbox_x = 0;
        st->hitbox_y = 0;
        st->hitbox_w = R01_ENTITY_HITBOX_W;
        st->hitbox_h = R01_ENTITY_HITBOX_H;
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
    st->origin_x = ox;
    st->origin_y = oy;
    /* Keep authored hitbox position/size; only clamp into the compose grid. */
    {
        int hw = st->hitbox_w > 0 ? st->hitbox_w : R01_ENTITY_HITBOX_W;
        int hh = st->hitbox_h > 0 ? st->hitbox_h : R01_ENTITY_HITBOX_H;
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
