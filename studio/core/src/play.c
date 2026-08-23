#include "retr01_studio/play.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <string.h>

void r01_constraints_init_default(R01Constraints *c) {
    if (!c) {
        return;
    }
    memset(c, 0, sizeof(*c));
    c->player_meta = -1;
    c->enemy_anim_rate = 8;
    c->anim_rate = 12;
    c->scroll_mode = R01_SCROLL_DEADZONE;
    c->deadzone_x = R01_PLAY_DZ_INSET_X;
    c->deadzone_y = R01_PLAY_DZ_INSET_Y;
    c->transition = R01_XITION_CUT;
}

const R01Constraints *r01_project_constraints(const R01Project *p) {
    const R01World *w;
    if (!p) {
        return NULL;
    }
    w = &p->worlds[p->active_world];
    if (w->present && w->use_constraints) {
        return &w->constraints;
    }
    return &p->constraints;
}

static int screen_present_at(const R01World *w, int col, int row) {
    return r01_world_find_screen(w, col, row) >= 0;
}

static void clamp_player_to_world(const R01World *w, int *px, int *py) {
    int col, row, lx, ly;
    int gc = w->grid_cols > 0 ? w->grid_cols : R01_GRID_SIZE;
    int gr = w->grid_rows > 0 ? w->grid_rows : R01_GRID_SIZE;
    int max_x = gc * R01_SCREEN_PX_W - R01_PLAY_PLAYER_SIZE;
    int max_y = gr * R01_SCREEN_PX_H - R01_PLAY_PLAYER_SIZE;
    if (*px < 0) {
        *px = 0;
    }
    if (*py < 0) {
        *py = 0;
    }
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (*px > max_x) {
        *px = max_x;
    }
    if (*py > max_y) {
        *py = max_y;
    }
    col = *px / R01_SCREEN_PX_W;
    row = *py / R01_SCREEN_PX_H;
    lx = *px - col * R01_SCREEN_PX_W;
    ly = *py - row * R01_SCREEN_PX_H;
    if (col >= gc) {
        col = gc - 1;
        lx = R01_SCREEN_PX_W - R01_PLAY_PLAYER_SIZE;
    }
    if (row >= gr) {
        row = gr - 1;
        ly = R01_SCREEN_PX_H - R01_PLAY_PLAYER_SIZE;
    }
    if (!screen_present_at(w, col, row)) {
        int i;
        for (i = 0; i < w->screen_count; i++) {
            if (w->screens[i].present) {
                col = w->screens[i].col;
                row = w->screens[i].row;
                lx = R01_SCREEN_PX_W / 2 - R01_PLAY_PLAYER_SIZE / 2;
                ly = R01_SCREEN_PX_H / 2 - R01_PLAY_PLAYER_SIZE / 2;
                break;
            }
        }
    }
    *px = col * R01_SCREEN_PX_W + lx;
    *py = row * R01_SCREEN_PX_H + ly;
}

static void update_camera(R01PlayState *pl, const R01Constraints *c) {
    int target_x, target_y;
    int mode = c ? c->scroll_mode : R01_SCROLL_DEADZONE;
    int dzx = c ? c->deadzone_x : R01_PLAY_DZ_INSET_X;
    int dzy = c ? c->deadzone_y : R01_PLAY_DZ_INSET_Y;
    /* Camera tracks the center of the 8×8 player. */
    int ax = pl->player_x + R01_PLAY_PLAYER_SIZE / 2;
    int ay = pl->player_y + R01_PLAY_PLAYER_SIZE / 2;

    if (mode == R01_SCROLL_INSTANT) {
        pl->cam_x = (ax / R01_SCREEN_PX_W) * R01_SCREEN_PX_W;
        pl->cam_y = (ay / R01_SCREEN_PX_H) * R01_SCREEN_PX_H;
        return;
    }

    target_x = ax - R01_SCREEN_PX_W / 2;
    target_y = ay - R01_SCREEN_PX_H / 2;
    if (target_x < 0) {
        target_x = 0;
    }
    if (target_y < 0) {
        target_y = 0;
    }

    if (mode == R01_SCROLL_PIXEL) {
        pl->cam_x = target_x;
        pl->cam_y = target_y;
        return;
    }

    /* DEADZONE / HYBRID: free box is inset by deadzone_* from viewport edges. */
    {
        int left = pl->cam_x + dzx;
        int right = pl->cam_x + R01_SCREEN_PX_W - dzx;
        int top = pl->cam_y + dzy;
        int bot = pl->cam_y + R01_SCREEN_PX_H - dzy;
        if (ax < left) {
            pl->cam_x -= (left - ax);
        } else if (ax > right) {
            pl->cam_x += (ax - right);
        }
        if (ay < top) {
            pl->cam_y -= (top - ay);
        } else if (ay > bot) {
            pl->cam_y += (ay - bot);
        }
        if (pl->cam_x < 0) {
            pl->cam_x = 0;
        }
        if (pl->cam_y < 0) {
            pl->cam_y = 0;
        }
    }

    if (mode == R01_SCROLL_HYBRID) {
        int pcol = ax / R01_SCREEN_PX_W;
        int prow = ay / R01_SCREEN_PX_H;
        int ccol = pl->cam_x / R01_SCREEN_PX_W;
        int crow = pl->cam_y / R01_SCREEN_PX_H;
        if (pcol != ccol || prow != crow) {
            if (c && c->transition == R01_XITION_FADE && pl->fade == 0) {
                pl->fade = 16;
            }
            pl->cam_x = pcol * R01_SCREEN_PX_W;
            pl->cam_y = prow * R01_SCREEN_PX_H;
        }
    }
}

void r01_play_start(R01PlayState *pl, const R01Project *p) {
    const R01World *w;
    R01Screen *s;
    if (!pl) {
        return;
    }
    memset(pl, 0, sizeof(*pl));
    if (!p) {
        return;
    }
    w = &p->worlds[p->active_world];
    s = NULL;
    if (p->active_screen >= 0 && p->active_screen < w->screen_count) {
        s = (R01Screen *)&w->screens[p->active_screen];
    } else if (w->screen_count > 0) {
        s = (R01Screen *)&w->screens[0];
    }
    pl->active = 1;
    if (s && s->present) {
        pl->player_x = s->col * R01_SCREEN_PX_W + R01_SCREEN_PX_W / 2 - R01_PLAY_PLAYER_SIZE / 2;
        pl->player_y = s->row * R01_SCREEN_PX_H + R01_SCREEN_PX_H / 2 - R01_PLAY_PLAYER_SIZE / 2;
        pl->cam_x = s->col * R01_SCREEN_PX_W;
        pl->cam_y = s->row * R01_SCREEN_PX_H;
    }
    update_camera(pl, r01_project_constraints(p));
}

void r01_play_stop(R01PlayState *pl) {
    if (pl) {
        pl->active = 0;
    }
}

void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy) {
    const R01World *w;
    const R01Constraints *c;
    int nx, ny;
    if (!pl || !pl->active || !p) {
        return;
    }
    w = &p->worlds[p->active_world];
    c = r01_project_constraints(p);
    pl->frame++;

    if (pl->fade > 0) {
        pl->fade--;
    }

    nx = pl->player_x + dx;
    ny = pl->player_y + dy;
    {
        int ncol = nx / R01_SCREEN_PX_W;
        int nrow = ny / R01_SCREEN_PX_H;
        int ocol = pl->player_x / R01_SCREEN_PX_W;
        int orow = pl->player_y / R01_SCREEN_PX_H;
        int gc = w->grid_cols > 0 ? w->grid_cols : R01_GRID_SIZE;
        int gr = w->grid_rows > 0 ? w->grid_rows : R01_GRID_SIZE;
        if (ncol != ocol || nrow != orow) {
            if (ncol < 0 || nrow < 0 || ncol >= gc || nrow >= gr || !screen_present_at(w, ncol, nrow)) {
                nx = pl->player_x;
                ny = pl->player_y;
            } else if (c && c->transition == R01_XITION_FADE) {
                pl->fade = 12;
            }
        }
    }
    pl->player_x = nx;
    pl->player_y = ny;
    clamp_player_to_world(w, &pl->player_x, &pl->player_y);
    update_camera(pl, c);
}

int r01_play_sample(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                    uint8_t *b) {
    const R01World *w;
    const R01Constraints *c;
    int wx, wy, col, row, lx, ly, idx;
    const R01Screen *s;
    uint8_t attr;
    int anim_frame = 0;

    if (!p || !pl || vx < 0 || vy < 0 || vx >= R01_SCREEN_PX_W || vy >= R01_SCREEN_PX_H) {
        return -1;
    }
    w = &p->worlds[p->active_world];
    c = r01_project_constraints(p);
    wx = pl->cam_x + vx;
    wy = pl->cam_y + vy;
    if (wx < 0 || wy < 0) {
        return -1;
    }
    col = wx / R01_SCREEN_PX_W;
    row = wy / R01_SCREEN_PX_H;
    lx = wx % R01_SCREEN_PX_W;
    ly = wy % R01_SCREEN_PX_H;
    idx = r01_world_find_screen(w, col, row);
    if (idx < 0) {
        if (r) {
            *r = 8;
        }
        if (g) {
            *g = 8;
        }
        if (b) {
            *b = 12;
        }
        return -1;
    }
    s = &w->screens[idx];
    attr = s->attrs[(ly / 8) * R01_SCREEN_TILES_X + (lx / 8)];
    if (r01_attr_anim(attr) && c && c->anim_rate > 0) {
        anim_frame = (pl->frame / c->anim_rate) & 3;
        /* soft preview: nudge sample coords within tile by frame (visual pulse) */
        lx = (lx & ~7) + ((lx + anim_frame) & 7);
        if (lx >= R01_SCREEN_PX_W) {
            lx = R01_SCREEN_PX_W - 1;
        }
    }
    r01_tilemap_pixel_rgb(p, w, s->pixels, s->attrs, lx, ly, r, g, b);
    if (pl->fade > 0) {
        /* dim during fade transition */
        if (r) {
            *r = (uint8_t)((*r * pl->fade) / 16);
        }
        if (g) {
            *g = (uint8_t)((*g * pl->fade) / 16);
        }
        if (b) {
            *b = (uint8_t)((*b * pl->fade) / 16);
        }
    }
    return 0;
}
