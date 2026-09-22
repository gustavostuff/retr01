#include "retr01_emu/play.h"

#include "retr01_emu/cart.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/video.h"
#include "r01_play_anim_cart.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"

#include <string.h>

/* Emu Host Play SoT (docs). Keep algorithms aligned with Studio play.c. */

static int cart_is_phase1_play(const R01eCart *c) {
    const uint8_t *prg = r01e_cart_prg(c);
    if (!prg || c->len_prg < 0x0105u) {
        return 0;
    }
    return prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P';
}

static int cart_is_c_prg(const R01eCart *c) {
    const uint8_t *prg = r01e_cart_prg(c);
    if (!prg || c->len_prg < 0x00F5u) {
        return 0;
    }
    return cart_is_phase1_play(c) && prg[0x00F4] >= 5u;
}

static void play_follow_c_sys(R01eMachine *m) {
    if (!m || !m->ram[0x02E8]) {
        return;
    }
    m->play.player_x = (int)m->ram[0x02E0] | ((int)m->ram[0x02E1] << 8);
    m->play.player_y = (int)m->ram[0x02E2] | ((int)m->ram[0x02E3] << 8);
    m->play.cam_x = (int)m->ram[0x02E4] | ((int)m->ram[0x02E5] << 8);
    m->play.cam_y = (int)m->ram[0x02E6] | ((int)m->ram[0x02E7] << 8);
}

static void player_hit_rect(R01eMachine *m, int origin_x, int origin_y, int state_idx, int *hx, int *hy, int *hw,
                            int *hh) {
    R01eWorldView wv;
    int box_x = origin_x;
    int box_y = origin_y;
    int box_w = R01E_PLAY_PLAYER_W;
    int box_h = R01E_PLAY_PLAYER_H;

    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) == 0 &&
        wv.player_entity != R01E_CART_PLAYER_ENTITY_NONE && wv.player_entity < wv.entity_type_count) {
        if (wv.has_player_anim) {
            const uint8_t *blob =
                r01e_cart_ptr(&m->cart, wv.base + wv.off_player_anim, wv.len > wv.off_player_anim ? wv.len - wv.off_player_anim : 0);
            R01CartPlayerAnim anim;
            const uint8_t *fh0;
            if (blob && r01_cart_player_anim_parse(blob, wv.len - wv.off_player_anim, &anim) == 0) {
                if (state_idx < 0 || state_idx >= anim.state_count) {
                    state_idx = 0;
                }
                fh0 = r01_cart_player_anim_frame_hdr(&anim, state_idx, 0);
                if (fh0) {
                    box_x = origin_x + (int)fh0[2] - (int)fh0[0];
                    box_y = origin_y + (int)fh0[3] - (int)fh0[1];
                    box_w = (int)fh0[4];
                    box_h = (int)fh0[5];
                }
            }
        } else {
            /* Hitbox in WHDR is first-drawable-frame origin relative. */
            box_x = origin_x + (int)wv.player_hit_x;
            box_y = origin_y + (int)wv.player_hit_y;
            box_w = (int)wv.player_hit_w;
            box_h = (int)wv.player_hit_h;
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

static int player_move_ok(R01eMachine *m, int ox, int oy) {
    int hx, hy, hw, hh;
    int state_idx = r01_play_anim_entity_state(&m->play.anim);
    player_hit_rect(m, ox, oy, state_idx, &hx, &hy, &hw, &hh);
    return r01e_cart_aabb_ok(&m->cart, (int)m->io.world, hx, hy, hw, hh);
}

static int play_origin_ok(void *user, int ox, int oy) {
    return player_move_ok((R01eMachine *)user, ox, oy);
}

static void update_camera(R01ePlay *pl) {
    r01_play_camera_update(&pl->cam_x, &pl->cam_y, pl->player_x, pl->player_y, R01E_PLAY_PLAYER_W,
                           R01E_PLAY_PLAYER_H, R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, pl->cam_deadzone_x,
                           pl->cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
}

static void snap_camera(R01ePlay *pl) {
    r01_play_camera_snap(&pl->cam_x, &pl->cam_y, pl->player_x, pl->player_y, R01E_PLAY_PLAYER_W,
                         R01E_PLAY_PLAYER_H, R01E_SCREEN_PX_W, R01E_SCREEN_PX_H, pl->cam_deadzone_x,
                         pl->cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
}

/*
 * Clamp play cam to the present-screen bounding box (same as video.cam_max_*).
 * Do not shove the camera away from sparse holes inside that box: on L-shaped
 * maps that fight kills follow scroll and can push the player fully off-screen
 * (OAM cull). Missing BG1 slots sample BG0 by design.
 */
static void clamp_cam_to_world_bounds(R01eMachine *m) {
    R01ePlay *pl;
    int max_x;
    int max_y;

    if (!m || !m->play.enabled) {
        return;
    }
    pl = &m->play;
    max_x = m->video.cam_max_x;
    max_y = m->video.cam_max_y;
    if (pl->cam_x < 0) {
        pl->cam_x = 0;
    }
    if (pl->cam_y < 0) {
        pl->cam_y = 0;
    }
    if (max_x >= 0 && pl->cam_x > max_x) {
        pl->cam_x = max_x;
    }
    if (max_y >= 0 && pl->cam_y > max_y) {
        pl->cam_y = max_y;
    }
}

static void play_load_cart_camera(R01eMachine *m) {
    R01eWorldView wv;
    const uint8_t *prg;
    if (!m) {
        return;
    }
    r01_play_physics_init(&m->play.phys);
    m->play.cam_deadzone_x = R01_PLAY_CAM_DEADZONE_X_DEFAULT;
    m->play.cam_deadzone_y = R01_PLAY_CAM_DEADZONE_Y_DEFAULT;
    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) == 0) {
        int dx = (int)wv.cam_deadzone_x;
        int dy = (int)wv.cam_deadzone_y;
        /* 0,0 is valid (dead zone off / 1:1 track), not "missing". */
        if (dx <= R01E_SCREEN_PX_W && dy <= R01E_SCREEN_PX_H) {
            m->play.cam_deadzone_x = dx;
            m->play.cam_deadzone_y = dy;
        }
        if (wv.world_flags & R01E_CART_WHDR_FLAG_PLATFORMER) {
            r01_play_physics_set_mode(&m->play.phys, R01_GAME_MODE_PLATFORMER);
        }
    }
    prg = r01e_cart_prg(&m->cart);
    if (prg && m->cart.len_prg > R01E_PRG_PLAT_METER_OFF) {
        uint8_t grav = prg[R01E_PRG_PLAT_GRAVITY_OFF];
        uint8_t jump = prg[R01E_PRG_PLAT_JUMP_OFF];
        uint8_t meter = prg[R01E_PRG_PLAT_METER_OFF];
        if (grav) {
            r01_play_physics_set_gravity(&m->play.phys, (int)grav);
        }
        if (jump) {
            r01_play_physics_set_jump(&m->play.phys, (int)jump);
        }
        if (meter) {
            r01_play_physics_set_meter(&m->play.phys, (int)meter);
        }
    }
}

static void play_load_anim_maps(R01eMachine *m) {
    const uint8_t *prg;
    if (!m) {
        return;
    }
    prg = r01e_cart_prg(&m->cart);
    if (!prg || m->cart.len_prg <= R01E_PRG_PLAYER_ANIM_JUMP_OFF) {
        return;
    }
    {
        uint8_t crouch = prg[R01E_PRG_PLAT_CROUCH_OFF];
        if (crouch < R01_PLAY_ANIM_STATES_MAX) {
            r01_play_anim_set_crouch_state(&m->play.anim, (int)crouch);
        }
    }
    {
        uint8_t idle = prg[R01E_PRG_PLAYER_ANIM_IDLE_OFF];
        if (idle < R01_PLAY_ANIM_STATES_MAX) {
            r01_play_anim_set_idle_state(&m->play.anim, (int)idle);
        }
    }
    {
        uint8_t walk = prg[R01E_PRG_PLAYER_ANIM_WALK_OFF];
        if (walk < R01_PLAY_ANIM_STATES_MAX) {
            r01_play_anim_set_walk_all(&m->play.anim, (int)walk);
        }
    }
    {
        uint8_t jump_st = prg[R01E_PRG_PLAYER_ANIM_JUMP_OFF];
        if (jump_st < R01_PLAY_ANIM_STATES_MAX) {
            r01_play_anim_set_jump_state(&m->play.anim, (int)jump_st);
        }
    }
}

static void place_player_on_screen(R01ePlay *pl, int col, int row) {
    pl->player_x = R01E_PLAY_SPAWN_CENTER_X(col);
    pl->player_y = R01E_PLAY_SPAWN_CENTER_Y(row);
    r01_play_physics_reset_air(&pl->phys);
    snap_camera(pl);
}

static void place_player_xy(R01ePlay *pl, int wx, int wy) {
    pl->player_x = wx;
    pl->player_y = wy;
    r01_play_physics_reset_air(&pl->phys);
    snap_camera(pl);
}

/* First PRG instance of the marked player type (placements are not in the cart blob). */
static int player_instance_spawn(R01eMachine *m, int *out_x, int *out_y) {
    R01eWorldView wv;
    const uint8_t *prg;
    const uint8_t *insts;
    int inst_n;
    int ii;

    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) != 0) {
        return 0;
    }
    if (wv.player_entity == R01E_CART_PLAYER_ENTITY_NONE || wv.player_entity >= wv.entity_type_count) {
        return 0;
    }
    prg = r01e_cart_prg(&m->cart);
    if (!prg || m->cart.len_prg < R01E_PRG_PLAY_INST_TABLE_OFF + R01E_CART_INSTANCE_SIZE ||
        !cart_is_phase1_play(&m->cart)) {
        return 0;
    }
    inst_n = (int)prg[R01E_PRG_PLAY_INST_COUNT_OFF];
    if (inst_n < 1) {
        return 0;
    }
    if ((uint32_t)R01E_PRG_PLAY_INST_TABLE_OFF + (uint32_t)inst_n * R01E_CART_INSTANCE_SIZE > m->cart.len_prg) {
        return 0;
    }
    insts = prg + R01E_PRG_PLAY_INST_TABLE_OFF;
    for (ii = 0; ii < inst_n; ii++) {
        const uint8_t *irec = insts + (size_t)ii * R01E_CART_INSTANCE_SIZE;
        if (irec[0] != wv.player_entity) {
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

static int spawn_screen(R01eMachine *m, int *out_col, int *out_row) {
    const uint8_t *prg = r01e_cart_prg(&m->cart);
    int sc, sr;

    if (prg && m->cart.len_prg > R01E_PRG_PLAY_SPAWN_CELL_OFF && cart_is_phase1_play(&m->cart)) {
        uint8_t cell = prg[R01E_PRG_PLAY_SPAWN_CELL_OFF];
        sc = (int)(cell & 0x0fu);
        sr = (int)((cell >> 4) & 0x0fu);
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
                *out_col = R01E_CELL_COL(e[0]);
            }
            if (out_row) {
                *out_row = R01E_CELL_ROW(e[0]);
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
    } else {
        /* L0 must track every pixel of BG1 cam, not only screen crosses. */
        r01e_video_update_bg0_scroll(m);
    }
}

static int write_player_oam(R01eMachine *m, R01eWorldView *wv, int *slot) {
    R01ePlay *pl = &m->play;
    int player_type;

    if (!wv || !slot || *slot >= R01E_OAM_ENTRIES) {
        return 0;
    }
    if (wv->player_entity == R01E_CART_PLAYER_ENTITY_NONE || wv->player_entity >= wv->entity_type_count) {
        return 0;
    }
    player_type = (int)wv->player_entity;

    if (wv->has_player_anim) {
        const uint8_t *blob = r01e_cart_ptr(&m->cart, wv->base + wv->off_player_anim,
                                            wv->len > wv->off_player_anim ? wv->len - wv->off_player_anim : 0);
        R01CartPlayerAnim anim;
        int state_idx = r01_play_anim_entity_state(&pl->anim);
        int frame_slot = r01_play_anim_frame(&pl->anim);
        int flip_h = r01_play_anim_flip_h(&pl->anim);
        const uint8_t *fh;
        const uint8_t *parts;
        int part_count;
        int pi;
        int origin_x;
        int origin_y;
        if (!blob || r01_cart_player_anim_parse(blob, wv->len - wv->off_player_anim, &anim) != 0) {
            return 0;
        }
        if (state_idx < 0 || state_idx >= anim.state_count) {
            state_idx = 0;
        }
        fh = r01_cart_player_anim_frame_hdr(&anim, state_idx, frame_slot);
        if (!fh) {
            return 0;
        }
        parts = r01_cart_player_anim_frame_parts(&anim, state_idx, frame_slot, &part_count);
        if (!parts || part_count < 1) {
            return 0;
        }
        if (part_count > R01E_CART_ENTITY_PARTS_MAX) {
            part_count = R01E_CART_ENTITY_PARTS_MAX;
        }
        origin_x = (int)fh[0];
        origin_y = (int)fh[1];
        for (pi = 0; pi < part_count && *slot < R01E_OAM_ENTRIES; pi++) {
            const uint8_t *part = parts + (size_t)pi * 4u;
            int dx, dy;
            uint8_t attr;
            int sx, sy;
            uint8_t *oe;
            r01_cart_part_pose(origin_x, origin_y, (int)(int8_t)part[2], (int)(int8_t)part[3], part[1], flip_h, 0, &dx,
                               &dy, &attr);
            sx = pl->player_x + dx - origin_x - pl->cam_x;
            sy = pl->player_y + dy - origin_y - pl->cam_y;
            oe = &m->io.oam[(size_t)*slot * R01E_OAM_ENTRY_BYTES];
            if (r01e_oam_tile_off_screen(sx, sy)) {
                continue;
            }
            oe[0] = r01e_oam_coord_to_u8(sy);
            oe[1] = part[0];
            oe[2] = attr;
            oe[3] = r01e_oam_coord_to_u8(sx);
            (*slot)++;
        }
        return 1;
    }

    {
        const uint8_t *def = r01e_cart_entity_def(&m->cart, wv, player_type);
        const uint8_t *sprites = NULL;
        int part_count = 0;
        int pi;
        if (!def || r01e_cart_entity_frame(def, 0, 0, &sprites, &part_count) != 0 || !sprites) {
            return 0;
        }
        for (pi = 0; pi < part_count && *slot < R01E_OAM_ENTRIES; pi++) {
            const uint8_t *sp = sprites + (size_t)pi * 4u;
            int rx = (int)(int8_t)sp[1];
            int ry = (int)(int8_t)sp[2];
            int sx = pl->player_x + rx - pl->cam_x;
            int sy = pl->player_y + ry - pl->cam_y;
            uint8_t *oe = &m->io.oam[(size_t)*slot * R01E_OAM_ENTRY_BYTES];
            if (r01e_oam_tile_off_screen(sx, sy)) {
                continue;
            }
            oe[0] = r01e_oam_coord_to_u8(sy);
            oe[1] = sp[0];
            oe[2] = sp[3];
            oe[3] = r01e_oam_coord_to_u8(sx);
            (*slot)++;
        }
        return *slot > 0;
    }
}

static void write_oam(R01eMachine *m) {
    R01ePlay *pl = &m->play;
    R01eWorldView wv;
    const uint8_t *prg = NULL;
    const uint8_t *insts = NULL;
    int inst_n = 0;
    int slot = 0;
    int ii;
    int player_type = -1;
    int have_world = 0;

    memset(m->io.oam, 0xFF, sizeof(m->io.oam));

    if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) == 0) {
        have_world = 1;
        if (wv.player_entity != R01E_CART_PLAYER_ENTITY_NONE && wv.player_entity < wv.entity_type_count) {
            player_type = (int)wv.player_entity;
        }
    }

    if (player_type >= 0 && have_world) {
        (void)write_player_oam(m, &wv, &slot);
    }
    if (slot < 1) {
        int vx = pl->player_x - pl->cam_x;
        int vy = pl->player_y - pl->cam_y;
        if (!r01e_oam_tile_off_screen(vx, vy)) {
            m->io.oam[0] = r01e_oam_coord_to_u8(vy);
            m->io.oam[1] = 1; /* solid tile in SPR bank 0 */
            m->io.oam[2] = 0; /* bank 0, pal 0 */
            m->io.oam[3] = r01e_oam_coord_to_u8(vx);
            slot = 1;
        }
    }

    prg = r01e_cart_prg(&m->cart);
    if (have_world && prg && cart_is_phase1_play(&m->cart) &&
        m->cart.len_prg >= R01E_PRG_PLAY_INST_TABLE_OFF + R01E_CART_INSTANCE_SIZE) {
        inst_n = (int)prg[R01E_PRG_PLAY_INST_COUNT_OFF];
        if (inst_n > 0 &&
            (uint32_t)R01E_PRG_PLAY_INST_TABLE_OFF + (uint32_t)inst_n * R01E_CART_INSTANCE_SIZE <=
                m->cart.len_prg) {
            insts = prg + R01E_PRG_PLAY_INST_TABLE_OFF;
        } else {
            inst_n = 0;
        }
    }
    if (!have_world || !insts || inst_n < 1) {
        return;
    }
    for (ii = 0; ii < inst_n && slot < R01E_OAM_ENTRIES; ii++) {
        const uint8_t *irec = insts + (size_t)ii * R01E_CART_INSTANCE_SIZE;
        uint8_t type_id = irec[0];
        int world_x = (int)((uint16_t)irec[2] | ((uint16_t)irec[3] << 8));
        int world_y = (int)((uint16_t)irec[4] | ((uint16_t)irec[5] << 8));
        const uint8_t *def;
        const uint8_t *sprites = NULL;
        int part_count = 0;
        int pi;
        if (type_id >= wv.entity_type_count) {
            continue;
        }
        if (player_type >= 0 && (int)type_id == player_type) {
            continue;
        }
        def = r01e_cart_entity_def(&m->cart, &wv, (int)type_id);
        if (!def || r01e_cart_entity_frame(def, 0, 0, &sprites, &part_count) != 0 || !sprites) {
            continue;
        }
        for (pi = 0; pi < part_count && slot < R01E_OAM_ENTRIES; pi++) {
            const uint8_t *sp = sprites + (size_t)pi * 4u;
            int rx = (int)(int8_t)sp[1];
            int ry = (int)(int8_t)sp[2];
            uint8_t attr = sp[3];
            int sx, sy;
            uint8_t *oe;
            if (irec[1] & 1u) {
                rx = -rx - 8;
                attr = (uint8_t)(attr ^ R01E_ATTR_FLIP_H);
            }
            if (irec[1] & 2u) {
                ry = -ry - 8;
                attr = (uint8_t)(attr ^ R01E_ATTR_FLIP_V);
            }
            sx = world_x + rx - pl->cam_x;
            sy = world_y + ry - pl->cam_y;
            oe = &m->io.oam[(size_t)slot * R01E_OAM_ENTRY_BYTES];
            if (r01e_oam_tile_off_screen(sx, sy)) {
                continue;
            }
            oe[0] = r01e_oam_coord_to_u8(sy);
            oe[1] = sp[0];
            oe[2] = attr;
            oe[3] = r01e_oam_coord_to_u8(sx);
            slot++;
        }
    }
}

void r01e_play_reset(R01ePlay *play) {
    if (!play) {
        return;
    }
    memset(play, 0, sizeof(*play));
    play->player_w = R01E_PLAY_PLAYER_W;
    play->player_h = R01E_PLAY_PLAYER_H;
    r01_play_physics_init(&play->phys);
}

int r01e_play_start(R01eMachine *m) {
    int col = 0, row = 0;
    int sx, sy;

    if (!m) {
        return 0;
    }
    r01e_play_reset(&m->play);
    play_load_cart_camera(m);
    if (cart_is_c_prg(&m->cart)) {
        m->play.enabled = 1;
        if (m->ram[0x02E8]) {
            play_follow_c_sys(m);
            clamp_cam_to_world_bounds(m);
            r01e_play_sync_video(m);
            (void)r01e_video_sync_camera(m);
        }
        return 1;
    }
    /* Phase 1 carts (R01P) always run Studio-Play-equivalent runtime from cart MAP. */
    if (!cart_is_phase1_play(&m->cart) && !r01e_cart_has_screen(&m->cart, 0, R01E_START_COL, R01E_START_ROW)) {
        /* Still try if any screens exist. */
    }
    if (player_instance_spawn(m, &sx, &sy)) {
        m->play.enabled = 1;
        r01_play_anim_init(&m->play.anim);
        play_load_anim_maps(m);
        place_player_xy(&m->play, sx, sy);
        clamp_cam_to_world_bounds(m);
        r01e_play_sync_video(m);
        (void)r01e_video_sync_camera(m);
        write_oam(m);
        return 1;
    }
    if (!spawn_screen(m, &col, &row)) {
        return 0;
    }
    m->play.enabled = 1;
    r01_play_anim_init(&m->play.anim);
    play_load_anim_maps(m);
    place_player_on_screen(&m->play, col, row);
    clamp_cam_to_world_bounds(m);
    r01e_play_sync_video(m);
    (void)r01e_video_sync_camera(m);
    write_oam(m);
    return 1;
}

void r01e_play_tick(R01eMachine *m) {
    R01ePlay *pl;
    uint8_t pad;
    int dx = 0;
    int dy = 0;
    int anim_dx = 0;
    int anim_dy = 0;
    int jump_down = 0;

    if (!m || !m->play.enabled) {
        return;
    }
    if (cart_is_c_prg(&m->cart)) {
        if (m->ram[0x02E8]) {
            play_follow_c_sys(m);
            clamp_cam_to_world_bounds(m);
            r01e_play_sync_video(m);
        }
        return;
    }
    pl = &m->play;
    pad = m->io.pad0;
    pl->pad_prev = pad;

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
    if (pad & R01E_PAD_Y) {
        jump_down = 1;
    }
    {
        int crouch = 0;
        int phys_dx = dx;
        if (pl->phys.mode == R01_GAME_MODE_PLATFORMER && pl->phys.grounded && dy > 0 &&
            pl->anim.player_crouch_state >= 0) {
            crouch = 1;
            phys_dx = 0;
        }
        {
            int move_mul = 1;
            int delay_ov = 0;
            if (move_mul < 1) {
                move_mul = 1;
            }
            r01_play_physics_set_run_mul(&pl->phys, move_mul);
            r01_play_anim_set_frame_delay(&pl->anim, delay_ov);
        }
        r01_play_physics_tick(&pl->phys, &pl->player_x, &pl->player_y, phys_dx, dy, jump_down, play_origin_ok, m,
                              &anim_dx, &anim_dy);
        if (pl->phys.mode == R01_GAME_MODE_PLATFORMER && !pl->phys.grounded) {
            crouch = 0;
        }
        r01_play_anim_set_crouching(&pl->anim, crouch);
        r01_play_anim_set_airborne(&pl->anim, pl->phys.mode == R01_GAME_MODE_PLATFORMER && !pl->phys.grounded);
        r01_play_anim_update(&pl->anim, anim_dx, anim_dy);
    }
    /* No dead zone: camera tracks the player every tick. */
    update_camera(pl);
    clamp_cam_to_world_bounds(m);
    {
        R01eWorldView wv;
        if (r01e_cart_world(&m->cart, (int)m->io.world, &wv) == 0 && wv.has_player_anim) {
            const uint8_t *blob = r01e_cart_ptr(&m->cart, wv.base + wv.off_player_anim,
                                                wv.len > wv.off_player_anim ? wv.len - wv.off_player_anim : 0);
            R01CartPlayerAnim anim;
            if (blob && r01_cart_player_anim_parse(blob, wv.len - wv.off_player_anim, &anim) == 0) {
                r01_play_anim_tick_cart(&pl->anim, &anim);
            }
        }
    }
    r01e_play_sync_video(m);
    write_oam(m);
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
