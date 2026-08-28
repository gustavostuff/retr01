#include "retr01_studio/play.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <string.h>

static int player_aabb_ok(const R01World *w, int px, int py) {
    int x1, y1, c0, c1, r0, r1, c, r;
    if (!w || px < 0 || py < 0) {
        return 0;
    }
    x1 = px + R01_PLAY_PLAYER_W - 1;
    y1 = py + R01_PLAY_PLAYER_H - 1;
    c0 = px / R01_SCREEN_PX_W;
    c1 = x1 / R01_SCREEN_PX_W;
    r0 = py / R01_SCREEN_PX_H;
    r1 = y1 / R01_SCREEN_PX_H;
    for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
            if (r01_world_find_screen(w, c, r) < 0) {
                return 0;
            }
        }
    }
    return 1;
}

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
        if (player_aabb_ok(w, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_aabb_ok(w, pl->player_x, ny)) {
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
