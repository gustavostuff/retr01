#include "retr01_emu/play.h"

#include "retr01_emu/cart.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/video.h"
#include "r01_play_anim_cart.h"
#include "r01_play_camera.h"

#include <string.h>

/* Emu Host Play SoT (docs). Keep algorithms aligned with Studio play.c. */

/* Phase-1 PRG play table (must match apps/studio/core/src/prg_phase1.c). */
#define R01E_PRG_PLAY_SPAWN_CELL_OFF 0x0120u
#define R01E_PRG_PLAY_INST_COUNT_OFF 0x01C0u
#define R01E_PRG_PLAY_INST_TABLE_OFF 0x01C1u

static R01ePlaySfxFn s_sfx_on_x;
static R01ePlaySfxFn s_sfx_on_y;

void r01e_play_set_sfx_on_x(R01ePlaySfxFn fn) {
    s_sfx_on_x = fn;
}

void r01e_play_set_sfx_on_y(R01ePlaySfxFn fn) {
    s_sfx_on_y = fn;
}

static int cart_is_phase1_play(const R01eCart *c) {
    const uint8_t *prg = r01e_cart_prg(c);
    if (!prg || c->len_prg < 0x0105u) {
        return 0;
    }
    return prg[0x00F0] == 'R' && prg[0x00F1] == '0' && prg[0x00F2] == '1' && prg[0x00F3] == 'P';
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
            const uint8_t *st = NULL;
            if (blob && r01_cart_player_anim_parse(blob, wv.len - wv.off_player_anim, &anim) == 0) {
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
        } else {
            /* Hitbox in WHDR is draw-origin relative (Studio bakes authoring origin). */
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
 * Keep the 128x120 viewport on present BG1 screens only. Sparse holes inside the
 * present bbox otherwise sample BG0 (often a solid blue field) until the next
 * VRAM slot reload fills that cell — looks like platforms changing palette.
 */
static void clamp_cam_viewport_to_present(R01eMachine *m) {
    R01ePlay *pl;
    int world;
    int guard;

    if (!m || !m->play.enabled) {
        return;
    }
    pl = &m->play;
    world = (int)m->io.world;
    for (guard = 0; guard < 64; guard++) {
        int x0 = pl->cam_x;
        int y0 = pl->cam_y;
        int x1 = x0 + R01E_SCREEN_PX_W - 1;
        int y1 = y0 + R01E_SCREEN_PX_H - 1;
        int c0 = x0 / R01E_SCREEN_PX_W;
        int c1 = x1 / R01E_SCREEN_PX_W;
        int r0 = y0 / R01E_SCREEN_PX_H;
        int r1 = y1 / R01E_SCREEN_PX_H;
        int fixed = 0;
        int c, r;

        if (x0 < 0) {
            pl->cam_x = 0;
            fixed = 1;
            continue;
        }
        if (y0 < 0) {
            pl->cam_y = 0;
            fixed = 1;
            continue;
        }
        for (r = r0; r <= r1; r++) {
            for (c = c0; c <= c1; c++) {
                int nx, ny;
                if (r01e_cart_has_screen(&m->cart, world, c, r)) {
                    continue;
                }
                if (c == c1 && c1 > c0) {
                    nx = c * R01E_SCREEN_PX_W - R01E_SCREEN_PX_W;
                    if (nx < 0) {
                        nx = 0;
                    }
                    if (nx < pl->cam_x) {
                        pl->cam_x = nx;
                        fixed = 1;
                    }
                } else if (c == c0) {
                    nx = (c + 1) * R01E_SCREEN_PX_W;
                    if (nx != pl->cam_x) {
                        pl->cam_x = nx;
                        fixed = 1;
                    }
                }
                if (r == r1 && r1 > r0) {
                    ny = r * R01E_SCREEN_PX_H - R01E_SCREEN_PX_H;
                    if (ny < 0) {
                        ny = 0;
                    }
                    if (ny < pl->cam_y) {
                        pl->cam_y = ny;
                        fixed = 1;
                    }
                } else if (r == r0) {
                    ny = (r + 1) * R01E_SCREEN_PX_H;
                    if (ny != pl->cam_y) {
                        pl->cam_y = ny;
                        fixed = 1;
                    }
                }
            }
        }
        if (!fixed) {
            break;
        }
    }
}

static void play_load_cart_camera(R01eMachine *m) {
    R01eWorldView wv;
    if (!m) {
        return;
    }
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
    }
}

static void place_player_on_screen(R01ePlay *pl, int col, int row) {
    pl->player_x = R01E_PLAY_SPAWN_CENTER_X(col);
    pl->player_y = R01E_PLAY_SPAWN_CENTER_Y(row);
    snap_camera(pl);
}

static void place_player_xy(R01ePlay *pl, int wx, int wy) {
    pl->player_x = wx;
    pl->player_y = wy;
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
        const uint8_t *st = NULL;
        const uint8_t *parts;
        int part_count;
        int pi;
        if (!blob || r01_cart_player_anim_parse(blob, wv->len - wv->off_player_anim, &anim) != 0) {
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
        if (part_count > R01E_CART_ENTITY_PARTS_MAX) {
            part_count = R01E_CART_ENTITY_PARTS_MAX;
        }
        for (pi = 0; pi < part_count && *slot < R01E_OAM_ENTRIES; pi++) {
            const uint8_t *part = parts + (size_t)pi * 4u;
            int origin_x = (int)st[0];
            int origin_y = (int)st[1];
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
}

int r01e_play_start(R01eMachine *m) {
    int col = 0, row = 0;
    int sx, sy;

    if (!m) {
        return 0;
    }
    r01e_play_reset(&m->play);
    play_load_cart_camera(m);
    /* Phase 1 carts (R01P) always run Studio-Play-equivalent runtime from cart MAP. */
    if (!cart_is_phase1_play(&m->cart) && !r01e_cart_has_screen(&m->cart, 0, R01E_START_COL, R01E_START_ROW)) {
        /* Still try if any screens exist. */
    }
    if (player_instance_spawn(m, &sx, &sy)) {
        m->play.enabled = 1;
        r01_play_anim_init(&m->play.anim);
        place_player_xy(&m->play, sx, sy);
        clamp_cam_viewport_to_present(m);
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
    place_player_on_screen(&m->play, col, row);
    clamp_cam_viewport_to_present(m);
    r01e_play_sync_video(m);
    (void)r01e_video_sync_camera(m);
    write_oam(m);
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
    clamp_cam_viewport_to_present(m);
    r01e_play_sync_video(m);
    write_oam(m);
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

    /* Studio: X -> (0,0) + SFX_X, Y -> (1,0) + SFX_Y (P1 only). */
    if (edge & R01E_PAD_X) {
        if (s_sfx_on_x) {
            s_sfx_on_x();
        }
        (void)warp_to(m, 0, 0);
    }
    if (edge & R01E_PAD_Y) {
        if (s_sfx_on_y) {
            s_sfx_on_y();
        }
        (void)warp_to(m, 1, 0);
    }

    if (pad & R01E_PAD_LEFT) {
        dx = -1;
    } else if (pad & R01E_PAD_RIGHT) {
        dx = 1;
    }
    if (pad & R01E_PAD_UP) {
        dy = -1;
    } else     if (pad & R01E_PAD_DOWN) {
        dy = 1;
    }

    r01_play_anim_update(&pl->anim, dx, dy);

    if (dx != 0) {
        int nx = pl->player_x + dx;
        if (player_move_ok(m, nx, pl->player_y)) {
            pl->player_x = nx;
        }
    }
    if (dy != 0) {
        int ny = pl->player_y + dy;
        if (player_move_ok(m, pl->player_x, ny)) {
            pl->player_y = ny;
        }
    }
    /* No dead zone: camera tracks the player every tick. */
    update_camera(pl);
    clamp_cam_viewport_to_present(m);
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
