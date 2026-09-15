#include "ui/undo/undo_cmds.h"
#include "ui/undo/undo.h"
#include "ui/internal.h"

#include "retr01_studio/chr_pack.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"
#include "retr01_studio/metatiles.h"
#include "retr01_studio/project.h"
#include "retr01_studio/sprites.h"

#include <stdlib.h>
#include <string.h>

static R01Screen *undo_screen_ptr(UiState *ui, int world_idx, int plane, int screen_idx) {
    R01World *w;
    if (!ui || !ui->project || world_idx < 0 || world_idx >= R01_MAX_WORLDS) {
        return NULL;
    }
    w = &ui->project->worlds[world_idx];
    if (plane == UI_WORLDS_PLANE_BG0) {
        if (screen_idx < 0 || screen_idx >= w->bg0_screen_count) {
            return NULL;
        }
        return &w->bg0_screens[screen_idx];
    }
    if (screen_idx < 0 || screen_idx >= w->screen_count) {
        return NULL;
    }
    return &w->screens[screen_idx];
}

static int undo_active_screen_ids(const UiState *ui, int *world_idx, int *plane, int *screen_idx) {
    const R01World *w;
    const R01Screen *s;
    if (!ui || !ui->project) {
        return 0;
    }
    w = r01_project_active_world_const(ui->project);
    s = ui_edit_map_screen(ui);
    if (!w || !s || !s->present) {
        return 0;
    }
    *world_idx = ui->project->active_world;
    *plane = ui->worlds_plane;
    if (*plane == UI_WORLDS_PLANE_BG0) {
        *screen_idx = w->bg0_active_screen;
    } else {
        *screen_idx = ui->project->active_screen;
    }
    return *screen_idx >= 0;
}

static void free_ptr(void *data) {
    free(data);
}

/* ---- paint stroke ---- */

static void paint_stroke_destroy(void *data) {
    UiUndoPaintStroke *st = (UiUndoPaintStroke *)data;
    if (!st) {
        return;
    }
    free(st->cells);
    free(st);
}

static void paint_stroke_apply(UiState *ui, UiUndoPaintStroke *st, int use_new) {
    R01World *w;
    R01Screen *s;
    int i;
    if (!ui || !st || !ui->project) {
        return;
    }
    w = &ui->project->worlds[st->world_idx];
    s = undo_screen_ptr(ui, st->world_idx, st->plane, st->screen_idx);
    if (!w || !s || !s->present) {
        return;
    }
    for (i = 0; i < st->count; i++) {
        UiUndoPaintCell *c = &st->cells[i];
        int tx = (int)(c->cell % R01_SCREEN_TILES_X);
        int ty = (int)(c->cell / R01_SCREEN_TILES_X);
        r01_screen_paint_tile(w, s, tx, ty, use_new ? c->new_tile : c->old_tile,
                              use_new ? c->new_attr : c->old_attr);
    }
}

static void paint_stroke_undo(UiState *ui, void *data) {
    paint_stroke_apply(ui, (UiUndoPaintStroke *)data, 0);
}

static void paint_stroke_redo(UiState *ui, void *data) {
    paint_stroke_apply(ui, (UiUndoPaintStroke *)data, 1);
}

static const UiUndoVTable paint_stroke_vt = {paint_stroke_undo, paint_stroke_redo, paint_stroke_destroy};

static int paint_stroke_ensure_cap(UiUndoPaintStroke *st, int need) {
    UiUndoPaintCell *ncells;
    int ncap;
    if (need <= st->cap) {
        return 0;
    }
    ncap = st->cap < 16 ? 16 : st->cap * 2;
    while (ncap < need) {
        ncap *= 2;
    }
    ncells = (UiUndoPaintCell *)realloc(st->cells, (size_t)ncap * sizeof(*ncells));
    if (!ncells) {
        return -1;
    }
    st->cells = ncells;
    st->cap = ncap;
    return 0;
}

static int paint_stroke_find(const UiUndoPaintStroke *st, uint16_t cell) {
    int i;
    for (i = 0; i < st->count; i++) {
        if (st->cells[i].cell == cell) {
            return i;
        }
    }
    return -1;
}

int ui_undo_paint_begin(UiState *ui) {
    UiUndoPaintStroke *st;
    int wi, plane, si;
    if (!ui) {
        return -1;
    }
    ui_undo_paint_end(ui);
    if (!undo_active_screen_ids(ui, &wi, &plane, &si)) {
        return -1;
    }
    st = (UiUndoPaintStroke *)calloc(1, sizeof(*st));
    if (!st) {
        return -1;
    }
    st->world_idx = wi;
    st->plane = plane;
    st->screen_idx = si;
    ui->undo_paint = st;
    return 0;
}

void ui_undo_paint_end(UiState *ui) {
    UiUndoPaintStroke *st;
    if (!ui || !ui->undo_paint) {
        return;
    }
    st = (UiUndoPaintStroke *)ui->undo_paint;
    ui->undo_paint = NULL;
    if (st->count < 1) {
        paint_stroke_destroy(st);
        return;
    }
    if (ui_undo_push(&ui->undo, &paint_stroke_vt, st, "paint") != 0) {
        paint_stroke_destroy(st);
    }
}

void ui_undo_paint_record_cell(UiState *ui, int tx, int ty, uint8_t old_tile, uint8_t old_attr, uint8_t new_tile,
                               uint8_t new_attr) {
    UiUndoPaintStroke *st;
    uint16_t cell;
    int idx;
    if (!ui || !ui->undo_paint || tx < 0 || ty < 0 || tx >= R01_SCREEN_TILES_X || ty >= R01_SCREEN_TILES_Y) {
        return;
    }
    if (old_tile == new_tile && old_attr == new_attr) {
        return;
    }
    st = (UiUndoPaintStroke *)ui->undo_paint;
    cell = (uint16_t)(ty * R01_SCREEN_TILES_X + tx);
    idx = paint_stroke_find(st, cell);
    if (idx < 0) {
        if (paint_stroke_ensure_cap(st, st->count + 1) != 0) {
            return;
        }
        st->cells[st->count].cell = cell;
        st->cells[st->count].old_tile = old_tile;
        st->cells[st->count].old_attr = old_attr;
        st->cells[st->count].new_tile = new_tile;
        st->cells[st->count].new_attr = new_attr;
        st->count++;
    } else {
        st->cells[idx].new_tile = new_tile;
        st->cells[idx].new_attr = new_attr;
    }
}

/* ---- entity add ---- */

typedef struct UiUndoEntityAdd {
    int world_idx;
    int idx;
    R01EntityType ent;
    int was_player;
} UiUndoEntityAdd;

static void entity_add_undo(UiState *ui, void *data) {
    UiUndoEntityAdd *d = (UiUndoEntityAdd *)data;
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    (void)r01_world_entity_remove(w, d->idx);
}

static void entity_add_redo(UiState *ui, void *data) {
    UiUndoEntityAdd *d = (UiUndoEntityAdd *)data;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (w->entity_count >= R01_MAX_ENTITY_TYPES) {
        return;
    }
    idx = w->entity_count;
    w->entities[idx] = d->ent;
    w->entity_count++;
    d->idx = idx;
    if (d->was_player) {
        w->player_entity = idx;
    }
}

static const UiUndoVTable entity_add_vt = {entity_add_undo, entity_add_redo, free_ptr};

void ui_undo_push_entity_add(UiState *ui, int type_idx) {
    UiUndoEntityAdd *d;
    R01World *w;
    if (!ui || !ui->project) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || type_idx < 0 || type_idx >= w->entity_count) {
        return;
    }
    d = (UiUndoEntityAdd *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = type_idx;
    d->ent = w->entities[type_idx];
    d->was_player = (w->player_entity == type_idx);
    (void)ui_undo_push(&ui->undo, &entity_add_vt, d, "add entity");
}

typedef struct UiUndoEntityRemove {
    int world_idx;
    int idx;
    R01EntityType ent;
    int was_player;
} UiUndoEntityRemove;

static void entity_remove_undo(UiState *ui, void *data) {
    UiUndoEntityRemove *d = (UiUndoEntityRemove *)data;
    R01World *w;
    int i;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (w->entity_count >= R01_MAX_ENTITY_TYPES) {
        return;
    }
    for (i = w->entity_count; i > d->idx; i--) {
        w->entities[i] = w->entities[i - 1];
    }
    w->entities[d->idx] = d->ent;
    w->entity_count++;
    for (i = 0; i < w->instance_count; i++) {
        if (w->instances[i].type_id >= d->idx) {
            w->instances[i].type_id++;
        }
    }
    if (d->was_player) {
        w->player_entity = d->idx;
    } else if (w->player_entity >= d->idx) {
        w->player_entity++;
    }
}

static void entity_remove_redo(UiState *ui, void *data) {
    UiUndoEntityRemove *d = (UiUndoEntityRemove *)data;
    if (!ui || !d || !ui->project) {
        return;
    }
    (void)r01_world_entity_remove(&ui->project->worlds[d->world_idx], d->idx);
}

static const UiUndoVTable entity_remove_vt = {entity_remove_undo, entity_remove_redo, free_ptr};

void ui_undo_push_entity_remove(UiState *ui, int type_idx, const R01EntityType *removed, int was_player) {
    UiUndoEntityRemove *d;
    if (!ui || !ui->project || !removed) {
        return;
    }
    d = (UiUndoEntityRemove *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = type_idx;
    d->ent = *removed;
    d->was_player = was_player;
    (void)ui_undo_push(&ui->undo, &entity_remove_vt, d, "remove entity");
}

/* ---- sprite add ---- */

typedef struct UiUndoSpriteAdd {
    int world_idx;
    int idx;
    R01SpriteDef spr;
} UiUndoSpriteAdd;

static void sprite_add_undo(UiState *ui, void *data) {
    UiUndoSpriteAdd *d = (UiUndoSpriteAdd *)data;
    if (!ui || !d || !ui->project) {
        return;
    }
    (void)r01_world_sprite_remove(&ui->project->worlds[d->world_idx], d->idx);
}

static void sprite_add_redo(UiState *ui, void *data) {
    UiUndoSpriteAdd *d = (UiUndoSpriteAdd *)data;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    idx = r01_world_sprite_add(w, d->spr.bank, d->spr.tile_id, d->spr.pal);
    if (idx >= 0) {
        d->idx = idx;
    }
}

static const UiUndoVTable sprite_add_vt = {sprite_add_undo, sprite_add_redo, free_ptr};

void ui_undo_push_sprite_add(UiState *ui, int catalog_idx) {
    UiUndoSpriteAdd *d;
    R01World *w;
    if (!ui || !ui->project) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || catalog_idx < 0 || catalog_idx >= w->sprite_count) {
        return;
    }
    d = (UiUndoSpriteAdd *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = catalog_idx;
    d->spr = w->sprites[catalog_idx];
    (void)ui_undo_push(&ui->undo, &sprite_add_vt, d, "add sprite");
}

/* ---- metasprite / metatile add ---- */

typedef struct UiUndoMetaspriteAdd {
    int world_idx;
    int idx;
    R01MetaspriteDef ms;
} UiUndoMetaspriteAdd;

static void metasprite_add_undo(UiState *ui, void *data) {
    UiUndoMetaspriteAdd *d = (UiUndoMetaspriteAdd *)data;
    if (!ui || !d || !ui->project) {
        return;
    }
    (void)r01_world_metasprite_remove(&ui->project->worlds[d->world_idx], d->idx);
}

static void metasprite_add_redo(UiState *ui, void *data) {
    UiUndoMetaspriteAdd *d = (UiUndoMetaspriteAdd *)data;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    idx = r01_world_metasprite_add(w);
    if (idx < 0) {
        return;
    }
    w->metasprites[idx] = d->ms;
    d->idx = idx;
}

static const UiUndoVTable metasprite_add_vt = {metasprite_add_undo, metasprite_add_redo, free_ptr};

void ui_undo_push_metasprite_add(UiState *ui, int meta_idx) {
    UiUndoMetaspriteAdd *d;
    R01World *w;
    if (!ui || !ui->project) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || meta_idx < 0 || meta_idx >= w->metasprite_count) {
        return;
    }
    d = (UiUndoMetaspriteAdd *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = meta_idx;
    d->ms = w->metasprites[meta_idx];
    (void)ui_undo_push(&ui->undo, &metasprite_add_vt, d, "add metasprite");
}

typedef struct UiUndoMetatileAdd {
    int world_idx;
    int idx;
    R01MetatileDef mt;
} UiUndoMetatileAdd;

static void metatile_add_undo(UiState *ui, void *data) {
    UiUndoMetatileAdd *d = (UiUndoMetatileAdd *)data;
    if (!ui || !d || !ui->project) {
        return;
    }
    (void)r01_world_metatile_remove(&ui->project->worlds[d->world_idx], d->idx);
}

static void metatile_add_redo(UiState *ui, void *data) {
    UiUndoMetatileAdd *d = (UiUndoMetatileAdd *)data;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    idx = r01_world_metatile_add(w);
    if (idx < 0) {
        return;
    }
    w->metatiles[idx] = d->mt;
    d->idx = idx;
}

static const UiUndoVTable metatile_add_vt = {metatile_add_undo, metatile_add_redo, free_ptr};

void ui_undo_push_metatile_add(UiState *ui, int mt_idx) {
    UiUndoMetatileAdd *d;
    R01World *w;
    if (!ui || !ui->project) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || mt_idx < 0 || mt_idx >= w->metatile_count) {
        return;
    }
    d = (UiUndoMetatileAdd *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = mt_idx;
    d->mt = w->metatiles[mt_idx];
    (void)ui_undo_push(&ui->undo, &metatile_add_vt, d, "add metatile");
}

/* ---- instance ---- */

typedef struct UiUndoInstance {
    int world_idx;
    int idx;
    R01EntityInstance inst;
} UiUndoInstance;

static void instance_add_undo(UiState *ui, void *data) {
    UiUndoInstance *d = (UiUndoInstance *)data;
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    (void)r01_world_instance_remove(w, d->idx);
    if (ui->sel_instance == d->idx) {
        ui->sel_instance = -1;
    } else if (ui->sel_instance > d->idx) {
        ui->sel_instance--;
    }
}

static void instance_add_redo(UiState *ui, void *data) {
    UiUndoInstance *d = (UiUndoInstance *)data;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    idx = r01_world_instance_add(w, d->inst.type_id, d->inst.world_x, d->inst.world_y);
    if (idx >= 0) {
        w->instances[idx].flip_h = d->inst.flip_h;
        w->instances[idx].flip_v = d->inst.flip_v;
        d->idx = idx;
    }
}

static const UiUndoVTable instance_add_vt = {instance_add_undo, instance_add_redo, free_ptr};

void ui_undo_push_instance_add(UiState *ui, int inst_idx) {
    UiUndoInstance *d;
    R01World *w;
    if (!ui || !ui->project) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w || inst_idx < 0 || inst_idx >= w->instance_count) {
        return;
    }
    d = (UiUndoInstance *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = inst_idx;
    d->inst = w->instances[inst_idx];
    (void)ui_undo_push(&ui->undo, &instance_add_vt, d, "place instance");
}

static void instance_remove_undo(UiState *ui, void *data) {
    instance_add_redo(ui, data);
}

static void instance_remove_redo(UiState *ui, void *data) {
    instance_add_undo(ui, data);
}

static const UiUndoVTable instance_remove_vt = {instance_remove_undo, instance_remove_redo, free_ptr};

void ui_undo_push_instance_remove(UiState *ui, int inst_idx, const R01EntityInstance *removed) {
    UiUndoInstance *d;
    if (!ui || !ui->project || !removed) {
        return;
    }
    d = (UiUndoInstance *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->idx = inst_idx;
    d->inst = *removed;
    (void)ui_undo_push(&ui->undo, &instance_remove_vt, d, "remove instance");
}

/* ---- screen create / remove / paste ---- */

typedef struct UiUndoScreenCreate {
    int world_idx;
    int plane;
    int col;
    int row;
} UiUndoScreenCreate;

static void screen_create_undo(UiState *ui, void *data) {
    UiUndoScreenCreate *d = (UiUndoScreenCreate *)data;
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->plane == UI_WORLDS_PLANE_BG0) {
        (void)r01_world_bg0_remove_screen(w, d->col, d->row);
    } else {
        (void)r01_world_remove_screen(w, d->col, d->row);
        r01_project_select_start_screen(ui->project);
    }
}

static void screen_create_redo(UiState *ui, void *data) {
    UiUndoScreenCreate *d = (UiUndoScreenCreate *)data;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->plane == UI_WORLDS_PLANE_BG0) {
        (void)r01_world_bg0_create_screen(w, d->col, d->row);
    } else {
        idx = r01_world_create_screen(w, d->col, d->row);
        if (idx >= 0) {
            ui->project->active_screen = idx;
        }
    }
}

static const UiUndoVTable screen_create_vt = {screen_create_undo, screen_create_redo, free_ptr};

void ui_undo_push_screen_create(UiState *ui, int plane, int col, int row) {
    UiUndoScreenCreate *d;
    if (!ui || !ui->project) {
        return;
    }
    d = (UiUndoScreenCreate *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->plane = plane;
    d->col = col;
    d->row = row;
    (void)ui_undo_push(&ui->undo, &screen_create_vt, d, "create screen");
}

typedef struct UiUndoScreenRemove {
    int world_idx;
    int plane;
    int col;
    int row;
    R01Screen screen;
} UiUndoScreenRemove;

static void screen_remove_undo(UiState *ui, void *data) {
    UiUndoScreenRemove *d = (UiUndoScreenRemove *)data;
    R01World *w;
    R01Screen *dst;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->plane == UI_WORLDS_PLANE_BG0) {
        idx = r01_world_bg0_create_screen(w, d->col, d->row);
        if (idx < 0) {
            return;
        }
        dst = &w->bg0_screens[idx];
        w->bg0_active_screen = idx;
    } else {
        idx = r01_world_create_screen(w, d->col, d->row);
        if (idx < 0) {
            return;
        }
        dst = &w->screens[idx];
        ui->project->active_screen = idx;
    }
    memcpy(dst->tiles, d->screen.tiles, sizeof(dst->tiles));
    memcpy(dst->attrs, d->screen.attrs, sizeof(dst->attrs));
    memcpy(dst->pixels, d->screen.pixels, sizeof(dst->pixels));
    dst->col = d->col;
    dst->row = d->row;
    dst->present = 1;
    r01_screen_fill_pixels_from_bank(w, dst);
}

static void screen_remove_redo(UiState *ui, void *data) {
    UiUndoScreenRemove *d = (UiUndoScreenRemove *)data;
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->plane == UI_WORLDS_PLANE_BG0) {
        (void)r01_world_bg0_remove_screen(w, d->col, d->row);
    } else {
        (void)r01_world_remove_screen(w, d->col, d->row);
        r01_project_select_start_screen(ui->project);
    }
}

static const UiUndoVTable screen_remove_vt = {screen_remove_undo, screen_remove_redo, free_ptr};

void ui_undo_push_screen_remove(UiState *ui, int plane, int col, int row, const R01Screen *removed) {
    UiUndoScreenRemove *d;
    if (!ui || !ui->project || !removed) {
        return;
    }
    d = (UiUndoScreenRemove *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->plane = plane;
    d->col = col;
    d->row = row;
    d->screen = *removed;
    (void)ui_undo_push(&ui->undo, &screen_remove_vt, d, "remove screen");
}

typedef struct UiUndoScreenPaste {
    int world_idx;
    int plane;
    int screen_idx;
    int col;
    int row;
    int created;
    R01Screen before;
    R01Screen after;
} UiUndoScreenPaste;

static void screen_paste_undo(UiState *ui, void *data) {
    UiUndoScreenPaste *d = (UiUndoScreenPaste *)data;
    R01World *w;
    R01Screen *s;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->created) {
        if (d->plane == UI_WORLDS_PLANE_BG0) {
            (void)r01_world_bg0_remove_screen(w, d->col, d->row);
        } else {
            (void)r01_world_remove_screen(w, d->col, d->row);
            r01_project_select_start_screen(ui->project);
        }
        return;
    }
    s = undo_screen_ptr(ui, d->world_idx, d->plane, d->screen_idx);
    if (!s) {
        return;
    }
    memcpy(s->tiles, d->before.tiles, sizeof(s->tiles));
    memcpy(s->attrs, d->before.attrs, sizeof(s->attrs));
    memcpy(s->pixels, d->before.pixels, sizeof(s->pixels));
    r01_screen_fill_pixels_from_bank(w, s);
}

static void screen_paste_redo(UiState *ui, void *data) {
    UiUndoScreenPaste *d = (UiUndoScreenPaste *)data;
    R01World *w;
    R01Screen *s;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->created) {
        if (d->plane == UI_WORLDS_PLANE_BG0) {
            idx = r01_world_bg0_create_screen(w, d->col, d->row);
            if (idx < 0) {
                return;
            }
            s = &w->bg0_screens[idx];
            w->bg0_active_screen = idx;
            d->screen_idx = idx;
        } else {
            idx = r01_world_create_screen(w, d->col, d->row);
            if (idx < 0) {
                return;
            }
            s = &w->screens[idx];
            ui->project->active_screen = idx;
            d->screen_idx = idx;
        }
    } else {
        s = undo_screen_ptr(ui, d->world_idx, d->plane, d->screen_idx);
        if (!s) {
            return;
        }
    }
    memcpy(s->tiles, d->after.tiles, sizeof(s->tiles));
    memcpy(s->attrs, d->after.attrs, sizeof(s->attrs));
    memcpy(s->pixels, d->after.pixels, sizeof(s->pixels));
    s->col = d->col;
    s->row = d->row;
    s->present = 1;
    r01_screen_fill_pixels_from_bank(w, s);
}

static const UiUndoVTable screen_paste_vt = {screen_paste_undo, screen_paste_redo, free_ptr};

void ui_undo_push_screen_paste(UiState *ui, int plane, int screen_idx, const R01Screen *before_or_null) {
    UiUndoScreenPaste *d;
    R01Screen *s;
    if (!ui || !ui->project) {
        return;
    }
    s = undo_screen_ptr(ui, ui->project->active_world, plane, screen_idx);
    if (!s) {
        return;
    }
    d = (UiUndoScreenPaste *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->plane = plane;
    d->screen_idx = screen_idx;
    d->col = s->col;
    d->row = s->row;
    d->created = (before_or_null == NULL);
    if (before_or_null) {
        d->before = *before_or_null;
    }
    d->after = *s;
    (void)ui_undo_push(&ui->undo, &screen_paste_vt, d, "paste screen");
}

/* ---- tile create ---- */

typedef struct UiUndoTileCreate {
    int world_idx;
    int bank;
    int tile_id;
    int old_tile_count;
    int painted;
    int plane;
    int screen_idx;
    int paint_tx;
    int paint_ty;
    uint8_t old_tile;
    uint8_t old_attr;
    uint8_t new_tile;
    uint8_t new_attr;
    uint8_t chr[R01_TILE_BYTES];
} UiUndoTileCreate;

static void tile_create_undo(UiState *ui, void *data) {
    UiUndoTileCreate *d = (UiUndoTileCreate *)data;
    R01World *w;
    R01Screen *s;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->painted) {
        s = undo_screen_ptr(ui, d->world_idx, d->plane, d->screen_idx);
        if (s) {
            r01_screen_paint_tile(w, s, d->paint_tx, d->paint_ty, d->old_tile, d->old_attr);
        }
    }
    if (d->bank >= 0 && d->bank < R01_BG_BANKS && d->old_tile_count >= 0 &&
        d->old_tile_count <= R01_TILES_PER_BANK) {
        w->bg_banks[d->bank].tile_count = d->old_tile_count;
    }
}

static void tile_create_redo(UiState *ui, void *data) {
    UiUndoTileCreate *d = (UiUndoTileCreate *)data;
    R01World *w;
    R01Screen *s;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    (void)r01_chr_write_tile(w, d->bank, d->tile_id, d->chr);
    if (d->painted) {
        s = undo_screen_ptr(ui, d->world_idx, d->plane, d->screen_idx);
        if (s) {
            r01_screen_paint_tile(w, s, d->paint_tx, d->paint_ty, d->new_tile, d->new_attr);
        }
    }
}

static const UiUndoVTable tile_create_vt = {tile_create_undo, tile_create_redo, free_ptr};

void ui_undo_push_tile_create(UiState *ui, int bank, int tile_id, int old_tile_count, int painted, int paint_tx,
                              int paint_ty, uint8_t old_tile, uint8_t old_attr, uint8_t new_tile,
                              uint8_t new_attr) {
    UiUndoTileCreate *d;
    R01World *w;
    int wi, plane, si;
    if (!ui || !ui->project) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w) {
        return;
    }
    d = (UiUndoTileCreate *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->bank = bank;
    d->tile_id = tile_id;
    d->old_tile_count = old_tile_count;
    d->painted = painted;
    d->paint_tx = paint_tx;
    d->paint_ty = paint_ty;
    d->old_tile = old_tile;
    d->old_attr = old_attr;
    d->new_tile = new_tile;
    d->new_attr = new_attr;
    if (undo_active_screen_ids(ui, &wi, &plane, &si)) {
        d->plane = plane;
        d->screen_idx = si;
    }
    if (bank >= 0 && bank < R01_BG_BANKS && tile_id >= 0 && tile_id < w->bg_banks[bank].tile_count) {
        memcpy(d->chr, w->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES, R01_TILE_BYTES);
    }
    (void)ui_undo_push(&ui->undo, &tile_create_vt, d, "add tile");
}

/* ---- sprite CHR paint stroke (entity / compose) ---- */

typedef struct UiUndoSprPaintTile {
    int bank;
    int tile_id;
    uint8_t old_chr[R01_TILE_BYTES];
    uint8_t new_chr[R01_TILE_BYTES];
} UiUndoSprPaintTile;

typedef struct UiUndoSprPaintStroke {
    int world_idx;
    UiUndoSprPaintTile *tiles;
    int count;
    int cap;
} UiUndoSprPaintStroke;

static void spr_paint_destroy(void *data) {
    UiUndoSprPaintStroke *st = (UiUndoSprPaintStroke *)data;
    if (!st) {
        return;
    }
    free(st->tiles);
    free(st);
}

static void spr_paint_apply(UiState *ui, UiUndoSprPaintStroke *st, int use_new) {
    R01World *w;
    int i;
    if (!ui || !st || !ui->project) {
        return;
    }
    if (st->world_idx < 0 || st->world_idx >= R01_MAX_WORLDS) {
        return;
    }
    w = &ui->project->worlds[st->world_idx];
    for (i = 0; i < st->count; i++) {
        UiUndoSprPaintTile *t = &st->tiles[i];
        (void)r01_chr_write_spr_tile(w, t->bank, t->tile_id, use_new ? t->new_chr : t->old_chr);
    }
}

static void spr_paint_undo(UiState *ui, void *data) {
    spr_paint_apply(ui, (UiUndoSprPaintStroke *)data, 0);
}

static void spr_paint_redo(UiState *ui, void *data) {
    spr_paint_apply(ui, (UiUndoSprPaintStroke *)data, 1);
}

static const UiUndoVTable spr_paint_vt = {spr_paint_undo, spr_paint_redo, spr_paint_destroy};

static int spr_paint_ensure_cap(UiUndoSprPaintStroke *st, int need) {
    UiUndoSprPaintTile *ntiles;
    int ncap;
    if (!st || need <= st->cap) {
        return 0;
    }
    ncap = st->cap < 4 ? 4 : st->cap * 2;
    while (ncap < need) {
        ncap *= 2;
    }
    ntiles = (UiUndoSprPaintTile *)realloc(st->tiles, (size_t)ncap * sizeof(*ntiles));
    if (!ntiles) {
        return -1;
    }
    st->tiles = ntiles;
    st->cap = ncap;
    return 0;
}

static int spr_paint_find(const UiUndoSprPaintStroke *st, int bank, int tile_id) {
    int i;
    if (!st) {
        return -1;
    }
    for (i = 0; i < st->count; i++) {
        if (st->tiles[i].bank == bank && st->tiles[i].tile_id == tile_id) {
            return i;
        }
    }
    return -1;
}

int ui_undo_spr_paint_begin(UiState *ui) {
    UiUndoSprPaintStroke *st;
    if (!ui || !ui->project) {
        return -1;
    }
    ui_undo_spr_paint_end(ui);
    st = (UiUndoSprPaintStroke *)calloc(1, sizeof(*st));
    if (!st) {
        return -1;
    }
    st->world_idx = ui->project->active_world;
    ui->undo_spr_paint = st;
    return 0;
}

void ui_undo_spr_paint_end(UiState *ui) {
    UiUndoSprPaintStroke *st;
    R01World *w;
    int i;
    int changed = 0;
    if (!ui || !ui->undo_spr_paint) {
        return;
    }
    st = (UiUndoSprPaintStroke *)ui->undo_spr_paint;
    ui->undo_spr_paint = NULL;
    if (st->count < 1 || !ui->project || st->world_idx < 0 || st->world_idx >= R01_MAX_WORLDS) {
        spr_paint_destroy(st);
        return;
    }
    w = &ui->project->worlds[st->world_idx];
    for (i = 0; i < st->count; i++) {
        const uint8_t *src = r01_chr_spr_tile(w, st->tiles[i].bank, st->tiles[i].tile_id);
        if (src) {
            memcpy(st->tiles[i].new_chr, src, R01_TILE_BYTES);
        }
        if (memcmp(st->tiles[i].old_chr, st->tiles[i].new_chr, R01_TILE_BYTES) != 0) {
            changed = 1;
        }
    }
    if (!changed) {
        spr_paint_destroy(st);
        return;
    }
    if (ui_undo_push(&ui->undo, &spr_paint_vt, st, "paint sprite") != 0) {
        spr_paint_destroy(st);
    }
}

void ui_undo_spr_paint_touch_tile(UiState *ui, int bank, int tile_id) {
    UiUndoSprPaintStroke *st;
    R01World *w;
    const uint8_t *src;
    int idx;
    if (!ui || !ui->undo_spr_paint || !ui->project) {
        return;
    }
    st = (UiUndoSprPaintStroke *)ui->undo_spr_paint;
    if (st->world_idx < 0 || st->world_idx >= R01_MAX_WORLDS) {
        return;
    }
    w = &ui->project->worlds[st->world_idx];
    idx = spr_paint_find(st, bank, tile_id);
    if (idx >= 0) {
        return;
    }
    src = r01_chr_spr_tile(w, bank, tile_id);
    if (!src) {
        return;
    }
    if (spr_paint_ensure_cap(st, st->count + 1) != 0) {
        return;
    }
    st->tiles[st->count].bank = bank;
    st->tiles[st->count].tile_id = tile_id;
    memcpy(st->tiles[st->count].old_chr, src, R01_TILE_BYTES);
    memcpy(st->tiles[st->count].new_chr, src, R01_TILE_BYTES);
    st->count++;
}
