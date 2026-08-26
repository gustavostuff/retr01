#include "retr01_sim/play.h"

#include "retr01_sim/board.h"
#include "retr01_sim/gamepad.h"
#include "atmega1284p.h"
#include "as6c62256.h"
#include "atmega328p.h"
#include "beam_xy.h"
#include "video_sink.h"

#include <string.h>

/* Studio/emu move+camera math; sim applies 1 logical px per sim VBlank (game frame). */

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

static void write_oam_player(R01sBoard *b) {
    R01sPlay *pl;
    int vx;
    int vy;

    if (!b || !b->play.enabled) {
        return;
    }
    pl = &b->play;
    vx = pl->player_x - pl->cam_x;
    vy = pl->player_y - pl->cam_y;
    if (vx < 0 || vy < 0 || vx > 247 || vy > 247) {
        r01s_atmega1284p_oam_poke(&b->mcu1284, 0, 0xFF);
        return;
    }
    r01s_atmega1284p_oam_poke(&b->mcu1284, 0, (uint8_t)vy);
    r01s_atmega1284p_oam_poke(&b->mcu1284, 1, 1);
    r01s_atmega1284p_oam_poke(&b->mcu1284, 2, 0);
    r01s_atmega1284p_oam_poke(&b->mcu1284, 3, (uint8_t)vx);
    b->health_saw_oam = 1;
}

static void queue_video(R01sBoard *b) {
    R01sPlay *pl;
    int ox;
    int oy;
    int origin_changed;
    uint8_t sx;
    uint8_t sy;

    if (!b || !b->play.enabled) {
        return;
    }
    pl = &b->play;
    ox = pl->cam_x / R01S_BG_SCREEN_PX_W;
    oy = pl->cam_y / R01S_BG_SCREEN_PX_H;
    origin_changed = (ox != pl->origin_col || oy != pl->origin_row);
    if (pl->force_camera_reload) {
        origin_changed = 1;
        pl->force_camera_reload = 0;
    }
    sx = (uint8_t)(pl->cam_x - ox * R01S_BG_SCREEN_PX_W);
    sy = (uint8_t)(pl->cam_y - oy * R01S_BG_SCREEN_PX_H);
    if (sx > 127) {
        sx = 127;
    }
    if (sy > 119) {
        sy = 119;
    }
    pl->pending_scroll_x = sx;
    pl->pending_scroll_y = sy;
    pl->pending_origin_col = ox;
    pl->pending_origin_row = oy;
    pl->pending_camera_reload = origin_changed;
    pl->video_pending = 1;
}

static void apply_video_latch(R01sBoard *b) {
    R01sPlay *pl;
    R01sVideoSink *sink;

    if (!b || !b->play.enabled || !b->play.video_pending) {
        return;
    }
    pl = &b->play;
    sink = b->video_impl.sink;
    /* Persist/Phosphor must not hard-clear on scroll: that wipes trails and makes the
     * slow beam redraw look like a growing/glitching sprite. Normal clears on VBlank. */
    if (sink && r01s_video_sink_render_mode(sink) == R01S_VIDEO_RENDER_NORMAL) {
        int scroll_changed = pl->pending_scroll_x != r01s_sn74hc573_peek_q(b->io_latch_impl.latch573[R01S_LATCH_FE02]) ||
                             pl->pending_scroll_y != r01s_sn74hc573_peek_q(b->io_latch_impl.latch573[R01S_LATCH_FE03]);
        int origin_changed = pl->pending_origin_col != pl->origin_col || pl->pending_origin_row != pl->origin_row;
        if (scroll_changed || origin_changed || pl->pending_camera_reload) {
            r01s_video_sink_clear(sink);
        }
    }
    r01s_board_set_scroll(b, pl->pending_scroll_x, pl->pending_scroll_y);
    if (pl->pending_camera_reload) {
        (void)r01s_board_load_camera_2x2(b, pl->pending_origin_col, pl->pending_origin_row);
    }
    pl->origin_col = pl->pending_origin_col;
    pl->origin_row = pl->pending_origin_row;
    pl->video_pending = 0;
    write_oam_player(b);
}

static void step_move_from_pad(R01sBoard *b) {
    R01sPlay *pl;
    uint8_t pad;
    int dx = 0;
    int dy = 0;

    if (!b || !b->play.enabled) {
        return;
    }
    pl = &b->play;
    pad = pl->pad_held;
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
    if (dx == 0 && dy == 0) {
        return;
    }
    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (player_aabb_ok(b, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_aabb_ok(b, pl->player_x, ny)) {
            pl->player_y = ny;
        }
    }
    update_camera(pl);
    queue_video(b);
}

void r01s_play_on_vblank(R01sBoard *b) {
    if (!b || !b->play.enabled) {
        return;
    }
    step_move_from_pad(b);
    apply_video_latch(b);
    write_oam_player(b);
}

static int warp_to(R01sBoard *b, int col, int row) {
    if (!b || !b->play.enabled) {
        return 0;
    }
    if (!r01s_board_has_screen(b, col, row)) {
        return 0;
    }
    place_player_centered(&b->play, col, row);
    b->play.force_camera_reload = 1;
    queue_video(b);
    apply_video_latch(b);
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
    r01s_board_mark_map_ready(board);
    board->play.enabled = 1;
    /* Host Play owns pads; FAST catchup skips smoke LDA $FE60 so mark pad health here. */
    board->health_saw_pad = 1;
    place_player_centered(&board->play, col, row);
    /* Same as FAST apply: do not let residual reset_hold re-vector into smoke. */
    board->reset_hold = 0;
    r01s_board_park_bringup_cpu(board);
    /* Ensure smoke-equivalent tone if catchup skipped or APU was reset. */
    if (!r01s_atmega328p_enabled(board->apu_impl.apu)) {
        r01s_atmega328p_poke(board->apu_impl.apu, 1, 0x10);
        r01s_atmega328p_poke(board->apu_impl.apu, 2, 0x00);
        r01s_atmega328p_poke(board->apu_impl.apu, 0, 0x8F);
    }
    if (board->video_impl.sink) {
        r01s_video_sink_clear(board->video_impl.sink);
    }
    /* After clear, start raster at (0,0) so the first field paints the top (no black corner). */
    r01s_beam_xy_rewind(board->beam_impl.beam_x);
    board->linebuf_prev_hblank = 0;
    board->vblank_prev = 0;
    board->linebuf_show_half = 0;
    if (board->mcu_lb_impl.sram) {
        uint16_t ai;
        for (ai = 0; ai < 256u; ai++) {
            r01s_as6c62256_poke(board->mcu_lb_impl.sram, ai, 0);
        }
    }
    board->play.force_camera_reload = 1;
    queue_video(board);
    r01s_play_on_vblank(board);
    return 1;
}

void r01s_play_tick(R01sBoard *board, uint8_t pad) {
    R01sPlay *pl;
    uint8_t edge;

    if (!board || !board->play.enabled) {
        return;
    }
    pl = &board->play;
    edge = (uint8_t)(pad & (uint8_t)~pl->pad_prev);
    pl->pad_prev = pad;
    pl->pad_held = (uint8_t)(pad & (R01S_PAD_UP | R01S_PAD_DOWN | R01S_PAD_LEFT | R01S_PAD_RIGHT));

    if (edge & R01S_PAD_X) {
        (void)warp_to(board, 0, 0);
    }
    if (edge & R01S_PAD_Y) {
        (void)warp_to(board, 1, 0);
    }
}

void r01s_play_draw(R01sBoard *board) {
    (void)board;
}
