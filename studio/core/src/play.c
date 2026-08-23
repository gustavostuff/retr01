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
    int gc = w->grid_cols > 0 ? w->grid_cols : R01_GRID_SIZE;
    int gr = w->grid_rows > 0 ? w->grid_rows : R01_GRID_SIZE;
    if (col < 0 || row < 0 || col >= gc || row >= gr) {
        return 0;
    }
    return r01_world_find_screen(w, col, row) >= 0;
}

/*
 * True if the full player AABB at (px,py) only overlaps present screens.
 * If lock_home: every overlapped cell must equal (home_col, home_row).
 */
static int player_aabb_ok(const R01World *w, int px, int py, int lock_home, int home_col, int home_row) {
    int x1, y1, c0, c1, r0, r1, c, r;
    if (!w || px < 0 || py < 0) {
        return 0;
    }
    x1 = px + R01_PLAY_PLAYER_SIZE - 1;
    y1 = py + R01_PLAY_PLAYER_SIZE - 1;
    c0 = px / R01_SCREEN_PX_W;
    c1 = x1 / R01_SCREEN_PX_W;
    r0 = py / R01_SCREEN_PX_H;
    r1 = y1 / R01_SCREEN_PX_H;
    for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
            if (lock_home) {
                if (c != home_col || r != home_row) {
                    return 0;
                }
            } else if (!screen_present_at(w, c, r)) {
                return 0;
            }
        }
    }
    return 1;
}

static void set_home_from_player(R01PlayState *pl) {
    pl->home_col = pl->player_x / R01_SCREEN_PX_W;
    pl->home_row = pl->player_y / R01_SCREEN_PX_H;
}

static void place_player_centered(R01PlayState *pl, int col, int row) {
    pl->home_col = col;
    pl->home_row = row;
    pl->player_x = col * R01_SCREEN_PX_W + R01_SCREEN_PX_W / 2 - R01_PLAY_PLAYER_SIZE / 2;
    pl->player_y = row * R01_SCREEN_PX_H + R01_SCREEN_PX_H / 2 - R01_PLAY_PLAYER_SIZE / 2;
    pl->cam_x = col * R01_SCREEN_PX_W;
    pl->cam_y = row * R01_SCREEN_PX_H;
}

static void update_camera(R01PlayState *pl, const R01Constraints *c) {
    int target_x, target_y;
    int mode = c ? c->scroll_mode : R01_SCROLL_DEADZONE;
    int dzx = c ? c->deadzone_x : R01_PLAY_DZ_INSET_X;
    int dzy = c ? c->deadzone_y : R01_PLAY_DZ_INSET_Y;
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

    /* DEADZONE and HYBRID: smooth free-box only — no automatic screen snap. */
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
}

static void rebuild_fade_pal(R01PlayState *pl) {
    int i;
    for (i = 0; i < R01_MASTER_COLORS; i++) {
        uint8_t r, g, b;
        r01_kit_rgb(i, &r, &g, &b);
        pl->fade_pal[i][0] = (uint8_t)((r * pl->fade_level) / R01_PLAY_FADE_MAX);
        pl->fade_pal[i][1] = (uint8_t)((g * pl->fade_level) / R01_PLAY_FADE_MAX);
        pl->fade_pal[i][2] = (uint8_t)((b * pl->fade_level) / R01_PLAY_FADE_MAX);
    }
}

static int fade_pal_all_black(const R01PlayState *pl) {
    int i;
    if (pl->fade_level <= 0) {
        return 1;
    }
    for (i = 0; i < R01_MASTER_COLORS; i++) {
        if (pl->fade_pal[i][0] | pl->fade_pal[i][1] | pl->fade_pal[i][2]) {
            return 0;
        }
    }
    return 1;
}

static void apply_fade_rgb(const R01PlayState *pl, uint8_t *r, uint8_t *g, uint8_t *b) {
    /* Scale sampled kit RGB by fade_level (same as reading fade_pal[master]). */
    if (r) {
        *r = (uint8_t)((*r * pl->fade_level) / R01_PLAY_FADE_MAX);
    }
    if (g) {
        *g = (uint8_t)((*g * pl->fade_level) / R01_PLAY_FADE_MAX);
    }
    if (b) {
        *b = (uint8_t)((*b * pl->fade_level) / R01_PLAY_FADE_MAX);
    }
}

static void tick_fade(R01PlayState *pl, const R01Constraints *c) {
    if (pl->fade_phase == R01_PLAY_FADE_OUT) {
        if (pl->fade_level > 0) {
            pl->fade_level--;
        }
        rebuild_fade_pal(pl);
        if (fade_pal_all_black(pl)) {
            if (pl->pending_col >= 0 && pl->pending_row >= 0) {
                place_player_centered(pl, pl->pending_col, pl->pending_row);
                update_camera(pl, c);
            }
            pl->pending_col = -1;
            pl->pending_row = -1;
            pl->fade_phase = R01_PLAY_FADE_IN;
            pl->fade_level = 0;
            rebuild_fade_pal(pl);
        }
        return;
    }
    if (pl->fade_phase == R01_PLAY_FADE_IN) {
        if (pl->fade_level < R01_PLAY_FADE_MAX) {
            pl->fade_level++;
        }
        rebuild_fade_pal(pl);
        if (pl->fade_level >= R01_PLAY_FADE_MAX) {
            pl->fade_phase = R01_PLAY_FADE_IDLE;
            pl->fade_level = R01_PLAY_FADE_MAX;
            rebuild_fade_pal(pl);
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
    pl->facing_dx = 1;
    pl->facing_dy = 0;
    pl->fade_phase = R01_PLAY_FADE_IDLE;
    pl->fade_level = R01_PLAY_FADE_MAX;
    pl->pending_col = -1;
    pl->pending_row = -1;
    rebuild_fade_pal(pl);
    if (s && s->present) {
        place_player_centered(pl, s->col, s->row);
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
    int mode;
    int lock_home;
    if (!pl || !pl->active || !p) {
        return;
    }
    w = &p->worlds[p->active_world];
    c = r01_project_constraints(p);
    mode = c ? c->scroll_mode : R01_SCROLL_DEADZONE;
    lock_home = (mode == R01_SCROLL_HYBRID);
    pl->frame++;

    /* Palette fade runs even if movement is frozen. */
    if (pl->fade_phase != R01_PLAY_FADE_IDLE) {
        tick_fade(pl, c);
        return; /* no move / camera chase mid-fade */
    }

    if (dx || dy) {
        if (dx) {
            pl->facing_dx = dx < 0 ? -1 : 1;
            pl->facing_dy = 0;
        } else {
            pl->facing_dx = 0;
            pl->facing_dy = dy < 0 ? -1 : 1;
        }
    }

    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (player_aabb_ok(w, nx, pl->player_y, lock_home, pl->home_col, pl->home_row)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_aabb_ok(w, pl->player_x, ny, lock_home, pl->home_col, pl->home_row)) {
            pl->player_y = ny;
        }
    }

    if (!lock_home) {
        set_home_from_player(pl);
    }

    update_camera(pl, c);
}

static int pick_warp_neighbor(const R01World *w, const R01PlayState *pl, int *out_col, int *out_row) {
    int dirs[4][2];
    int n = 0;
    int i;
    int cx = pl->player_x + R01_PLAY_PLAYER_SIZE / 2;
    int cy = pl->player_y + R01_PLAY_PLAYER_SIZE / 2;
    int lx = cx - pl->home_col * R01_SCREEN_PX_W;
    int ly = cy - pl->home_row * R01_SCREEN_PX_H;

    if (pl->facing_dx || pl->facing_dy) {
        dirs[n][0] = pl->facing_dx;
        dirs[n][1] = pl->facing_dy;
        n++;
    }
    {
        int dist_l = lx;
        int dist_r = R01_SCREEN_PX_W - 1 - lx;
        int dist_t = ly;
        int dist_b = R01_SCREEN_PX_H - 1 - ly;
        int best = dist_l;
        int bdx = -1, bdy = 0;
        if (dist_r < best) {
            best = dist_r;
            bdx = 1;
            bdy = 0;
        }
        if (dist_t < best) {
            best = dist_t;
            bdx = 0;
            bdy = -1;
        }
        if (dist_b < best) {
            bdx = 0;
            bdy = 1;
        }
        dirs[n][0] = bdx;
        dirs[n][1] = bdy;
        n++;
    }
    dirs[n][0] = 1;
    dirs[n][1] = 0;
    n++;
    dirs[n][0] = -1;
    dirs[n][1] = 0;
    n++;
    for (i = 0; i < n; i++) {
        int ncol = pl->home_col + dirs[i][0];
        int nrow = pl->home_row + dirs[i][1];
        if (dirs[i][0] == 0 && dirs[i][1] == 0) {
            continue;
        }
        if (screen_present_at(w, ncol, nrow)) {
            *out_col = ncol;
            *out_row = nrow;
            return 1;
        }
    }
    if (screen_present_at(w, pl->home_col, pl->home_row - 1)) {
        *out_col = pl->home_col;
        *out_row = pl->home_row - 1;
        return 1;
    }
    if (screen_present_at(w, pl->home_col, pl->home_row + 1)) {
        *out_col = pl->home_col;
        *out_row = pl->home_row + 1;
        return 1;
    }
    return 0;
}

int r01_play_button(R01PlayState *pl, const R01Project *p, int button) {
    const R01World *w;
    const R01Constraints *c;
    int ncol, nrow;
    if (!pl || !pl->active || !p) {
        return 0;
    }
    if (pl->fade_phase != R01_PLAY_FADE_IDLE) {
        return 0;
    }
    w = &p->worlds[p->active_world];
    c = r01_project_constraints(p);

    if (button != R01_PLAY_BTN_COIN && button != R01_PLAY_BTN_START) {
        return 0;
    }
    if (!c || c->scroll_mode != R01_SCROLL_HYBRID) {
        return 0;
    }
    if (!pick_warp_neighbor(w, pl, &ncol, &nrow)) {
        return 0;
    }
    if (ncol == pl->home_col && nrow == pl->home_row) {
        return 0;
    }

    if (c->transition == R01_XITION_FADE) {
        /* Fade palettes to black, swap when buffer is black, then fade in. */
        pl->pending_col = ncol;
        pl->pending_row = nrow;
        pl->fade_phase = R01_PLAY_FADE_OUT;
        pl->fade_level = R01_PLAY_FADE_MAX;
        rebuild_fade_pal(pl);
        return 1;
    }

    place_player_centered(pl, ncol, nrow);
    update_camera(pl, c);
    return 1;
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
            *r = 0;
        }
        if (g) {
            *g = 0;
        }
        if (b) {
            *b = 0;
        }
        apply_fade_rgb(pl, r, g, b);
        return -1;
    }
    s = &w->screens[idx];
    attr = s->attrs[(ly / 8) * R01_SCREEN_TILES_X + (lx / 8)];
    if (r01_attr_anim(attr) && c && c->anim_rate > 0) {
        anim_frame = (pl->frame / c->anim_rate) & 3;
        lx = (lx & ~7) + ((lx + anim_frame) & 7);
        if (lx >= R01_SCREEN_PX_W) {
            lx = R01_SCREEN_PX_W - 1;
        }
    }
    r01_tilemap_pixel_rgb(p, w, s->pixels, s->attrs, lx, ly, r, g, b);
    apply_fade_rgb(pl, r, g, b);
    return 0;
}
