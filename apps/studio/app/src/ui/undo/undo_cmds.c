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
        (void)r01_project_set_player_entity(ui->project, w, idx);
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
        (void)r01_project_set_player_entity(ui->project, w, d->idx);
    } else if (w->player_entity >= d->idx) {
        w->player_entity++;
    }
}

static void entity_remove_redo(UiState *ui, void *data) {
    UiUndoEntityRemove *d = (UiUndoEntityRemove *)data;
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->was_player) {
        (void)r01_project_set_player_entity(ui->project, w, -1);
    }
    (void)r01_world_entity_remove(w, d->idx);
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

/* ---- tile create / BG CHR edit ---- */

static void undo_refresh_world_screens(R01World *w) {
    int si;
    if (!w) {
        return;
    }
    for (si = 0; si < w->screen_count; si++) {
        if (w->screens[si].present) {
            r01_screen_fill_pixels_from_bank(w, &w->screens[si]);
        }
    }
    for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
        if (w->bg0_screens[si].present) {
            r01_screen_fill_pixels_from_bank(w, &w->bg0_screens[si]);
        }
    }
}

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
    undo_refresh_world_screens(w);
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
    undo_refresh_world_screens(w);
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

/* BG CHR edit: snapshot old/new tile bytes; refresh every screen preview from banks. */
typedef struct UiUndoBgChrEdit {
    int world_idx;
    int bank;
    int tile_id;
    uint8_t old_chr[R01_TILE_BYTES];
    uint8_t new_chr[R01_TILE_BYTES];
} UiUndoBgChrEdit;

static void bg_chr_edit_apply(UiState *ui, UiUndoBgChrEdit *d, int use_new) {
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    if (d->world_idx < 0 || d->world_idx >= R01_MAX_WORLDS) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    (void)r01_chr_write_tile(w, d->bank, d->tile_id, use_new ? d->new_chr : d->old_chr);
    undo_refresh_world_screens(w);
}

static void bg_chr_edit_undo(UiState *ui, void *data) {
    bg_chr_edit_apply(ui, (UiUndoBgChrEdit *)data, 0);
}

static void bg_chr_edit_redo(UiState *ui, void *data) {
    bg_chr_edit_apply(ui, (UiUndoBgChrEdit *)data, 1);
}

static const UiUndoVTable bg_chr_edit_vt = {bg_chr_edit_undo, bg_chr_edit_redo, free_ptr};

void ui_undo_push_bg_chr_edit(UiState *ui, int bank, int tile_id, const uint8_t old_chr[R01_TILE_BYTES],
                              const uint8_t new_chr[R01_TILE_BYTES]) {
    UiUndoBgChrEdit *d;
    if (!ui || !ui->project || !old_chr || !new_chr) {
        return;
    }
    if (memcmp(old_chr, new_chr, R01_TILE_BYTES) == 0) {
        return;
    }
    d = (UiUndoBgChrEdit *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->bank = bank;
    d->tile_id = tile_id;
    memcpy(d->old_chr, old_chr, R01_TILE_BYTES);
    memcpy(d->new_chr, new_chr, R01_TILE_BYTES);
    if (ui_undo_push(&ui->undo, &bg_chr_edit_vt, d, "edit tile") != 0) {
        free(d);
    }
}

/* ---- other SPR CHR edit ---- */

typedef struct UiUndoPlayerChrEdit {
    int bank;
    int tile_id;
    uint8_t old_chr[R01_TILE_BYTES];
    uint8_t new_chr[R01_TILE_BYTES];
} UiUndoPlayerChrEdit;

static void player_chr_edit_apply(UiState *ui, UiUndoPlayerChrEdit *d, int use_new) {
    if (!ui || !d || !ui->project) {
        return;
    }
    (void)r01_other_spr_write_tile(ui->project, d->bank, d->tile_id, use_new ? d->new_chr : d->old_chr);
}

static void player_chr_edit_undo(UiState *ui, void *data) {
    player_chr_edit_apply(ui, (UiUndoPlayerChrEdit *)data, 0);
}

static void player_chr_edit_redo(UiState *ui, void *data) {
    player_chr_edit_apply(ui, (UiUndoPlayerChrEdit *)data, 1);
}

static const UiUndoVTable player_chr_edit_vt = {player_chr_edit_undo, player_chr_edit_redo, free_ptr};

void ui_undo_push_player_chr_edit(UiState *ui, int bank, int tile_id, const uint8_t old_chr[R01_TILE_BYTES],
                                  const uint8_t new_chr[R01_TILE_BYTES]) {
    UiUndoPlayerChrEdit *d;
    if (!ui || !ui->project || !old_chr || !new_chr) {
        return;
    }
    if (memcmp(old_chr, new_chr, R01_TILE_BYTES) == 0) {
        return;
    }
    d = (UiUndoPlayerChrEdit *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->bank = bank;
    d->tile_id = tile_id;
    memcpy(d->old_chr, old_chr, R01_TILE_BYTES);
    memcpy(d->new_chr, new_chr, R01_TILE_BYTES);
    if (ui_undo_push(&ui->undo, &player_chr_edit_vt, d, "edit other SPR tile") != 0) {
        free(d);
    }
}

/* ---- bank tile / sprite remove (source CHR + remap refs to tile 0) ---- */

typedef struct UiUndoBankTileRef {
    uint8_t screen_plane; /* UI_WORLDS_PLANE_* */
    uint8_t screen_idx;
    uint16_t cell;
    uint8_t old_tile;
} UiUndoBankTileRef;

typedef struct UiUndoBankPartRef {
    uint8_t kind; /* 0=entity 1=metasprite 3=metatile */
    int16_t a;    /* entity/meta/metatile idx */
    int16_t b;    /* state or corner */
    int16_t c;    /* frame */
    int16_t d;    /* part */
    uint8_t old_tile;
} UiUndoBankPartRef;

typedef struct UiUndoBankTileRemove {
    int world_idx;
    int bank_plane;
    int bank;
    int tile_id;
    int old_tile_count;
    uint8_t old_chr[R01_TILE_BYTES];
    UiUndoBankTileRef *refs;
    int ref_count;
    UiUndoBankPartRef *parts;
    int part_count;
    R01SpriteDef *removed_cats;
    int removed_cat_count;
} UiUndoBankTileRemove;

static void bank_tile_remove_destroy(void *data) {
    UiUndoBankTileRemove *d = (UiUndoBankTileRemove *)data;
    if (!d) {
        return;
    }
    free(d->refs);
    free(d->parts);
    free(d->removed_cats);
    free(d);
}

static int bank_tile_remove_write_chr(UiState *ui, UiUndoBankTileRemove *d, const uint8_t *chr) {
    R01World *w;
    if (!ui || !d || !chr || !ui->project) {
        return -1;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->bank_plane == UI_BANKS_PLANE_OTHER_SPR) {
        return r01_other_spr_write_tile(ui->project, d->bank, d->tile_id, chr);
    }
    if (d->bank_plane == UI_BANKS_PLANE_SPR) {
        return r01_chr_write_spr_tile(w, d->bank, d->tile_id, chr);
    }
    return r01_chr_write_tile(w, d->bank, d->tile_id, chr);
}

static void bank_tile_remove_apply_refs(UiState *ui, UiUndoBankTileRemove *d, int use_new) {
    R01World *w;
    int i;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    for (i = 0; i < d->ref_count; i++) {
        UiUndoBankTileRef *r = &d->refs[i];
        R01Screen *s = undo_screen_ptr(ui, d->world_idx, r->screen_plane, r->screen_idx);
        int tx, ty;
        uint8_t tile;
        if (!s || !s->present) {
            continue;
        }
        tx = (int)(r->cell % R01_SCREEN_TILES_X);
        ty = (int)(r->cell / R01_SCREEN_TILES_X);
        tile = use_new ? 0 : r->old_tile;
        r01_screen_paint_tile(w, s, tx, ty, tile, s->attrs[r->cell]);
    }
    for (i = 0; i < d->part_count; i++) {
        UiUndoBankPartRef *p = &d->parts[i];
        uint8_t tile = use_new ? 0 : p->old_tile;
        if (p->kind == 0) {
            if (p->a >= 0 && p->a < w->entity_count && p->b >= 0 && p->b < w->entities[p->a].state_count &&
                p->c >= 0 && p->c < w->entities[p->a].states[p->b].frame_count && p->d >= 0 &&
                p->d < w->entities[p->a].states[p->b].frames[p->c].part_count) {
                w->entities[p->a].states[p->b].frames[p->c].parts[p->d].tile_id = tile;
            }
        } else if (p->kind == 1) {
            if (p->a >= 0 && p->a < w->metasprite_count && p->d >= 0 &&
                p->d < w->metasprites[p->a].frame.part_count) {
                w->metasprites[p->a].frame.parts[p->d].tile_id = tile;
            }
        } else if (p->kind == 3) {
            if (p->a >= 0 && p->a < w->metatile_count && p->b >= 0 && p->b < 4) {
                w->metatiles[p->a].tile[p->b] = tile;
            }
        }
    }
    if (use_new) {
        /* Drop catalog entries that pointed at this pattern. */
        for (i = w->sprite_count - 1; i >= 0; i--) {
            int match = 0;
            if (d->bank_plane == UI_BANKS_PLANE_OTHER_SPR) {
                match = w->sprites[i].bank == (R01_GLOBAL_SPR_BANK_BASE + d->bank) &&
                        w->sprites[i].tile_id == d->tile_id;
            } else if (d->bank_plane == UI_BANKS_PLANE_SPR) {
                match = w->sprites[i].bank == d->bank && w->sprites[i].tile_id == d->tile_id;
            }
            if (match) {
                (void)r01_world_sprite_remove(w, i);
            }
        }
    } else {
        for (i = 0; i < d->removed_cat_count; i++) {
            const R01SpriteDef *s = &d->removed_cats[i];
            (void)r01_world_sprite_add(w, s->bank, s->tile_id, s->pal);
        }
    }
}

static int bank_tile_chr_is_blank(const uint8_t *tile) {
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

static void bank_tile_trim_count(UiState *ui, UiUndoBankTileRemove *d) {
    R01World *w;
    if (!ui || !d || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    if (d->bank_plane == UI_BANKS_PLANE_OTHER_SPR && d->bank >= 0 && d->bank < R01_SPR_BANKS) {
        R01ChrBank *b = &ui->project->other_spr_banks[d->bank];
        while (b->tile_count > 0) {
            int id = b->tile_count - 1;
            const uint8_t *t = b->chr + (size_t)id * R01_TILE_BYTES;
            if (!bank_tile_chr_is_blank(t)) {
                break;
            }
            b->tile_count--;
        }
    } else if (d->bank_plane == UI_BANKS_PLANE_SPR && d->bank >= 0 && d->bank < R01_SPR_BANKS) {
        R01ChrBank *b = &w->spr_banks[d->bank];
        while (b->tile_count > 0) {
            int id = b->tile_count - 1;
            const uint8_t *t = b->chr + (size_t)id * R01_TILE_BYTES;
            if (!bank_tile_chr_is_blank(t)) {
                break;
            }
            b->tile_count--;
        }
    } else if (d->bank_plane == UI_BANKS_PLANE_BG && d->bank >= 0 && d->bank < R01_BG_BANKS) {
        R01ChrBank *b = &w->bg_banks[d->bank];
        while (b->tile_count > 0) {
            int id = b->tile_count - 1;
            const uint8_t *t = b->chr + (size_t)id * R01_TILE_BYTES;
            if (!bank_tile_chr_is_blank(t)) {
                break;
            }
            b->tile_count--;
        }
    }
}

static void bank_tile_remove_apply(UiState *ui, UiUndoBankTileRemove *d, int use_new) {
    uint8_t blank[R01_TILE_BYTES];
    R01World *w;
    if (!d || !ui || !ui->project) {
        return;
    }
    w = &ui->project->worlds[d->world_idx];
    memset(blank, 0, sizeof(blank));
    (void)bank_tile_remove_write_chr(ui, d, use_new ? blank : d->old_chr);
    if (!use_new) {
        if (d->bank_plane == UI_BANKS_PLANE_OTHER_SPR && d->bank >= 0 && d->bank < R01_SPR_BANKS) {
            if (d->old_tile_count > ui->project->other_spr_banks[d->bank].tile_count) {
                ui->project->other_spr_banks[d->bank].tile_count = d->old_tile_count;
            }
        } else if (d->bank_plane == UI_BANKS_PLANE_SPR && d->bank >= 0 && d->bank < R01_SPR_BANKS) {
            if (d->old_tile_count > w->spr_banks[d->bank].tile_count) {
                w->spr_banks[d->bank].tile_count = d->old_tile_count;
            }
        } else if (d->bank_plane == UI_BANKS_PLANE_BG && d->bank >= 0 && d->bank < R01_BG_BANKS) {
            if (d->old_tile_count > w->bg_banks[d->bank].tile_count) {
                w->bg_banks[d->bank].tile_count = d->old_tile_count;
            }
        }
    }
    bank_tile_remove_apply_refs(ui, d, use_new);
    if (use_new) {
        bank_tile_trim_count(ui, d);
        if (d->bank_plane == UI_BANKS_PLANE_OTHER_SPR) {
            r01_project_densify_other_spr_bank(ui->project, d->bank);
        } else if (d->bank_plane == UI_BANKS_PLANE_SPR) {
            r01_chr_densify_spr_bank(w, d->bank);
        } else if (d->bank_plane == UI_BANKS_PLANE_BG) {
            r01_chr_densify_bg_bank(w, d->bank);
        }
    }
    if (d->bank_plane == UI_BANKS_PLANE_BG) {
        undo_refresh_world_screens(w);
    }
}

static void bank_tile_remove_undo(UiState *ui, void *data) {
    bank_tile_remove_apply(ui, (UiUndoBankTileRemove *)data, 0);
}

static void bank_tile_remove_redo(UiState *ui, void *data) {
    bank_tile_remove_apply(ui, (UiUndoBankTileRemove *)data, 1);
}

static const UiUndoVTable bank_tile_remove_vt = {bank_tile_remove_undo, bank_tile_remove_redo,
                                                bank_tile_remove_destroy};

static int bank_tile_remove_push_ref(UiUndoBankTileRemove *d, int screen_plane, int screen_idx, int cell,
                                     uint8_t old_tile) {
    UiUndoBankTileRef *n;
    if (!d) {
        return -1;
    }
    n = (UiUndoBankTileRef *)realloc(d->refs, (size_t)(d->ref_count + 1) * sizeof(*n));
    if (!n) {
        return -1;
    }
    d->refs = n;
    d->refs[d->ref_count].screen_plane = (uint8_t)screen_plane;
    d->refs[d->ref_count].screen_idx = (uint8_t)screen_idx;
    d->refs[d->ref_count].cell = (uint16_t)cell;
    d->refs[d->ref_count].old_tile = old_tile;
    d->ref_count++;
    return 0;
}

static int bank_tile_remove_push_part(UiUndoBankTileRemove *d, uint8_t kind, int a, int b, int c, int part,
                                      uint8_t old_tile) {
    UiUndoBankPartRef *n;
    if (!d) {
        return -1;
    }
    n = (UiUndoBankPartRef *)realloc(d->parts, (size_t)(d->part_count + 1) * sizeof(*n));
    if (!n) {
        return -1;
    }
    d->parts = n;
    d->parts[d->part_count].kind = kind;
    d->parts[d->part_count].a = (int16_t)a;
    d->parts[d->part_count].b = (int16_t)b;
    d->parts[d->part_count].c = (int16_t)c;
    d->parts[d->part_count].d = (int16_t)part;
    d->parts[d->part_count].old_tile = old_tile;
    d->part_count++;
    return 0;
}

static int bank_tile_matches_part(int bank_plane, int bank, int tile_id, int part_bank, int part_tile) {
    if (part_tile != tile_id) {
        return 0;
    }
    if (bank_plane == UI_BANKS_PLANE_OTHER_SPR) {
        return part_bank == (R01_GLOBAL_SPR_BANK_BASE + bank);
    }
    if (bank_plane == UI_BANKS_PLANE_SPR) {
        return part_bank == bank;
    }
    return 0;
}

void ui_undo_push_bank_tile_remove(UiState *ui, int bank_plane, int bank, int tile_id) {
    UiUndoBankTileRemove *d;
    R01World *w;
    const uint8_t *src;
    uint8_t blank[R01_TILE_BYTES];
    int si, ei, mi, ci, corner;
    int wi;

    if (!ui || !ui->project || tile_id < 0 || tile_id >= R01_TILES_PER_BANK) {
        return;
    }
    if (bank_plane == UI_BANKS_PLANE_BG && tile_id == 0) {
        ui_toast(ui, "cannot remove tile 0", 1);
        return;
    }
    wi = ui->project->active_world;
    if (wi < 0 || wi >= R01_MAX_WORLDS) {
        return;
    }
    w = &ui->project->worlds[wi];
    if (bank_plane == UI_BANKS_PLANE_OTHER_SPR) {
        src = r01_other_spr_tile(ui->project, bank, tile_id);
    } else if (bank_plane == UI_BANKS_PLANE_SPR) {
        src = r01_chr_spr_tile(w, bank, tile_id);
    } else {
        if (bank < 0 || bank >= R01_BG_BANKS || tile_id >= w->bg_banks[bank].tile_count) {
            return;
        }
        src = w->bg_banks[bank].chr + (size_t)tile_id * R01_TILE_BYTES;
    }
    if (!src) {
        return;
    }

    d = (UiUndoBankTileRemove *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = wi;
    d->bank_plane = bank_plane;
    d->bank = bank;
    d->tile_id = tile_id;
    memcpy(d->old_chr, src, R01_TILE_BYTES);
    if (bank_plane == UI_BANKS_PLANE_OTHER_SPR) {
        d->old_tile_count = ui->project->other_spr_banks[bank].tile_count;
    } else if (bank_plane == UI_BANKS_PLANE_SPR) {
        d->old_tile_count = w->spr_banks[bank].tile_count;
    } else {
        d->old_tile_count = w->bg_banks[bank].tile_count;
    }

    if (bank_plane == UI_BANKS_PLANE_BG) {
        for (si = 0; si < w->screen_count; si++) {
            R01Screen *s = &w->screens[si];
            int cell;
            if (!s->present) {
                continue;
            }
            for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
                if (s->tiles[cell] == (uint8_t)tile_id && r01_attr_bank(s->attrs[cell]) == bank) {
                    if (bank_tile_remove_push_ref(d, UI_WORLDS_PLANE_BG1, si, cell, s->tiles[cell]) != 0) {
                        bank_tile_remove_destroy(d);
                        return;
                    }
                }
            }
        }
        for (si = 0; si < w->bg0_screen_count && si < R01_BG0_SCREENS_MAX; si++) {
            R01Screen *s = &w->bg0_screens[si];
            int cell;
            if (!s->present) {
                continue;
            }
            for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
                if (s->tiles[cell] == (uint8_t)tile_id && r01_attr_bank(s->attrs[cell]) == bank) {
                    if (bank_tile_remove_push_ref(d, UI_WORLDS_PLANE_BG0, si, cell, s->tiles[cell]) != 0) {
                        bank_tile_remove_destroy(d);
                        return;
                    }
                }
            }
        }
        for (mi = 0; mi < w->metatile_count; mi++) {
            for (corner = 0; corner < 4; corner++) {
                if (w->metatiles[mi].tile[corner] == (uint8_t)tile_id &&
                    r01_attr_bank(w->metatiles[mi].attr[corner]) == bank) {
                    if (bank_tile_remove_push_part(d, 3, mi, corner, 0, 0, w->metatiles[mi].tile[corner]) != 0) {
                        bank_tile_remove_destroy(d);
                        return;
                    }
                }
            }
        }
    } else {
        for (ei = 0; ei < w->entity_count; ei++) {
            R01EntityType *ent = &w->entities[ei];
            int sti, fi, pi;
            for (sti = 0; sti < ent->state_count && sti < R01_ENTITY_STATES_MAX; sti++) {
                for (fi = 0; fi < ent->states[sti].frame_count && fi < R01_ENTITY_FRAMES_MAX; fi++) {
                    R01EntityFrame *fr = &ent->states[sti].frames[fi];
                    for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                        if (bank_tile_matches_part(bank_plane, bank, tile_id, fr->parts[pi].bank,
                                                   fr->parts[pi].tile_id)) {
                            if (bank_tile_remove_push_part(d, 0, ei, sti, fi, pi, (uint8_t)fr->parts[pi].tile_id) !=
                                0) {
                                bank_tile_remove_destroy(d);
                                return;
                            }
                        }
                    }
                }
            }
        }
        for (mi = 0; mi < w->metasprite_count; mi++) {
            R01EntityFrame *fr = &w->metasprites[mi].frame;
            int pi;
            for (pi = 0; pi < fr->part_count && pi < R01_ENTITY_PARTS_MAX; pi++) {
                if (bank_tile_matches_part(bank_plane, bank, tile_id, fr->parts[pi].bank, fr->parts[pi].tile_id)) {
                    if (bank_tile_remove_push_part(d, 1, mi, 0, 0, pi, (uint8_t)fr->parts[pi].tile_id) != 0) {
                        bank_tile_remove_destroy(d);
                        return;
                    }
                }
            }
        }
        for (ci = w->sprite_count - 1; ci >= 0; ci--) {
            if (bank_tile_matches_part(bank_plane, bank, tile_id, w->sprites[ci].bank, w->sprites[ci].tile_id)) {
                R01SpriteDef *n = (R01SpriteDef *)realloc(d->removed_cats,
                                                          (size_t)(d->removed_cat_count + 1) * sizeof(*n));
                if (!n) {
                    bank_tile_remove_destroy(d);
                    return;
                }
                d->removed_cats = n;
                d->removed_cats[d->removed_cat_count++] = w->sprites[ci];
                (void)r01_world_sprite_remove(w, ci);
            }
        }
    }

    memset(blank, 0, sizeof(blank));
    if (bank_tile_remove_write_chr(ui, d, blank) != 0) {
        bank_tile_remove_destroy(d);
        ui_toast(ui, "cannot remove tile", 1);
        return;
    }
    bank_tile_remove_apply_refs(ui, d, 1);
    if (bank_plane == UI_BANKS_PLANE_BG) {
        undo_refresh_world_screens(w);
    }
    if (ui_undo_push(&ui->undo, &bank_tile_remove_vt, d,
                     bank_plane == UI_BANKS_PLANE_SPR ? "remove sprite" : "remove tile") != 0) {
        bank_tile_remove_destroy(d);
        return;
    }
    if (ui->bank_sel_tile == tile_id && ui->bank_sel_plane == bank_plane &&
        (bank_plane == UI_BANKS_PLANE_OTHER_SPR || ui->bank_sel_bank == bank)) {
        bank_sel_clear(ui);
    }
    ui_toast(ui, bank_plane == UI_BANKS_PLANE_SPR ? "sprite removed" : "tile removed", 0);
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
        (void)r01_chr_write_resolved_spr(ui->project, w, t->bank, t->tile_id, use_new ? t->new_chr : t->old_chr);
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
        const uint8_t *src = r01_chr_resolve_spr(ui->project, w, st->tiles[i].bank, st->tiles[i].tile_id);
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
    src = r01_chr_resolve_spr(ui->project, w, bank, tile_id);
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

/* ---- entity compose part add / remove ---- */

typedef struct UiUndoEntityPart {
    int world_idx;
    int state;
    int frame;
    int part_idx;
    R01EntityPart part;
    int owns_catalog; /* 1 if add also created a catalog sprite (matched by bank/tile) */
    R01SpriteDef spr;
} UiUndoEntityPart;

static R01EntityFrame *undo_entity_edit_frame(UiState *ui, int state, int frame) {
    if (!ui || !ui->entity_edit.open) {
        return NULL;
    }
    return r01_entity_ensure_frame(&ui->entity_edit.draft, state, frame);
}

static void undo_entity_edit_guides(UiState *ui, int state) {
    R01EntityState *st;
    if (!ui || !ui->entity_edit.open) {
        return;
    }
    st = r01_entity_state(&ui->entity_edit.draft, state);
    if (st) {
        r01_entity_state_recompute_guides(st);
    }
}

static int undo_frame_insert_part(R01EntityFrame *fr, int idx, const R01EntityPart *part) {
    int i;
    if (!fr || !part || fr->part_count >= R01_ENTITY_PARTS_MAX) {
        return -1;
    }
    if (idx < 0) {
        idx = 0;
    }
    if (idx > fr->part_count) {
        idx = fr->part_count;
    }
    for (i = fr->part_count; i > idx; i--) {
        fr->parts[i] = fr->parts[i - 1];
    }
    fr->parts[idx] = *part;
    fr->part_count++;
    return idx;
}

static int undo_find_sprite_catalog(const R01World *w, int bank, int tile_id) {
    int i;
    if (!w) {
        return -1;
    }
    for (i = 0; i < w->sprite_count; i++) {
        if (w->sprites[i].bank == bank && w->sprites[i].tile_id == tile_id) {
            return i;
        }
    }
    return -1;
}

static void entity_part_add_undo(UiState *ui, void *data) {
    UiUndoEntityPart *d = (UiUndoEntityPart *)data;
    R01EntityFrame *fr;
    R01World *w;
    int cat;
    if (!ui || !d || !ui->project) {
        return;
    }
    fr = undo_entity_edit_frame(ui, d->state, d->frame);
    if (fr) {
        (void)r01_entity_frame_remove_part(fr, d->part_idx);
        if (ui->entity_edit.sel_part == d->part_idx) {
            ui->entity_edit.sel_part = -1;
        } else if (ui->entity_edit.sel_part > d->part_idx) {
            ui->entity_edit.sel_part--;
        }
        undo_entity_edit_guides(ui, d->state);
    }
    if (d->owns_catalog) {
        w = &ui->project->worlds[d->world_idx];
        cat = undo_find_sprite_catalog(w, d->spr.bank, d->spr.tile_id);
        if (cat >= 0) {
            (void)r01_world_sprite_remove(w, cat);
        }
    }
}

static void entity_part_add_redo(UiState *ui, void *data) {
    UiUndoEntityPart *d = (UiUndoEntityPart *)data;
    R01EntityFrame *fr;
    R01World *w;
    int idx;
    if (!ui || !d || !ui->project) {
        return;
    }
    fr = undo_entity_edit_frame(ui, d->state, d->frame);
    if (fr) {
        idx = undo_frame_insert_part(fr, d->part_idx, &d->part);
        if (idx >= 0) {
            d->part_idx = idx;
            ui->entity_edit.sel_part = idx;
            undo_entity_edit_guides(ui, d->state);
        }
    }
    if (d->owns_catalog) {
        w = &ui->project->worlds[d->world_idx];
        if (undo_find_sprite_catalog(w, d->spr.bank, d->spr.tile_id) < 0) {
            (void)r01_world_sprite_add(w, d->spr.bank, d->spr.tile_id, d->spr.pal);
        }
    }
}

static const UiUndoVTable entity_part_add_vt = {entity_part_add_undo, entity_part_add_redo, free_ptr};

void ui_undo_push_entity_part_add(UiState *ui, int state, int frame, int part_idx, const R01EntityPart *part,
                                  int catalog_idx) {
    UiUndoEntityPart *d;
    R01World *w;
    if (!ui || !ui->project || !part) {
        return;
    }
    w = r01_project_active_world(ui->project);
    if (!w) {
        return;
    }
    d = (UiUndoEntityPart *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project->active_world;
    d->state = state;
    d->frame = frame;
    d->part_idx = part_idx;
    d->part = *part;
    d->owns_catalog = (catalog_idx >= 0);
    if (catalog_idx >= 0 && catalog_idx < w->sprite_count) {
        d->spr = w->sprites[catalog_idx];
    } else {
        d->spr.bank = part->bank;
        d->spr.tile_id = part->tile_id;
        d->spr.pal = part->pal;
        d->owns_catalog = 0;
    }
    (void)ui_undo_push(&ui->undo, &entity_part_add_vt, d, "add sprite");
}

static void entity_part_remove_undo(UiState *ui, void *data) {
    UiUndoEntityPart *d = (UiUndoEntityPart *)data;
    R01EntityFrame *fr;
    int idx;
    if (!ui || !d) {
        return;
    }
    fr = undo_entity_edit_frame(ui, d->state, d->frame);
    if (!fr) {
        return;
    }
    idx = undo_frame_insert_part(fr, d->part_idx, &d->part);
    if (idx >= 0) {
        d->part_idx = idx;
        ui->entity_edit.sel_part = idx;
        undo_entity_edit_guides(ui, d->state);
    }
}

static void entity_part_remove_redo(UiState *ui, void *data) {
    UiUndoEntityPart *d = (UiUndoEntityPart *)data;
    R01EntityFrame *fr;
    if (!ui || !d) {
        return;
    }
    fr = undo_entity_edit_frame(ui, d->state, d->frame);
    if (!fr) {
        return;
    }
    (void)r01_entity_frame_remove_part(fr, d->part_idx);
    if (ui->entity_edit.sel_part == d->part_idx) {
        ui->entity_edit.sel_part = -1;
    } else if (ui->entity_edit.sel_part > d->part_idx) {
        ui->entity_edit.sel_part--;
    }
    undo_entity_edit_guides(ui, d->state);
}

static const UiUndoVTable entity_part_remove_vt = {entity_part_remove_undo, entity_part_remove_redo, free_ptr};

void ui_undo_push_entity_part_remove(UiState *ui, int state, int frame, int part_idx, const R01EntityPart *removed) {
    UiUndoEntityPart *d;
    if (!ui || !removed) {
        return;
    }
    d = (UiUndoEntityPart *)calloc(1, sizeof(*d));
    if (!d) {
        return;
    }
    d->world_idx = ui->project ? ui->project->active_world : 0;
    d->state = state;
    d->frame = frame;
    d->part_idx = part_idx;
    d->part = *removed;
    d->owns_catalog = 0;
    (void)ui_undo_push(&ui->undo, &entity_part_remove_vt, d, "remove sprite");
}
