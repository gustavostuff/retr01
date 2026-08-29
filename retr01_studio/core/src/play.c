#include "retr01_studio/collision.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/play.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <string.h>

static void update_camera(R01PlayState *pl) {
    int ax = pl->player_x + R01_PLAY_PLAYER_W / 2;
    int ay = pl->player_y + R01_PLAY_PLAYER_H / 2;
    pl->cam_x = ax - R01_SCREEN_PX_W / 2;
    pl->cam_y = ay - R01_SCREEN_PX_H / 2;
    if (pl->cam_x < 0) {
        pl->cam_x = 0;
    }
    if (pl->cam_y < 0) {
        pl->cam_y = 0;
    }
}

static void place_player_on_screen(R01PlayState *pl, int col, int row) {
    pl->player_x = R01_PLAY_SPAWN_CENTER_X(col);
    pl->player_y = R01_PLAY_SPAWN_CENTER_Y(row);
    update_camera(pl);
}

static int play_spawn_screen(const R01World *w, int *out_col, int *out_row) {
    int idx;
    if (!w) {
        return 0;
    }
    idx = r01_world_default_screen(w);
    if (idx < 0 || idx >= w->screen_count || !w->screens[idx].present) {
        return 0;
    }
    if (out_col) {
        *out_col = w->screens[idx].col;
    }
    if (out_row) {
        *out_row = w->screens[idx].row;
    }
    return 1;
}

int r01_play_start(R01PlayState *pl, const R01Project *p) {
    const R01World *w;
    int col = 0, row = 0;
    if (!pl) {
        return 0;
    }
    memset(pl, 0, sizeof(*pl));
    if (!p) {
        return 0;
    }
    w = r01_project_active_world_const(p);
    if (!play_spawn_screen(w, &col, &row)) {
        return 0;
    }
    pl->active = 1;
    place_player_on_screen(pl, col, row);
    return 1;
}

void r01_play_stop(R01PlayState *pl) {
    if (pl) {
        pl->active = 0;
    }
}

void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy) {
    const R01World *w;
    if (!pl || !pl->active || !p) {
        return;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return;
    }
    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (r01_world_player_aabb_ok(w, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (r01_world_player_aabb_ok(w, pl->player_x, ny)) {
            pl->player_y = ny;
        }
    }
    /* No dead zone: camera tracks the player every tick. */
    update_camera(pl);
}

static int warp_to(R01PlayState *pl, const R01Project *p, int col, int row) {
    const R01World *w;
    if (!pl || !pl->active || !p) {
        return 0;
    }
    w = r01_project_active_world_const(p);
    if (!w || r01_world_find_screen(w, col, row) < 0) {
        return 0;
    }
    place_player_on_screen(pl, col, row);
    return 1;
}

int r01_play_button(R01PlayState *pl, const R01Project *p, int button) {
    if (button == R01_PLAY_BTN_X) {
        return warp_to(pl, p, 0, 0);
    }
    if (button == R01_PLAY_BTN_Y) {
        return warp_to(pl, p, 1, 0);
    }
    return 0;
}

int r01_play_screen_index(const R01PlayState *pl, const R01World *w) {
    int col, row;
    if (!pl || !w) {
        return -1;
    }
    col = (pl->player_x + R01_PLAY_PLAYER_W / 2) / R01_SCREEN_PX_W;
    row = (pl->player_y + R01_PLAY_PLAYER_H / 2) / R01_SCREEN_PX_H;
    return r01_world_find_screen(w, col, row);
}

int r01_play_sample_bg(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                       uint8_t *b) {
    const R01World *w;
    int wx, wy, col, row, idx;
    const R01Screen *s;
    if (!p || !pl || vx < 0 || vy < 0 || vx >= R01_SCREEN_PX_W || vy >= R01_SCREEN_PX_H) {
        return -1;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return -1;
    }
    wx = pl->cam_x + vx;
    wy = pl->cam_y + vy;
    if (wx < 0 || wy < 0) {
        r01_project_backdrop_rgb(p, w, r, g, b);
        return 0;
    }
    col = wx / R01_SCREEN_PX_W;
    row = wy / R01_SCREEN_PX_H;
    idx = r01_world_find_screen(w, col, row);
    if (idx < 0) {
        r01_project_backdrop_rgb(p, w, r, g, b);
        return 0;
    }
    s = &w->screens[idx];
    r01_screen_pixel_rgb(p, w, s, wx % R01_SCREEN_PX_W, wy % R01_SCREEN_PX_H, r, g, b);
    return 0;
}

int r01_play_build_oam(const R01Project *p, const R01PlayState *pl, R01OamEntry *out, int cap) {
    const R01World *w;
    int n = 0;
    int i;
    if (!p || !pl || !out || cap < 1) {
        return 0;
    }
    w = r01_project_active_world_const(p);
    if (!w) {
        return 0;
    }
    /* Slot 0: player (hardcoded 8x8 tile 1 / bank 0 -- matches cart stub). */
    out[0].x = pl->player_x - pl->cam_x;
    out[0].y = pl->player_y - pl->cam_y;
    out[0].bank = 0;
    out[0].tile_id = R01_SPR_PLAYER_TILE_ID;
    out[0].pal = 0;
    out[0].flip_h = 0;
    out[0].flip_v = 0;
    n = 1;

    for (i = 0; i < w->instance_count && n < cap && n < R01_OAM_MAX; i++) {
        const R01EntityInstance *inst = &w->instances[i];
        const R01EntityType *ent;
        const R01EntityState *st;
        const R01EntityFrame *fr;
        int pi;
        if (inst->type_id < 0 || inst->type_id >= w->entity_count) {
            continue;
        }
        ent = &w->entities[inst->type_id];
        if (ent->state_count < 1 || ent->states[0].frame_count < 1) {
            continue;
        }
        st = &ent->states[0];
        fr = &st->frames[0];
        for (pi = 0; pi < fr->part_count && n < cap && n < R01_OAM_MAX; pi++) {
            const R01EntityPart *pt = &fr->parts[pi];
            int dx, dy, fh, fv;
            int ox, oy;
            r01_entity_part_instance_pose(st, pt, inst->flip_h, inst->flip_v, &dx, &dy, &fh, &fv);
            ox = r01_entity_world_x(inst->world_x, st->origin_x, dx) - pl->cam_x;
            oy = r01_entity_world_y(inst->world_y, st->origin_y, dy) - pl->cam_y;
            if (r01_oam_tile_off_screen(ox, oy)) {
                continue;
            }
            out[n].x = ox;
            out[n].y = oy;
            out[n].bank = pt->bank;
            out[n].tile_id = pt->tile_id;
            out[n].pal = pt->pal;
            out[n].flip_h = fh;
            out[n].flip_v = fv;
            n++;
        }
    }
    return n;
}
