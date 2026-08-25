#include "retr01_sim/play.h"

#include "retr01_sim/board.h"
#include "retr01_sim/gamepad.h"
#include "at28c16.h"
#include "video_sink.h"

#include <string.h>

/* Mirror Studio / emu play.c — algorithms kept identical. */

static int player_aabb_ok(R01sBoard *b, int px, int py) {
    int x1, y1, c0, c1, r0, r1, c, r;

    if (!b || px < 0 || py < 0) {
        return 0;
    }
    x1 = px + R01S_PLAY_PLAYER_SIZE - 1;
    y1 = py + R01S_PLAY_PLAYER_SIZE - 1;
    c0 = px / R01S_BG_SCREEN_PX_W;
    c1 = x1 / R01S_BG_SCREEN_PX_W;
    r0 = py / R01S_BG_SCREEN_PX_H;
    r1 = y1 / R01S_BG_SCREEN_PX_H;
    for (c = c0; c <= c1; c++) {
        for (r = r0; r <= r1; r++) {
            if (!r01s_board_has_screen(b, c, r)) {
                return 0;
            }
        }
    }
    return 1;
}

static void place_player_centered(R01sPlay *pl, int col, int row) {
    pl->player_x = col * R01S_BG_SCREEN_PX_W + R01S_BG_SCREEN_PX_W / 2 - R01S_PLAY_PLAYER_SIZE / 2;
    pl->player_y = row * R01S_BG_SCREEN_PX_H + R01S_BG_SCREEN_PX_H / 2 - R01S_PLAY_PLAYER_SIZE / 2;
    pl->cam_x = pl->player_x + R01S_PLAY_PLAYER_SIZE / 2 - R01S_BG_SCREEN_PX_W / 2;
    pl->cam_y = pl->player_y + R01S_PLAY_PLAYER_SIZE / 2 - R01S_BG_SCREEN_PX_H / 2;
    if (pl->cam_x < 0) {
        pl->cam_x = 0;
    }
    if (pl->cam_y < 0) {
        pl->cam_y = 0;
    }
}

static void update_camera(R01sPlay *pl) {
    int ax = pl->player_x + R01S_PLAY_PLAYER_SIZE / 2;
    int ay = pl->player_y + R01S_PLAY_PLAYER_SIZE / 2;

    pl->cam_x = ax - R01S_BG_SCREEN_PX_W / 2;
    pl->cam_y = ay - R01S_BG_SCREEN_PX_H / 2;
    if (pl->cam_x < 0) {
        pl->cam_x = 0;
    }
    if (pl->cam_y < 0) {
        pl->cam_y = 0;
    }
}

static int spawn_screen(R01sBoard *b, int *out_col, int *out_row) {
    int sc, sr;

    if (!b) {
        return 0;
    }
    sc = (int)b->cart_start_col;
    sr = (int)b->cart_start_row;
    if (r01s_board_has_screen(b, sc, sr)) {
        if (out_col) {
            *out_col = sc;
        }
        if (out_row) {
            *out_row = sr;
        }
        return 1;
    }
    return r01s_board_first_screen(b, out_col, out_row);
}

static void sync_video(R01sBoard *b) {
    R01sPlay *pl;
    int ox, oy;
    int origin_changed;
    uint8_t sx, sy;

    if (!b || !b->play.enabled) {
        return;
    }
    pl = &b->play;
    ox = pl->cam_x / R01S_BG_SCREEN_PX_W;
    oy = pl->cam_y / R01S_BG_SCREEN_PX_H;
    origin_changed = (ox != pl->origin_col || oy != pl->origin_row);
    pl->origin_col = ox;
    pl->origin_row = oy;
    sx = (uint8_t)(pl->cam_x - ox * R01S_BG_SCREEN_PX_W);
    sy = (uint8_t)(pl->cam_y - oy * R01S_BG_SCREEN_PX_H);
    if (sx > 127) {
        sx = 127;
    }
    if (sy > 119) {
        sy = 119;
    }
    r01s_board_set_scroll(b, sx, sy);
    if (origin_changed) {
        (void)r01s_board_load_camera_2x2(b, ox, oy);
    }
}

static int warp_to(R01sBoard *b, int col, int row) {
    if (!b || !b->play.enabled) {
        return 0;
    }
    if (!r01s_board_has_screen(b, col, row)) {
        return 0;
    }
    place_player_centered(&b->play, col, row);
    /* Force camera reload even if origin numbers match after warp. */
    b->play.origin_col = -1;
    b->play.origin_row = -1;
    sync_video(b);
    return 1;
}

void r01s_play_reset(R01sPlay *play) {
    if (!play) {
        return;
    }
    memset(play, 0, sizeof(*play));
    play->player_w = R01S_PLAY_PLAYER_SIZE;
    play->player_h = R01S_PLAY_PLAYER_SIZE;
    play->origin_col = -1;
    play->origin_row = -1;
}

int r01s_play_start(R01sBoard *board) {
    int col = 0, row = 0;

    if (!board || !board->cart_loaded) {
        return 0;
    }
    r01s_play_reset(&board->play);
    if (!spawn_screen(board, &col, &row)) {
        return 0;
    }
    board->play.enabled = 1;
    place_player_centered(&board->play, col, row);
    sync_video(board);
    return 1;
}

void r01s_play_tick(R01sBoard *board, uint8_t pad) {
    R01sPlay *pl;
    uint8_t edge;
    int dx = 0;
    int dy = 0;

    if (!board || !board->play.enabled) {
        return;
    }
    pl = &board->play;
    edge = (uint8_t)(pad & (uint8_t)~pl->pad_prev);
    pl->pad_prev = pad;

    /* Studio: X → (0,0), Y → (1,0) */
    if (edge & R01S_PAD_X) {
        (void)warp_to(board, 0, 0);
    }
    if (edge & R01S_PAD_Y) {
        (void)warp_to(board, 1, 0);
    }

    if (pad & R01S_PAD_LEFT) {
        dx = -1;
    } else if (pad & R01S_PAD_RIGHT) {
        dx = 1;
    }
    if (pad & R01S_PAD_UP) {
        dy = -1;
    } else if (pad & R01S_PAD_DOWN) {
        dy = 1;
    }

    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (player_aabb_ok(board, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_aabb_ok(board, pl->player_x, ny)) {
            pl->player_y = ny;
        }
    }
    update_camera(pl);
    sync_video(board);
}

void r01s_play_draw(R01sBoard *board) {
    R01sPlay *pl;
    R01sVideoSink *sink;
    uint8_t master;
    uint8_t packed;
    int pcx, pcy;
    int scale_2x;

    if (!board || !board->play.enabled || !board->video_impl.sink || !board->video_impl.prom) {
        return;
    }
    pl = &board->play;
    sink = board->video_impl.sink;
    scale_2x = r01s_video_sink_scale_2x(sink);
    /* Studio / emu: global_pal_spr[0].idx[1] → active_pal[17]. */
    master = (uint8_t)(board->active_pal[17] & 63u);
    packed = r01s_at28c16_peek(board->video_impl.prom, master);

    for (pcy = 0; pcy < R01S_PLAY_PLAYER_SIZE; pcy++) {
        for (pcx = 0; pcx < R01S_PLAY_PLAYER_SIZE; pcx++) {
            int wx = pl->player_x + pcx;
            int wy = pl->player_y + pcy;
            int vx = wx - pl->cam_x;
            int vy = wy - pl->cam_y;
            int ox, oy;

            if (vx < 0 || vy < 0 || vx >= R01S_LOGICAL_W || vy >= R01S_LOGICAL_H) {
                continue;
            }
            if (scale_2x) {
                for (oy = 0; oy < 2; oy++) {
                    for (ox = 0; ox < 2; ox++) {
                        r01s_video_sink_plot(sink, vx * 2 + ox, vy * 2 + oy, packed);
                    }
                }
            } else {
                r01s_video_sink_plot(sink, vx + R01S_SCALE_1X_OX, vy + R01S_SCALE_1X_OY, packed);
            }
        }
    }
}
