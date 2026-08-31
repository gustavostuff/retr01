#include "retr01_sim/play.h"

#include "retr01_sim/board.h"
#include "retr01_sim/gamepad.h"
#include "atmega1284p.h"
#include "as6c62256.h"
#include "atmega328p.h"
#include "beam_xy.h"
#include "video_sink.h"
#include "r01_play_anim_cart.h"
#include "r01_play_camera.h"

#include <string.h>

/* Studio/emu move+camera math; sim applies 1 logical px per sim VBlank (game frame). */

static void player_hit_rect(R01sBoard *b, int origin_x, int origin_y, int state_idx, int *hx, int *hy, int *hw,
                            int *hh) {
    int box_x = origin_x;
    int box_y = origin_y;
    int box_w = R01S_PLAY_PLAYER_W;
    int box_h = R01S_PLAY_PLAYER_H;
    if (b && b->cart_loaded && b->cart_player_entity != 0xFF &&
        b->cart_player_entity < b->cart_entity_type_count) {
        if (b->cart_off_player_anim != 0) {
            const uint8_t *img = b->cart_flash.mem;
            const uint8_t *blob = img + b->cart_off_player_anim;
            size_t blob_len = sizeof(b->cart_flash.mem) - (size_t)b->cart_off_player_anim;
            R01CartPlayerAnim anim;
            const uint8_t *st = NULL;
            if (r01_cart_player_anim_parse(blob, blob_len, &anim) == 0) {
                if (state_idx < 0 || state_idx >= anim.state_count) {
                    state_idx = 0;
                }
                if (r01_cart_player_anim_state_hdr(&anim, state_idx, &st) == 0 && st) {
                    int origin_ax = (int)st[0];
                    int origin_ay = (int)st[1];
                    box_x = origin_x + (int)st[2] - origin_ax;
                    box_y = origin_y + (int)st[3] - origin_ay;
                    box_w = (int)st[4];
                    box_h = (int)st[5];
                }
            }
        } else if (b->cart_off_entity_types != 0) {
            const uint8_t *img = b->cart_flash.mem;
            const uint8_t *trec =
                img + b->cart_off_entity_types + (size_t)b->cart_player_entity * 20u;
            int origin_ax = (int)trec[0];
            int origin_ay = (int)trec[1];
            box_x = origin_x + (int)b->cart_player_hit_x - origin_ax;
            box_y = origin_y + (int)b->cart_player_hit_y - origin_ay;
            box_w = (int)b->cart_player_hit_w;
            box_h = (int)b->cart_player_hit_h;
        }
    }
    if (hx) {
        *hx = box_x;
    }
    if (hy) {
        *hy = box_y;
    }
    if (hw) {
        *hw = box_w;
    }
    if (hh) {
        *hh = box_h;
    }
}

static int player_move_ok(R01sBoard *b, int ox, int oy) {
    int hx, hy, hw, hh;
    int state_idx = r01_play_anim_entity_state(&b->play.anim);
    player_hit_rect(b, ox, oy, state_idx, &hx, &hy, &hw, &hh);
    return r01s_board_aabb_ok(b, hx, hy, hw, hh);
}

static void update_camera(R01sBoard *b) {
    R01sPlay *pl = &b->play;
    r01_play_camera_update(&pl->cam_x, &pl->cam_y, pl->player_x, pl->player_y, R01S_PLAY_PLAYER_W,
                           R01S_PLAY_PLAYER_H, R01S_BG_SCREEN_PX_W, R01S_BG_SCREEN_PX_H, pl->cam_deadzone_x,
                           pl->cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
}

static void snap_camera(R01sBoard *b) {
    R01sPlay *pl = &b->play;
    r01_play_camera_snap(&pl->cam_x, &pl->cam_y, pl->player_x, pl->player_y, R01S_PLAY_PLAYER_W,
                         R01S_PLAY_PLAYER_H, R01S_BG_SCREEN_PX_W, R01S_BG_SCREEN_PX_H, pl->cam_deadzone_x,
                         pl->cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
}

static void play_load_cart_camera(R01sBoard *b) {
    if (!b) {
        return;
    }
    b->play.cam_deadzone_x = R01_PLAY_CAM_DEADZONE_X_DEFAULT;
    b->play.cam_deadzone_y = R01_PLAY_CAM_DEADZONE_Y_DEFAULT;
    if (b->cart_loaded) {
        if (b->cart_cam_deadzone_x != 0 || b->cart_cam_deadzone_y != 0) {
            int dx = (int)b->cart_cam_deadzone_x;
            int dy = (int)b->cart_cam_deadzone_y;
            if (dx <= R01S_BG_SCREEN_PX_W && dy <= R01S_BG_SCREEN_PX_H) {
                b->play.cam_deadzone_x = dx;
                b->play.cam_deadzone_y = dy;
            }
        }
    }
}

static void place_player_on_screen(R01sBoard *b, int col, int row) {
    R01sPlay *pl = &b->play;
    pl->player_x = R01S_PLAY_SPAWN_CENTER_X(col);
    pl->player_y = R01S_PLAY_SPAWN_CENTER_Y(row);
    snap_camera(b);
}

static void place_player_xy(R01sBoard *b, int wx, int wy) {
    R01sPlay *pl = &b->play;
    pl->player_x = wx;
    pl->player_y = wy;
    snap_camera(b);
}

static int player_instance_spawn(R01sBoard *b, int *out_x, int *out_y) {
    const uint8_t *img;
    const uint8_t *insts;
    int ii;

    if (!b || !b->cart_loaded || b->cart_player_entity == 0xFF ||
        b->cart_player_entity >= b->cart_entity_type_count || b->cart_entity_inst_count < 1 ||
        b->cart_off_entity_insts == 0) {
        return 0;
    }
    img = b->cart_flash.mem;
    if ((size_t)b->cart_off_entity_insts + (size_t)b->cart_entity_inst_count * 6u >
        sizeof(b->cart_flash.mem)) {
        return 0;
    }
    insts = img + b->cart_off_entity_insts;
    for (ii = 0; ii < (int)b->cart_entity_inst_count; ii++) {
        const uint8_t *irec = insts + (size_t)ii * 6u;
        if (irec[0] != b->cart_player_entity) {
            continue;
        }
        if (out_x) {
            *out_x = (int)((uint16_t)irec[2] | ((uint16_t)irec[3] << 8));
        }
        if (out_y) {
            *out_y = (int)((uint16_t)irec[4] | ((uint16_t)irec[5] << 8));
        }
        return 1;
    }
    return 0;
}

static int spawn_screen(R01sBoard *b, int *out_col, int *out_row) {
    int sc, sr;
    const uint8_t *prg;

    if (!b) {
        return 0;
    }
    if (b->cart_off_prg != 0 && b->cart_len_prg > 0x0109u) {
        prg = b->cart_flash.mem + b->cart_off_prg;
        if (prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P') {
            sc = (int)prg[0x0108];
            sr = (int)prg[0x0109];
            if (r01s_board_has_screen(b, sc, sr)) {
                if (out_col) {
                    *out_col = sc;
                }
                if (out_row) {
                    *out_row = sr;
                }
                return 1;
            }
        }
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

static int write_player_oam(R01sBoard *b, int *slot) {
    R01sPlay *pl = &b->play;
    const uint8_t *img = b->cart_flash.mem;
    const uint8_t *types;
    int player_type;

    if (!b || !slot || *slot >= 64 || !b->cart_loaded || b->cart_entity_type_count < 1 ||
        b->cart_off_entity_types == 0) {
        return 0;
    }
    if (b->cart_player_entity == 0xFF || b->cart_player_entity >= b->cart_entity_type_count) {
        return 0;
    }
    player_type = (int)b->cart_player_entity;
    types = img + b->cart_off_entity_types;

    if (b->cart_off_player_anim != 0) {
        const uint8_t *blob = img + b->cart_off_player_anim;
        size_t blob_len = sizeof(b->cart_flash.mem) - (size_t)b->cart_off_player_anim;
        R01CartPlayerAnim anim;
        int state_idx = r01_play_anim_entity_state(&pl->anim);
        int frame_slot = r01_play_anim_frame(&pl->anim);
        int flip_h = r01_play_anim_flip_h(&pl->anim);
        const uint8_t *st = NULL;
        const uint8_t *parts;
        int part_count;
        int pi;
        if (r01_cart_player_anim_parse(blob, blob_len, &anim) != 0) {
            return 0;
        }
        if (state_idx < 0 || state_idx >= anim.state_count) {
            state_idx = 0;
        }
        if (r01_cart_player_anim_state_hdr(&anim, state_idx, &st) != 0 || !st) {
            return 0;
        }
        parts = r01_cart_player_anim_frame_parts(&anim, state_idx, frame_slot, &part_count);
        if (!parts || part_count < 1) {
            return 0;
        }
        if (part_count > 4) {
            part_count = 4;
        }
        for (pi = 0; pi < part_count && *slot < 64; pi++) {
            const uint8_t *part = parts + (size_t)pi * 4u;
            int origin_x = (int)st[0];
            int origin_y = (int)st[1];
            int dx, dy;
            uint8_t attr;
            int sx, sy;
            r01_cart_part_pose(origin_x, origin_y, (int)(int8_t)part[2], (int)(int8_t)part[3], part[1], flip_h, 0, &dx,
                               &dy, &attr);
            sx = pl->player_x + dx - origin_x - pl->cam_x;
            sy = pl->player_y + dy - origin_y - pl->cam_y;
            if (r01s_oam_tile_off_screen(sx, sy)) {
                continue;
            }
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 0), r01s_oam_coord_to_u8(sy));
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 1), part[0]);
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 2), attr);
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 3), r01s_oam_coord_to_u8(sx));
            (*slot)++;
            b->health_saw_oam = 1;
        }
        return 1;
    }

    {
        const uint8_t *trec = types + (size_t)player_type * 20u;
        int origin_x = (int)trec[0];
        int origin_y = (int)trec[1];
        int part_count = (int)trec[2];
        int pi;
        if (part_count > 4) {
            part_count = 4;
        }
        for (pi = 0; pi < part_count && *slot < 64; pi++) {
            const uint8_t *part = trec + 4 + pi * 4;
            int dx = (int)(int8_t)part[2];
            int dy = (int)(int8_t)part[3];
            int sx = pl->player_x + dx - origin_x - pl->cam_x;
            int sy = pl->player_y + dy - origin_y - pl->cam_y;
            if (r01s_oam_tile_off_screen(sx, sy)) {
                continue;
            }
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 0), r01s_oam_coord_to_u8(sy));
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 1), part[0]);
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 2), part[1]);
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(*slot * 4 + 3), r01s_oam_coord_to_u8(sx));
            (*slot)++;
            b->health_saw_oam = 1;
        }
        return *slot > 0;
    }
}

static void write_oam(R01sBoard *b) {
    R01sPlay *pl;
    int slot = 0;
    int ii;
    const uint8_t *img;
    const uint8_t *types = NULL;
    const uint8_t *insts;
    int player_type = -1;

    if (!b || !b->play.enabled) {
        return;
    }
    pl = &b->play;
    /* Clear all slots unused (tile == 0xFF); match emu / docs. */
    memset(b->mcu1284.oam, 0xFF, sizeof(b->mcu1284.oam));

    img = b->cart_flash.mem;
    if (b->cart_loaded && b->cart_entity_type_count > 0 && b->cart_off_entity_types != 0 &&
        (size_t)b->cart_off_entity_types + (size_t)b->cart_entity_type_count * 20u <=
            sizeof(b->cart_flash.mem)) {
        types = img + b->cart_off_entity_types;
        if (b->cart_player_entity != 0xFF && b->cart_player_entity < b->cart_entity_type_count) {
            player_type = (int)b->cart_player_entity;
        }
    }

    if (player_type >= 0 && types) {
        (void)write_player_oam(b, &slot);
    }
    if (slot < 1) {
        int vx = pl->player_x - pl->cam_x;
        int vy = pl->player_y - pl->cam_y;
        if (!r01s_oam_tile_off_screen(vx, vy)) {
            r01s_atmega1284p_oam_poke(&b->mcu1284, 0, r01s_oam_coord_to_u8(vy));
            r01s_atmega1284p_oam_poke(&b->mcu1284, 1, 1);
            r01s_atmega1284p_oam_poke(&b->mcu1284, 2, 0);
            r01s_atmega1284p_oam_poke(&b->mcu1284, 3, r01s_oam_coord_to_u8(vx));
            slot = 1;
            b->health_saw_oam = 1;
        }
    }

    if (!types || !b->cart_loaded || b->cart_entity_inst_count < 1 || b->cart_off_entity_insts == 0) {
        return;
    }
    if ((size_t)b->cart_off_entity_insts + (size_t)b->cart_entity_inst_count * 6u >
        sizeof(b->cart_flash.mem)) {
        return;
    }
    insts = img + b->cart_off_entity_insts;
    for (ii = 0; ii < (int)b->cart_entity_inst_count && slot < 64; ii++) {
        const uint8_t *irec = insts + (size_t)ii * 6u;
        uint8_t type_id = irec[0];
        int world_x = (int)((uint16_t)irec[2] | ((uint16_t)irec[3] << 8));
        int world_y = (int)((uint16_t)irec[4] | ((uint16_t)irec[5] << 8));
        const uint8_t *trec;
        int origin_x, origin_y, part_count, pi;
        if (type_id >= b->cart_entity_type_count) {
            continue;
        }
        if (player_type >= 0 && (int)type_id == player_type) {
            continue;
        }
        trec = types + (size_t)type_id * 20u;
        origin_x = (int)trec[0];
        origin_y = (int)trec[1];
        part_count = (int)trec[2];
        if (part_count > 4) {
            part_count = 4;
        }
        for (pi = 0; pi < part_count && slot < 64; pi++) {
            const uint8_t *part = trec + 4 + pi * 4;
            int dx = (int)(int8_t)part[2];
            int dy = (int)(int8_t)part[3];
            uint8_t attr = part[1];
            int sx, sy;
            if (irec[1] & 1u) {
                dx = 2 * origin_x - dx - 8;
                attr = (uint8_t)(attr ^ 0x10u); /* FLIP_H */
            }
            if (irec[1] & 2u) {
                dy = 2 * origin_y - dy - 8;
                attr = (uint8_t)(attr ^ 0x20u); /* FLIP_V */
            }
            sx = world_x + dx - origin_x - pl->cam_x;
            sy = world_y + dy - origin_y - pl->cam_y;
            if (r01s_oam_tile_off_screen(sx, sy)) {
                continue;
            }
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(slot * 4 + 0), r01s_oam_coord_to_u8(sy));
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(slot * 4 + 1), part[0]);
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(slot * 4 + 2), attr);
            r01s_atmega1284p_oam_poke(&b->mcu1284, (uint8_t)(slot * 4 + 3), r01s_oam_coord_to_u8(sx));
            slot++;
            b->health_saw_oam = 1;
        }
    }
}

static void queue_video(R01sBoard *b) {
    R01sPlay *pl;
    int ox;
    int oy;
    int origin_changed;
    uint8_t sx;
    uint8_t sy;

    if (!b) {
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
    /* Host Play L0: update every camera move (match emu, not only screen crosses). */
    r01s_board_update_bg0_scroll(b, pl->cam_x, pl->cam_y);
}

static void apply_video_latch(R01sBoard *b) {
    R01sPlay *pl;
    R01sVideoSink *sink;

    if (!b || !b->play.video_pending) {
        return;
    }
    pl = &b->play;
    sink = b->video_impl.sink;
    if (sink) {
        int scroll_changed = pl->pending_scroll_x != r01s_sn74hc573_peek_q(b->io_latch_impl.latch573[R01S_LATCH_FE02]) ||
                             pl->pending_scroll_y != r01s_sn74hc573_peek_q(b->io_latch_impl.latch573[R01S_LATCH_FE03]);
        int origin_changed = pl->pending_origin_col != pl->origin_col || pl->pending_origin_row != pl->origin_row;
        int mode = r01s_video_sink_render_mode(sink);
        int do_clear = 0;
        if (mode == R01S_VIDEO_RENDER_NORMAL) {
            /* Normal: vblank already cleared; also clear on scroll/seam so stale pixels
             * do not linger if beam budget skips part of the field. */
            do_clear = scroll_changed || origin_changed || pl->pending_camera_reload;
        }
        /* Persist/Phosphor: never clear on scroll or 2x2 seam reload -- VRAM swap only.
         * Clearing here caused a full black frame during vblank until the beam
         * repainted (visible on first left step when cam_x crosses a screen column). */
        if (do_clear) {
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
    write_oam(b);
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
        r01_play_anim_update(&pl->anim, 0, 0);
        return;
    }
    r01_play_anim_update(&pl->anim, dx, dy);
    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (player_move_ok(b, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_move_ok(b, pl->player_x, ny)) {
            pl->player_y = ny;
        }
    }
    update_camera(b);
    if (b->cart_off_player_anim != 0) {
        const uint8_t *blob = b->cart_flash.mem + b->cart_off_player_anim;
        size_t blob_len = sizeof(b->cart_flash.mem) - (size_t)b->cart_off_player_anim;
        R01CartPlayerAnim anim;
        if (r01_cart_player_anim_parse(blob, blob_len, &anim) == 0) {
            r01_play_anim_tick_cart(&pl->anim, &anim);
        }
    }
    queue_video(b);
}

void r01s_play_on_vblank(R01sBoard *b) {
    if (!b || !b->play.enabled) {
        return;
    }
    step_move_from_pad(b);
    apply_video_latch(b);
    write_oam(b);
}

static int warp_to(R01sBoard *b, int col, int row) {
    if (!b || !b->play.enabled) {
        return 0;
    }
    if (!r01s_board_has_screen(b, col, row)) {
        return 0;
    }
    place_player_on_screen(b, col, row);
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
    play->player_w = R01S_PLAY_PLAYER_W;
    play->player_h = R01S_PLAY_PLAYER_H;
    play->origin_col = -1;
    play->origin_row = -1;
}

int r01s_play_start(R01sBoard *board) {
    int col = 0, row = 0;
    int sx, sy;

    if (!board || !board->cart_loaded) {
        return 0;
    }
    r01s_play_reset(&board->play);
    play_load_cart_camera(board);
    if (player_instance_spawn(board, &sx, &sy)) {
        r01s_board_mark_map_ready(board);
        r01_play_anim_init(&board->play.anim);
        place_player_xy(board, sx, sy);
    } else if (spawn_screen(board, &col, &row)) {
        r01s_board_mark_map_ready(board);
        r01_play_anim_init(&board->play.anim);
        place_player_on_screen(board, col, row);
    } else {
        return 0;
    }
    /* Latch scroll + 2x2 before play.enabled so no field renders at scroll=$00. */
    board->play.force_camera_reload = 1;
    queue_video(board);
    apply_video_latch(board);
    if (board->video_impl.sink &&
        r01s_video_sink_render_mode(board->video_impl.sink) == R01S_VIDEO_RENDER_NORMAL) {
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
    board->play.enabled = 1;
    write_oam(board);
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
