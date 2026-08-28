#include "retr01_emu/play.h"

#include "retr01_emu/cart.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/video.h"

#include <string.h>

/* Studio Play SoT (retr01_studio/core/src/play.c) — keep algorithms identical. */

static int cart_is_phase1_play(const R01eCart *c) {
    const uint8_t *prg = r01e_cart_prg(c);
    if (!prg || c->len_prg < 0x0105u) {
        return 0;
    }
    return prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P';
}

static int player_aabb_ok(R01eMachine *m, int px, int py) {
    int x1, y1, c0, c1, r0, r1, c, r;
    int world = (int)m->io.world;

    if (!m || px < 0 || py < 0) {
        return 0;
    }
    x1 = px + R01E_PLAY_PLAYER_W - 1;
    y1 = py + R01E_PLAY_PLAYER_H - 1;
    c0 = px / R01E_SCREEN_PX_W;
    c1 = x1 / R01E_SCREEN_PX_W;
    r0 = py / R01E_SCREEN_PX_H;
    r1 = y1 / R01E_SCREEN_PX_H;
    for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
            if (!r01e_cart_has_screen(&m->cart, world, c, r)) {
                return 0;
            }
        }
    }
    return 1;
}

static void update_camera(R01ePlay *pl) {
    int ax = pl->player_x + R01E_PLAY_PLAYER_W / 2;
    int ay = pl->player_y + R01E_PLAY_PLAYER_H / 2;

    pl->cam_x = ax - R01E_SCREEN_PX_W / 2;
    pl->cam_y = ay - R01E_SCREEN_PX_H / 2;
    if (pl->cam_x < 0) {
        pl->cam_x = 0;
    }
    if (pl->cam_y < 0) {
        pl->cam_y = 0;
    }
}

static void place_player_on_screen(R01ePlay *pl, int col, int row) {
    pl->player_x = R01E_PLAY_SPAWN_CENTER_X(col);
    pl->player_y = R01E_PLAY_SPAWN_CENTER_Y(row);
    update_camera(pl);
}

static int spawn_screen(R01eMachine *m, int *out_col, int *out_row) {
    const uint8_t *prg = r01e_cart_prg(&m->cart);
    int sc, sr;

    if (prg && m->cart.len_prg > 0x0109u && cart_is_phase1_play(&m->cart)) {
        sc = (int)prg[0x0108];
        sr = (int)prg[0x0109];
        if (r01e_cart_has_screen(&m->cart, (int)m->io.world, sc, sr)) {
            if (out_col) {
                *out_col = sc;
            }
            if (out_row) {
                *out_row = sr;
            }
            return 1;
        }
    }
    if (r01e_cart_has_screen(&m->cart, (int)m->io.world, R01E_START_COL, R01E_START_ROW)) {
        if (out_col) {
            *out_col = R01E_START_COL;
        }
        if (out_row) {
            *out_row = R01E_START_ROW;
        }
        return 1;
    }
    {
        R01eWorldView wv;
        const uint8_t *dir;
        int si;
        if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) != 0) {
            return 0;
        }
        dir = r01e_cart_ptr(&m->cart, wv.base + wv.off_screen_dir, (size_t)wv.screen_count * 12u);
        if (!dir || wv.screen_count < 1) {
            return 0;
        }
        for (si = 0; si < wv.screen_count; si++) {
            const uint8_t *e = dir + (size_t)si * 12u;
            if (out_col) {
                *out_col = (int)e[0];
            }
            if (out_row) {
                *out_row = (int)e[1];
            }
            return 1;
        }
    }
    return 0;
}

void r01e_play_sync_video(R01eMachine *m) {
    R01ePlay *pl;
    R01eVideo *vid;
    int ox, oy;
    int origin_changed;

    if (!m || !m->play.enabled) {
        return;
    }
    pl = &m->play;
    vid = &m->video;
    ox = pl->cam_x / R01E_SCREEN_PX_W;
    oy = pl->cam_y / R01E_SCREEN_PX_H;
    origin_changed = (ox != vid->cam_origin_col || oy != vid->cam_origin_row);
    vid->cam_x = pl->cam_x;
    vid->cam_y = pl->cam_y;
    vid->cam_origin_col = ox;
    vid->cam_origin_row = oy;
    m->io.scroll_x = (uint8_t)(pl->cam_x - ox * R01E_SCREEN_PX_W);
    m->io.scroll_y = (uint8_t)(pl->cam_y - oy * R01E_SCREEN_PX_H);
    if (m->io.scroll_x > 127) {
        m->io.scroll_x = 127;
    }
    if (m->io.scroll_y > 119) {
        m->io.scroll_y = 119;
    }
    if (origin_changed) {
        (void)r01e_video_sync_camera(m);
    }
}

static void write_oam_player(R01eMachine *m) {
    R01ePlay *pl = &m->play;
    int vx = pl->player_x - pl->cam_x;
    int vy = pl->player_y - pl->cam_y;

    if (vx < 0 || vy < 0 || vx > 247 || vy > 247) {
        return;
    }
    m->io.oam[0] = (uint8_t)vy;
    m->io.oam[1] = 1; /* solid tile in SPR bank 0 */
    m->io.oam[2] = 0; /* bank 0, pal 0 */
    m->io.oam[3] = (uint8_t)vx;
}

void r01e_play_reset(R01ePlay *play) {
    if (!play) {
        return;
    }
    memset(play, 0, sizeof(*play));
    play->player_w = R01E_PLAY_PLAYER_W;
    play->player_h = R01E_PLAY_PLAYER_H;
}

int r01e_play_start(R01eMachine *m) {
    int col = 0, row = 0;

    if (!m) {
        return 0;
    }
    r01e_play_reset(&m->play);
    /* Phase 1 carts (R01P) always run Studio-Play-equivalent runtime from cart MAP. */
    if (!cart_is_phase1_play(&m->cart) && !r01e_cart_has_screen(&m->cart, 0, R01E_START_COL, R01E_START_ROW)) {
        /* Still try if any screens exist. */
    }
    if (!spawn_screen(m, &col, &row)) {
        return 0;
    }
    m->play.enabled = 1;
    place_player_on_screen(&m->play, col, row);
    r01e_play_sync_video(m);
    (void)r01e_video_sync_camera(m);
    write_oam_player(m);
    return 1;
}

static int warp_to(R01eMachine *m, int col, int row) {
    if (!m || !m->play.enabled) {
        return 0;
    }
    if (!r01e_cart_has_screen(&m->cart, (int)m->io.world, col, row)) {
        return 0;
    }
    place_player_on_screen(&m->play, col, row);
    r01e_play_sync_video(m);
    write_oam_player(m);
    return 1;
}

void r01e_play_tick(R01eMachine *m) {
    R01ePlay *pl;
    uint8_t pad;
    uint8_t edge;
    int dx = 0;
    int dy = 0;

    if (!m || !m->play.enabled) {
        return;
    }
    pl = &m->play;
    pad = m->io.pad0;
    edge = (uint8_t)(pad & (uint8_t)~pl->pad_prev);
    pl->pad_prev = pad;

    /* Studio: X → (0,0), Y → (1,0) */
    if (edge & R01E_PAD_X) {
        (void)warp_to(m, 0, 0);
    }
    if (edge & R01E_PAD_Y) {
        (void)warp_to(m, 1, 0);
    }

    if (pad & R01E_PAD_LEFT) {
        dx = -1;
    } else if (pad & R01E_PAD_RIGHT) {
        dx = 1;
    }
    if (pad & R01E_PAD_UP) {
        dy = -1;
    } else if (pad & R01E_PAD_DOWN) {
        dy = 1;
    }

    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (player_aabb_ok(m, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_aabb_ok(m, pl->player_x, ny)) {
            pl->player_y = ny;
        }
    }
    /* No dead zone: camera tracks the player every tick. */
    update_camera(pl);
    r01e_play_sync_video(m);
    write_oam_player(m);
}

void r01e_play_player_rgb(const R01eMachine *m, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!m || !r || !g || !b) {
        return;
    }
    r01e_video_kit_rgb(m->io.pal[R01E_PAL_PLAYER] & 63, r, g, b);
}

void r01e_play_draw(R01eMachine *m) {
    /* Player is drawn via OAM composite in r01e_video_render_frame. */
    (void)m;
}

void r01e_play_post_event(R01ePlay *play, R01eEvent evt) {
    (void)play;
    (void)evt;
}
