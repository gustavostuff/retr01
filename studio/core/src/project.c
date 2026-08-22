#include "retr01_studio/project.h"
#include "retr01_studio/palette.h"

#include <string.h>

void r01_project_init(R01Project *p, const char *name) {
    int wi, i;
    memset(p, 0, sizeof(*p));
    if (name && name[0]) {
        strncpy(p->name, name, R01_NAME_MAX - 1);
    } else {
        strncpy(p->name, "untitled", R01_NAME_MAX - 1);
    }
    p->active_world = 0;
    p->active_screen = -1;
    p->active_plane = -1;
    p->generate_bank = 0;
    p->paint_color = 3;
    r01_project_init_default_pals(p);
    p->worlds[0].present = 1;
    for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
        p->worlds[wi].default_bg_bank = 0;
        p->worlds[wi].default_pal_row = 0;
        p->worlds[wi].use_world_pals = 0;
        for (i = 0; i < R01_PAL_ROWS; i++) {
            r01_pal_row_init_default(&p->worlds[wi].pal_bg[i], i);
            r01_pal_row_init_default(&p->worlds[wi].pal_spr[i], i);
        }
        for (i = 0; i < R01_MAX_PARALLAX_PLANES; i++) {
            p->worlds[wi].planes[i].present = 0;
            p->worlds[wi].planes[i].slot = i;
        }
    }
}

R01World *r01_project_world(R01Project *p, int world_index) {
    if (!p || world_index < 0 || world_index >= R01_MAX_WORLDS) {
        return NULL;
    }
    return &p->worlds[world_index];
}

R01Screen *r01_project_active_screen(R01Project *p) {
    R01World *w;
    if (!p || p->active_plane >= 0 || p->active_screen < 0) {
        return NULL;
    }
    w = r01_project_world(p, p->active_world);
    if (!w || !w->present || p->active_screen >= w->screen_count) {
        return NULL;
    }
    if (!w->screens[p->active_screen].present) {
        return NULL;
    }
    return &w->screens[p->active_screen];
}

R01ParallaxPlane *r01_project_active_plane(R01Project *p) {
    R01World *w;
    if (!p || p->active_plane < 0 || p->active_plane >= R01_MAX_PARALLAX_PLANES) {
        return NULL;
    }
    w = r01_project_world(p, p->active_world);
    if (!w || !w->present) {
        return NULL;
    }
    if (!w->planes[p->active_plane].present) {
        return NULL;
    }
    return &w->planes[p->active_plane];
}

int r01_project_edit_surface(R01Project *p, R01EditSurface *out) {
    R01ParallaxPlane *pl;
    R01Screen *s;
    if (!p || !out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    pl = r01_project_active_plane(p);
    if (pl) {
        out->pixels = pl->pixels;
        out->tiles = pl->tiles;
        out->attrs = pl->attrs;
        out->is_plane = 1;
        out->index = p->active_plane;
        return 0;
    }
    s = r01_project_active_screen(p);
    if (s) {
        out->pixels = s->pixels;
        out->tiles = s->tiles;
        out->attrs = s->attrs;
        out->is_plane = 0;
        out->index = p->active_screen;
        return 0;
    }
    return -1;
}

int r01_project_select_bg_bank(R01Project *p, int bank) {
    if (!p || bank < 0 || bank >= R01_BG_BANKS) {
        return -1;
    }
    p->generate_bank = bank;
    return bank;
}

int r01_world_find_screen(const R01World *w, int col, int row) {
    int i;
    if (!w) {
        return -1;
    }
    for (i = 0; i < w->screen_count; i++) {
        if (w->screens[i].present && w->screens[i].col == col && w->screens[i].row == row) {
            return i;
        }
    }
    return -1;
}

int r01_world_toggle_screen(R01World *w, int col, int row) {
    int idx;
    if (!w || col < 0 || col >= R01_GRID_SIZE || row < 0 || row >= R01_GRID_SIZE) {
        return -1;
    }
    idx = r01_world_find_screen(w, col, row);
    if (idx >= 0) {
        int i;
        w->screens[idx].present = 0;
        for (i = idx; i < w->screen_count - 1; i++) {
            w->screens[i] = w->screens[i + 1];
        }
        w->screen_count--;
        memset(&w->screens[w->screen_count], 0, sizeof(R01Screen));
        return 0;
    }
    if (w->screen_count >= R01_MAX_SCREENS_PER_WORLD) {
        return -1;
    }
    {
        R01Screen *s = &w->screens[w->screen_count];
        int cell;
        memset(s, 0, sizeof(*s));
        s->col = col;
        s->row = row;
        s->present = 1;
        r01_screen_clear_pixels(s, 0);
        for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
            s->attrs[cell] = r01_attr_pack(w->default_bg_bank, w->default_pal_row, 0, 0, 0, 0);
        }
        w->screen_count++;
    }
    return 0;
}

int r01_world_toggle_plane(R01World *w, int slot) {
    R01ParallaxPlane *pl;
    int cell;
    if (!w || slot < 0 || slot >= R01_MAX_PARALLAX_PLANES) {
        return -1;
    }
    pl = &w->planes[slot];
    if (pl->present) {
        memset(pl, 0, sizeof(*pl));
        pl->slot = slot;
        pl->present = 0;
        return 0;
    }
    memset(pl, 0, sizeof(*pl));
    pl->slot = slot;
    pl->present = 1;
    r01_tilemap_clear_pixels(pl->pixels, 0);
    for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
        pl->attrs[cell] = r01_attr_pack(w->default_bg_bank, w->default_pal_row, 0, 0, 0, 0);
    }
    return 0;
}

void r01_tilemap_clear_pixels(uint8_t *pixels, uint8_t color) {
    size_t i;
    uint8_t c = (uint8_t)(color & 3u);
    if (!pixels) {
        return;
    }
    for (i = 0; i < (size_t)R01_SCREEN_PX_W * R01_SCREEN_PX_H; i++) {
        pixels[i] = c;
    }
}

void r01_tilemap_plot(uint8_t *pixels, int x, int y, uint8_t color) {
    if (!pixels || x < 0 || y < 0 || x >= R01_SCREEN_PX_W || y >= R01_SCREEN_PX_H) {
        return;
    }
    pixels[y * R01_SCREEN_PX_W + x] = (uint8_t)(color & 3u);
}

uint8_t r01_tilemap_get_pixel(const uint8_t *pixels, int x, int y) {
    if (!pixels || x < 0 || y < 0 || x >= R01_SCREEN_PX_W || y >= R01_SCREEN_PX_H) {
        return 0;
    }
    return pixels[y * R01_SCREEN_PX_W + x] & 3u;
}

void r01_tilemap_set_attr_bits(uint8_t *attrs, int tile_x, int tile_y, uint8_t set_mask, uint8_t clear_mask) {
    int cell;
    if (!attrs || tile_x < 0 || tile_y < 0 || tile_x >= R01_SCREEN_TILES_X || tile_y >= R01_SCREEN_TILES_Y) {
        return;
    }
    cell = tile_y * R01_SCREEN_TILES_X + tile_x;
    attrs[cell] = (uint8_t)((attrs[cell] & ~clear_mask) | set_mask);
}

uint8_t r01_tilemap_get_attr(const uint8_t *attrs, int tile_x, int tile_y) {
    if (!attrs || tile_x < 0 || tile_y < 0 || tile_x >= R01_SCREEN_TILES_X || tile_y >= R01_SCREEN_TILES_Y) {
        return 0;
    }
    return attrs[tile_y * R01_SCREEN_TILES_X + tile_x];
}

void r01_screen_clear_pixels(R01Screen *s, uint8_t color) {
    if (s) {
        r01_tilemap_clear_pixels(s->pixels, color);
    }
}

void r01_screen_plot(R01Screen *s, int x, int y, uint8_t color) {
    if (s) {
        r01_tilemap_plot(s->pixels, x, y, color);
    }
}

uint8_t r01_screen_get_pixel(const R01Screen *s, int x, int y) {
    return s ? r01_tilemap_get_pixel(s->pixels, x, y) : 0;
}

void r01_screen_set_attr_bits(R01Screen *s, int tile_x, int tile_y, uint8_t set_mask, uint8_t clear_mask) {
    if (s) {
        r01_tilemap_set_attr_bits(s->attrs, tile_x, tile_y, set_mask, clear_mask);
    }
}

uint8_t r01_screen_get_attr(const R01Screen *s, int tile_x, int tile_y) {
    return s ? r01_tilemap_get_attr(s->attrs, tile_x, tile_y) : 0;
}
