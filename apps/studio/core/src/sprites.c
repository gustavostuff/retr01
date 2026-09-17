#include "retr01_studio/sprites.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/project.h"

#include <string.h>

static int chr_tile_blank(const uint8_t *tile) {
    int b;
    if (!tile) {
        return 1;
    }
    for (b = 0; b < R01_TILE_BYTES; b++) {
        if (tile[b]) {
            return 0;
        }
    }
    return 1;
}

int r01_chr_find_spr_bank_space(const R01World *w) {
    int bank;
    if (!w) {
        return -1;
    }
    for (bank = 0; bank < R01_SPR_BANKS; bank++) {
        if (w->spr_banks[bank].tile_count < R01_TILES_PER_BANK) {
            return bank;
        }
    }
    return -1;
}

int r01_chr_alloc_spr_tile(R01World *w, int bank) {
    R01ChrBank *b;
    if (!w || bank < 0 || bank >= R01_SPR_BANKS) {
        return -1;
    }
    b = &w->spr_banks[bank];
    if (b->tile_count >= R01_TILES_PER_BANK) {
        return -1;
    }
    memset(b->chr + (size_t)b->tile_count * R01_TILE_BYTES, 0, R01_TILE_BYTES);
    b->tile_count++;
    return b->tile_count - 1;
}

int r01_chr_write_spr_tile(R01World *w, int bank, int tile_id, const uint8_t tile[R01_TILE_BYTES]) {
    R01ChrBank *b;
    if (!w || !tile || bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return -1;
    }
    b = &w->spr_banks[bank];
    if (tile_id >= b->tile_count) {
        b->tile_count = tile_id + 1;
    }
    memcpy(b->chr + (size_t)tile_id * R01_TILE_BYTES, tile, R01_TILE_BYTES);
    return 0;
}

const uint8_t *r01_chr_spr_tile(const R01World *w, int bank, int tile_id) {
    if (!w || bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0) {
        return NULL;
    }
    if (tile_id >= w->spr_banks[bank].tile_count) {
        return NULL;
    }
    return w->spr_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
}

const uint8_t *r01_chr_resolve_spr(const R01Project *p, const R01World *w, int bank, int tile_id) {
    if (r01_is_player_chr_bank(bank)) {
        return r01_player_bank_tile(p, tile_id);
    }
    return r01_chr_spr_tile(w, bank, tile_id);
}

int r01_chr_write_resolved_spr(R01Project *p, R01World *w, int bank, int tile_id,
                               const uint8_t tile[R01_TILE_BYTES]) {
    if (r01_is_player_chr_bank(bank)) {
        return r01_player_bank_write_tile(p, tile_id, tile);
    }
    return r01_chr_write_spr_tile(w, bank, tile_id, tile);
}

static void remap_spr_part(R01EntityPart *pt, int bank, const int *id_map, int map_n) {
    int nid;
    if (!pt || !id_map || pt->bank != bank || pt->tile_id < 0 || pt->tile_id >= map_n) {
        return;
    }
    nid = id_map[pt->tile_id];
    pt->tile_id = nid < 0 ? 0 : nid;
}

static void remap_world_spr_refs(R01World *w, int bank, const int *id_map, int map_n) {
    int ei, si, fi, pi, mi, ci;
    if (!w || !id_map || bank < 0 || bank >= R01_SPR_BANKS) {
        return;
    }
    for (ei = 0; ei < w->entity_count; ei++) {
        R01EntityType *ent = &w->entities[ei];
        for (si = 0; si < ent->state_count && si < R01_ENTITY_STATES_MAX; si++) {
            for (fi = 0; fi < ent->states[si].frame_count && fi < R01_ENTITY_FRAMES_MAX; fi++) {
                R01EntityFrame *fr = &ent->states[si].frames[fi];
                for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                    remap_spr_part(&fr->parts[pi], bank, id_map, map_n);
                }
            }
        }
    }
    for (mi = 0; mi < w->metasprite_count; mi++) {
        R01EntityFrame *fr = &w->metasprites[mi].frame;
        for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
            remap_spr_part(&fr->parts[pi], bank, id_map, map_n);
        }
    }
    for (ci = 0; ci < w->sprite_count; ci++) {
        if (w->sprites[ci].bank == bank && w->sprites[ci].tile_id >= 0 && w->sprites[ci].tile_id < map_n) {
            int nid = id_map[w->sprites[ci].tile_id];
            w->sprites[ci].tile_id = nid < 0 ? 0 : nid;
        }
    }
}

void r01_chr_densify_spr_bank(R01World *w, int bank) {
    R01ChrBank *b;
    int id_map[R01_TILES_PER_BANK];
    int old_n;
    int write = 0;
    int id;
    uint8_t packed[R01_BANK_CHR_BYTES];

    if (!w || bank < 0 || bank >= R01_SPR_BANKS) {
        return;
    }
    b = &w->spr_banks[bank];
    old_n = b->tile_count;
    if (old_n < 1) {
        return;
    }
    if (old_n > R01_TILES_PER_BANK) {
        old_n = R01_TILES_PER_BANK;
    }
    memset(packed, 0, sizeof(packed));
    for (id = 0; id < old_n; id++) {
        const uint8_t *src = b->chr + (size_t)id * R01_TILE_BYTES;
        if (chr_tile_blank(src)) {
            id_map[id] = -1;
            continue;
        }
        memcpy(packed + (size_t)write * R01_TILE_BYTES, src, R01_TILE_BYTES);
        id_map[id] = write;
        write++;
    }
    for (id = old_n; id < R01_TILES_PER_BANK; id++) {
        id_map[id] = -1;
    }
    if (write == old_n) {
        int holes = 0;
        for (id = 0; id < old_n; id++) {
            if (id_map[id] != id) {
                holes = 1;
                break;
            }
        }
        if (!holes) {
            return;
        }
    }
    memcpy(b->chr, packed, (size_t)write * R01_TILE_BYTES);
    if (write < old_n) {
        memset(b->chr + (size_t)write * R01_TILE_BYTES, 0, (size_t)(old_n - write) * R01_TILE_BYTES);
    }
    b->tile_count = write;
    remap_world_spr_refs(w, bank, id_map, R01_TILES_PER_BANK);
}

static void remap_player_part(R01EntityPart *pt, const int *id_map, int map_n) {
    int nid;
    if (!pt || !id_map || !r01_is_player_chr_bank(pt->bank) || pt->tile_id < 0 || pt->tile_id >= map_n) {
        return;
    }
    nid = id_map[pt->tile_id];
    pt->tile_id = nid < 0 ? 0 : nid;
}

void r01_project_densify_player_bank(R01Project *p) {
    R01ChrBank *b;
    int id_map[R01_TILES_PER_BANK];
    int old_n;
    int write = 0;
    int id, wi;
    uint8_t packed[R01_BANK_CHR_BYTES];

    if (!p) {
        return;
    }
    b = &p->player_bank;
    old_n = b->tile_count;
    if (old_n < 1) {
        return;
    }
    if (old_n > R01_TILES_PER_BANK) {
        old_n = R01_TILES_PER_BANK;
    }
    memset(packed, 0, sizeof(packed));
    for (id = 0; id < old_n; id++) {
        const uint8_t *src = b->chr + (size_t)id * R01_TILE_BYTES;
        if (chr_tile_blank(src)) {
            id_map[id] = -1;
            continue;
        }
        memcpy(packed + (size_t)write * R01_TILE_BYTES, src, R01_TILE_BYTES);
        id_map[id] = write;
        write++;
    }
    for (id = old_n; id < R01_TILES_PER_BANK; id++) {
        id_map[id] = -1;
    }
    if (write == old_n) {
        int holes = 0;
        for (id = 0; id < old_n; id++) {
            if (id_map[id] != id) {
                holes = 1;
                break;
            }
        }
        if (!holes) {
            return;
        }
    }
    memcpy(b->chr, packed, (size_t)write * R01_TILE_BYTES);
    if (write < old_n) {
        memset(b->chr + (size_t)write * R01_TILE_BYTES, 0, (size_t)(old_n - write) * R01_TILE_BYTES);
    }
    b->tile_count = write;
    for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
        R01World *w = &p->worlds[wi];
        int ei, si, fi, pi, mi, ci;
        for (ei = 0; ei < w->entity_count; ei++) {
            R01EntityType *ent = &w->entities[ei];
            for (si = 0; si < ent->state_count && si < R01_ENTITY_STATES_MAX; si++) {
                for (fi = 0; fi < ent->states[si].frame_count && fi < R01_ENTITY_FRAMES_MAX; fi++) {
                    R01EntityFrame *fr = &ent->states[si].frames[fi];
                    for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                        remap_player_part(&fr->parts[pi], id_map, R01_TILES_PER_BANK);
                    }
                }
            }
        }
        for (mi = 0; mi < w->metasprite_count; mi++) {
            R01EntityFrame *fr = &w->metasprites[mi].frame;
            for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                remap_player_part(&fr->parts[pi], id_map, R01_TILES_PER_BANK);
            }
        }
        for (ci = 0; ci < w->sprite_count; ci++) {
            if (r01_is_player_chr_bank(w->sprites[ci].bank) && w->sprites[ci].tile_id >= 0 &&
                w->sprites[ci].tile_id < R01_TILES_PER_BANK) {
                int nid = id_map[w->sprites[ci].tile_id];
                w->sprites[ci].tile_id = nid < 0 ? 0 : nid;
            }
        }
    }
}

void r01_chr_densify_bg_bank(R01World *w, int bank) {
    R01ChrBank *b;
    int id_map[R01_TILES_PER_BANK];
    int old_n;
    int write;
    int id, si, cell, mi, corner;
    uint8_t packed[R01_BANK_CHR_BYTES];

    if (!w || bank < 0 || bank >= R01_BG_BANKS) {
        return;
    }
    b = &w->bg_banks[bank];
    old_n = b->tile_count;
    if (old_n < 2) {
        return;
    }
    if (old_n > R01_TILES_PER_BANK) {
        old_n = R01_TILES_PER_BANK;
    }
    /* Tile 0 stays put (blank / locked). Pack 1..n into 1..m. */
    memset(packed, 0, sizeof(packed));
    memcpy(packed, b->chr, R01_TILE_BYTES);
    id_map[0] = 0;
    write = 1;
    for (id = 1; id < old_n; id++) {
        const uint8_t *src = b->chr + (size_t)id * R01_TILE_BYTES;
        if (chr_tile_blank(src)) {
            id_map[id] = 0; /* blank holes collapse to tile 0 */
            continue;
        }
        memcpy(packed + (size_t)write * R01_TILE_BYTES, src, R01_TILE_BYTES);
        id_map[id] = write;
        write++;
    }
    for (id = old_n; id < R01_TILES_PER_BANK; id++) {
        id_map[id] = 0;
    }
    if (write == old_n) {
        int holes = 0;
        for (id = 1; id < old_n; id++) {
            if (id_map[id] != id) {
                holes = 1;
                break;
            }
        }
        if (!holes) {
            return;
        }
    }
    memcpy(b->chr, packed, (size_t)write * R01_TILE_BYTES);
    if (write < old_n) {
        memset(b->chr + (size_t)write * R01_TILE_BYTES, 0, (size_t)(old_n - write) * R01_TILE_BYTES);
    }
    b->tile_count = write;
    for (si = 0; si < w->screen_count; si++) {
        R01Screen *s = &w->screens[si];
        if (!s->present) {
            continue;
        }
        for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
            if (r01_attr_bank(s->attrs[cell]) == bank) {
                int tid = s->tiles[cell];
                if (tid >= 0 && tid < R01_TILES_PER_BANK) {
                    s->tiles[cell] = (uint8_t)id_map[tid];
                }
            }
        }
        r01_screen_fill_pixels_from_bank(w, s);
    }
    for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
        R01Screen *s = &w->bg0_screens[si];
        if (!s->present) {
            continue;
        }
        for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
            if (r01_attr_bank(s->attrs[cell]) == bank) {
                int tid = s->tiles[cell];
                if (tid >= 0 && tid < R01_TILES_PER_BANK) {
                    s->tiles[cell] = (uint8_t)id_map[tid];
                }
            }
        }
        r01_screen_fill_pixels_from_bank(w, s);
    }
    for (mi = 0; mi < w->metatile_count; mi++) {
        for (corner = 0; corner < 4; corner++) {
            if (r01_attr_bank(w->metatiles[mi].attr[corner]) == bank) {
                int tid = w->metatiles[mi].tile[corner];
                if (tid >= 0 && tid < R01_TILES_PER_BANK) {
                    w->metatiles[mi].tile[corner] = (uint8_t)id_map[tid];
                }
            }
        }
    }
}

void r01_project_densify_all_banks(R01Project *p) {
    int wi, bi;
    if (!p) {
        return;
    }
    for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
        R01World *w = &p->worlds[wi];
        for (bi = 0; bi < R01_BG_BANKS; bi++) {
            r01_chr_densify_bg_bank(w, bi);
        }
        for (bi = 0; bi < R01_SPR_BANKS; bi++) {
            r01_chr_densify_spr_bank(w, bi);
        }
    }
    r01_project_densify_player_bank(p);
}

int r01_world_sprite_add(R01World *w, int bank, int tile_id, int pal) {
    R01SpriteDef *s;
    if (!w || w->sprite_count >= R01_MAX_SPRITES) {
        return -1;
    }
    if (r01_is_player_chr_bank(bank)) {
        if (tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
            return -1;
        }
    } else if (bank < 0 || bank >= R01_SPR_BANKS || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return -1;
    }
    if (pal < 0) {
        pal = 0;
    }
    if (pal > 3) {
        pal = 3;
    }
    s = &w->sprites[w->sprite_count];
    s->bank = bank;
    s->tile_id = tile_id;
    s->pal = pal;
    w->sprite_count++;
    return w->sprite_count - 1;
}

int r01_world_sprite_remove(R01World *w, int catalog_idx) {
    int i;
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    for (i = catalog_idx; i < w->sprite_count - 1; i++) {
        w->sprites[i] = w->sprites[i + 1];
    }
    w->sprite_count--;
    memset(&w->sprites[w->sprite_count], 0, sizeof(w->sprites[0]));
    return 0;
}

int r01_world_sprite_set_pal(R01World *w, int catalog_idx, int pal) {
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    if (pal < 0) {
        pal = 0;
    }
    if (pal > 3) {
        pal = 3;
    }
    w->sprites[catalog_idx].pal = pal;
    return 0;
}

int r01_world_sprite_move_bank(R01World *w, int catalog_idx, int new_bank) {
    R01SpriteDef *s;
    const uint8_t *src;
    uint8_t copy[R01_TILE_BYTES];
    int new_id;
    int old_bank;
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return -1;
    }
    if (new_bank < 0 || new_bank >= R01_SPR_BANKS) {
        return -1;
    }
    s = &w->sprites[catalog_idx];
    if (s->bank == new_bank) {
        return 0;
    }
    src = r01_chr_spr_tile(w, s->bank, s->tile_id);
    if (!src) {
        return -1;
    }
    memcpy(copy, src, R01_TILE_BYTES);
    old_bank = s->bank;
    new_id = r01_chr_alloc_spr_tile(w, new_bank);
    if (new_id < 0) {
        return -1;
    }
    if (r01_chr_write_spr_tile(w, new_bank, new_id, copy) != 0) {
        return -1;
    }
    s->bank = new_bank;
    s->tile_id = new_id;
    r01_chr_densify_spr_bank(w, old_bank);
    return 0;
}
