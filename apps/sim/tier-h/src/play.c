#include "retr01_sim/play.h"

#include "retr01_sim/board.h"
#include "retr01_sim/frame_log.h"
#include "retr01_sim/gamepad.h"
#include "avr128db28_s1.h"
#include "as6c62256.h"
#include "avr128db28_s2.h"
#include "beam_xy.h"
#include "video_sink.h"
#include "r01_play_anim_cart.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"
#include "r01_custom_logic_scan.h"
#include "r01_apu_cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Studio/emu move+camera math; sim applies 1 logical px per sim VBlank (game frame). */
static R01PlayPhysics s_phys;
static int s_phys_ready;
static int s_platformer;
static int s_anim_idle = -1;
static int s_anim_walk = -1;
static int s_anim_crouch = -1;
static int s_anim_jump = -1;

static void player_hit_rect(R01sBoard *b, int origin_x, int origin_y, int state_idx, int *hx, int *hy, int *hw,
                            int *hh) {
    int box_x = origin_x;
    int box_y = origin_y;
    int box_w = R01S_PLAY_PLAYER_W;
    int box_h = R01S_PLAY_PLAYER_H;
    if (b && b->cart_loaded && b->cart_player_entity != 0xFF &&
        b->cart_player_entity < b->cart_entity_type_count) {
        if (b->cart_off_player_anim != 0) {
            const uint8_t *img = b->cart_module.flash.mem;
            const uint8_t *blob = img + b->cart_off_player_anim;
            size_t blob_len = sizeof(b->cart_module.flash.mem) - (size_t)b->cart_off_player_anim;
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
            const uint8_t *img = b->cart_module.flash.mem;
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

static int catalog_player_box(R01sBoard *b, int ox, int oy, int state, int *hx, int *hy, int *hw, int *hh);

static int player_move_ok(R01sBoard *b, int ox, int oy) {
    int hx, hy, hw, hh;
    int state_idx = r01_play_anim_entity_state(&b->play.anim);
    if (!catalog_player_box(b, ox, oy, state_idx, &hx, &hy, &hw, &hh)) {
        player_hit_rect(b, ox, oy, state_idx, &hx, &hy, &hw, &hh);
    }
    return r01s_board_aabb_ok(b, hx, hy, hw, hh);
}

static void clamp_camera(R01sBoard *b) {
    R01sPlay *pl;
    if (!b) {
        return;
    }
    pl = &b->play;
    if (pl->cam_x < 0) {
        pl->cam_x = 0;
    }
    if (pl->cam_y < 0) {
        pl->cam_y = 0;
    }
    if (pl->cam_x > b->cam_max_x) {
        pl->cam_x = b->cam_max_x;
    }
    if (pl->cam_y > b->cam_max_y) {
        pl->cam_y = b->cam_max_y;
    }
}

static void update_camera(R01sBoard *b) {
    R01sPlay *pl = &b->play;
    uint16_t cx = (uint16_t)pl->cam_x;
    uint16_t cy = (uint16_t)pl->cam_y;
    r01_play_camera_update(&cx, &cy, (uint16_t)pl->player_x, (uint16_t)pl->player_y, R01S_PLAY_PLAYER_W,
                           R01S_PLAY_PLAYER_H, R01S_BG_SCREEN_PX_W, R01S_BG_SCREEN_PX_H, (uint8_t)pl->cam_deadzone_x,
                           (uint8_t)pl->cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
    pl->cam_x = (int)cx;
    pl->cam_y = (int)cy;
    clamp_camera(b);
}

static void snap_camera(R01sBoard *b) {
    R01sPlay *pl = &b->play;
    uint16_t cx = (uint16_t)pl->cam_x;
    uint16_t cy = (uint16_t)pl->cam_y;
    r01_play_camera_snap(&cx, &cy, (uint16_t)pl->player_x, (uint16_t)pl->player_y, R01S_PLAY_PLAYER_W,
                         R01S_PLAY_PLAYER_H, R01S_BG_SCREEN_PX_W, R01S_BG_SCREEN_PX_H, (uint8_t)pl->cam_deadzone_x,
                         (uint8_t)pl->cam_deadzone_y, R01_PLAY_CAM_AXIS_BOTH);
    pl->cam_x = (int)cx;
    pl->cam_y = (int)cy;
    clamp_camera(b);
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

/* Author game_logic.c is not running (boot overlay replaced PRG). Copy its mode and camera. */
static void cart_output_dir(const char *cart_path, char *out, size_t out_cap);

static void play_load_author_rules(R01sBoard *b) {
    char dir[512];
    char path[576];
    FILE *f;
    char line[256];
    int dx = 0;
    int dy = 0;

    if (!b) {
        return;
    }
    cart_output_dir(b->cart_path[0] ? b->cart_path : NULL, dir, sizeof(dir));
    if (snprintf(path, sizeof(path), "%s/game_logic.c", dir) >= (int)sizeof(path)) {
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "R01_GAME_MODE_PLATFORMER")) {
            s_platformer = 1;
        }
        if (sscanf(line, " r01_camera_set_deadzone ( ctx , %d , %d )", &dx, &dy) == 2 ||
            sscanf(line, " r01_camera_set_deadzone(ctx, %d, %d)", &dx, &dy) == 2) {
            if (dx > 0 && dy > 0 && dx <= R01S_BG_SCREEN_PX_W && dy <= R01S_BG_SCREEN_PX_H) {
                b->play.cam_deadzone_x = dx;
                b->play.cam_deadzone_y = dy;
            }
        }
        if (sscanf(line, " r01_player_anim_set_idle_state(ctx, %d)", &dx) == 1) {
            s_anim_idle = dx;
        }
        if (sscanf(line, " r01_player_anim_set_walk_all(ctx, %d)", &dx) == 1) {
            s_anim_walk = dx;
        }
        if (sscanf(line, " r01_player_anim_set_crouch_state(ctx, %d)", &dx) == 1) {
            s_anim_crouch = dx;
        }
        if (sscanf(line, " r01_player_anim_set_jump_state(ctx, %d)", &dx) == 1) {
            s_anim_jump = dx;
        }
    }
    fclose(f);
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
    img = b->cart_module.flash.mem;
    if ((size_t)b->cart_off_entity_insts + (size_t)b->cart_entity_inst_count * 6u >
        sizeof(b->cart_module.flash.mem)) {
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
        prg = b->cart_module.flash.mem + b->cart_off_prg;
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

static const uint8_t *catalog_def(const R01sBoard *b, int type_id, size_t *room_out) {
    const uint8_t *img;
    const uint8_t *dir;
    const uint8_t *def;
    size_t cap;
    uint16_t off;
    if (room_out) {
        *room_out = 0;
    }
    if (!b || type_id < 0 || type_id >= (int)b->cart_entity_type_count || b->cart_off_entity_types == 0) {
        return NULL;
    }
    img = b->cart_module.flash.mem;
    cap = sizeof(b->cart_module.flash.mem);
    if ((size_t)b->cart_off_entity_types + (size_t)b->cart_entity_type_count * 2u > cap) {
        return NULL;
    }
    dir = img + b->cart_off_entity_types;
    off = (uint16_t)dir[(size_t)type_id * 2u] | ((uint16_t)dir[(size_t)type_id * 2u + 1u] << 8);
    if (off == 0 || (size_t)b->cart_off_entity_types + (size_t)off + 12u > cap) {
        return NULL;
    }
    def = dir + off;
    if (room_out) {
        *room_out = cap - (size_t)(def - img);
    }
    return def;
}

/* EntityDef frame sprites: tile, rel_x, rel_y, attr. See software-api.md. */
static const uint8_t *frame_sprites(const uint8_t *def, size_t room, int state, int frame, int *out_n) {
    uint8_t sc;
    uint8_t fc;
    uint16_t soff;
    uint16_t foff;
    const uint8_t *st;
    const uint8_t *fr;
    int n;
    if (out_n) {
        *out_n = 0;
    }
    if (!def || room < 12u) {
        return NULL;
    }
    sc = def[1];
    if (sc > 4u) {
        sc = 4u;
    }
    if (state < 0 || state >= (int)sc) {
        return NULL;
    }
    soff = (uint16_t)def[4 + state * 2] | ((uint16_t)def[5 + state * 2] << 8);
    if (soff == 0 || (size_t)soff + 18u > room) {
        return NULL;
    }
    st = def + soff;
    fc = st[0];
    if (fc > 8u) {
        fc = 8u;
    }
    if (frame < 0 || frame >= (int)fc) {
        return NULL;
    }
    foff = (uint16_t)st[2 + frame * 2] | ((uint16_t)st[3 + frame * 2] << 8);
    if (foff == 0 || (size_t)soff + (size_t)foff + 6u > room) {
        return NULL;
    }
    fr = st + foff;
    n = (int)fr[1];
    if (n < 1) {
        return NULL;
    }
    if (n > 6) {
        n = 6;
    }
    if ((size_t)soff + (size_t)foff + 6u + (size_t)n * 4u > room) {
        return NULL;
    }
    if (out_n) {
        *out_n = n;
    }
    return fr + 6;
}

static int catalog_player_box(R01sBoard *b, int ox, int oy, int state, int *hx, int *hy, int *hw, int *hh) {
    const uint8_t *def;
    const uint8_t *parts;
    size_t room = 0;
    int n = 0;
    int dx;
    int dy;
    int dw;
    int dh;
    if (!b || b->cart_player_entity == 0xFF) {
        return 0;
    }
    def = catalog_def(b, (int)b->cart_player_entity, &room);
    parts = frame_sprites(def, room, state, 0, &n);
    if (!parts) {
        parts = frame_sprites(def, room, 0, 0, &n);
    }
    if (!parts) {
        return 0;
    }
    dx = (int)(int8_t)parts[-4];
    dy = (int)(int8_t)parts[-3];
    dw = (int)parts[-2];
    dh = (int)parts[-1];
    if (dw < 1) {
        dw = 8;
    }
    if (dh < 1) {
        dh = 8;
    }
    if (hx) {
        *hx = ox + dx;
    }
    if (hy) {
        *hy = oy + dy;
    }
    if (hw) {
        *hw = dw;
    }
    if (hh) {
        *hh = dh;
    }
    return 1;
}

static void catalog_anim_tick(R01sBoard *b) {
    R01PlayAnimCtx *ctx;
    const uint8_t *def;
    const uint8_t *st;
    const uint8_t *fr;
    size_t room = 0;
    uint8_t sc;
    uint8_t fc;
    uint8_t frame;
    uint8_t delay;
    uint16_t soff;
    uint16_t foff;

    if (!b) {
        return;
    }
    ctx = &b->play.anim;
    if (ctx->player_idle_state == (uint8_t)R01_PLAY_ANIM_UNMAPPED && ctx->player_anim_state == 0u) {
        ctx->player_anim_frame = 0;
        return;
    }
    def = catalog_def(b, (int)b->cart_player_entity, &room);
    if (!def || room < 12u) {
        return;
    }
    sc = def[1];
    if (sc > 4u) {
        sc = 4u;
    }
    if (ctx->player_anim_state >= sc) {
        return;
    }
    soff = (uint16_t)def[4 + ctx->player_anim_state * 2] | ((uint16_t)def[5 + ctx->player_anim_state * 2] << 8);
    if (soff == 0 || (size_t)soff + 4u > room) {
        return;
    }
    st = def + soff;
    fc = st[0];
    if (fc > 8u) {
        fc = 8u;
    }
    if (fc <= 1u) {
        ctx->player_anim_frame = 0;
        return;
    }
    frame = ctx->player_anim_frame;
    if (frame >= fc) {
        frame = 0;
        ctx->player_anim_frame = 0;
    }
    foff = (uint16_t)st[2 + frame * 2] | ((uint16_t)st[3 + frame * 2] << 8);
    if (foff == 0 || (size_t)soff + (size_t)foff + 1u > room) {
        return;
    }
    fr = st + foff;
    delay = r01_play_anim_frame_delay(ctx, fr[0]);
    ctx->player_anim_ctr++;
    if (ctx->player_anim_ctr < delay) {
        return;
    }
    ctx->player_anim_ctr = 0;
    ctx->player_anim_frame++;
    if (ctx->player_anim_frame >= fc) {
        ctx->player_anim_frame = 0;
    }
}

static void anim_boot(R01sBoard *b) {
    if (!b) {
        return;
    }
    r01_play_anim_init(&b->play.anim);
    if (s_anim_idle >= 0) {
        r01_play_anim_set_idle_state(&b->play.anim, s_anim_idle);
    }
    if (s_anim_walk >= 0) {
        r01_play_anim_set_walk_all(&b->play.anim, s_anim_walk);
    }
    if (s_anim_crouch >= 0) {
        r01_play_anim_set_crouch_state(&b->play.anim, s_anim_crouch);
    }
    if (s_anim_jump >= 0) {
        r01_play_anim_set_jump_state(&b->play.anim, s_anim_jump);
    }
}

static int oam_emit(R01sBoard *b, int slot, int ox, int oy, const uint8_t *parts, int n, int flip_h, int flip_v,
                    int tile_add) {
    int pi;
    for (pi = 0; pi < n && slot < R01_OAM_MAX; pi++) {
        const uint8_t *sp = parts + (size_t)pi * 4u;
        int rx = (int)(int8_t)sp[1];
        int ry = (int)(int8_t)sp[2];
        uint8_t attr = sp[3];
        int sx;
        int sy;
        if (flip_h) {
            rx = -rx - 8;
            attr = (uint8_t)(attr ^ R01_ATTR_FLIP_H);
        }
        if (flip_v) {
            ry = -ry - 8;
            attr = (uint8_t)(attr ^ R01_ATTR_FLIP_V);
        }
        sx = ox + rx - b->play.cam_x;
        sy = oy + ry - b->play.cam_y;
        if (r01s_oam_tile_off_screen(sx, sy)) {
            continue;
        }
        r01s_avr128db28_s1_oam_poke(&b->mcu_s1, (uint16_t)(slot * 4 + 0), r01s_oam_coord_to_u8(sy));
        r01s_avr128db28_s1_oam_poke(&b->mcu_s1, (uint16_t)(slot * 4 + 1),
                                   (uint8_t)(sp[0] + (uint8_t)tile_add));
        r01s_avr128db28_s1_oam_poke(&b->mcu_s1, (uint16_t)(slot * 4 + 2), attr);
        r01s_avr128db28_s1_oam_poke(&b->mcu_s1, (uint16_t)(slot * 4 + 3), r01s_oam_coord_to_u8(sx));
        slot++;
        b->health_saw_oam = 1;
    }
    return slot;
}

static int write_player_oam(R01sBoard *b, int *slot) {
    const uint8_t *def;
    const uint8_t *parts;
    size_t room = 0;
    int n = 0;
    int state_idx;
    int frame_slot;
    int before;

    if (!b || !slot || *slot >= R01_OAM_MAX || b->cart_player_entity == 0xFF) {
        return 0;
    }
    def = catalog_def(b, (int)b->cart_player_entity, &room);
    if (!def) {
        return 0;
    }
    state_idx = r01_play_anim_entity_state(&b->play.anim);
    frame_slot = r01_play_anim_frame(&b->play.anim);
    parts = frame_sprites(def, room, state_idx, frame_slot, &n);
    if (!parts) {
        parts = frame_sprites(def, room, 0, 0, &n);
    }
    if (!parts) {
        return 0;
    }
    before = *slot;
    *slot = oam_emit(b, *slot, b->play.player_x, b->play.player_y, parts, n,
                     r01_play_anim_flip_h(&b->play.anim), 0, 0);
    return *slot > before;
}


/* Host stand-in for example_01 slime_ai_tick. The boot overlay replaces PRG, so the 6502 never runs it. */
enum { NPC_N = 16, SLIME_JUMP_MIN = 30, SLIME_JUMP_MAX = 60, SLIME_JUMP_PX = 16 };

typedef struct NpcLive {
    uint8_t type;
    uint8_t flags;
    uint8_t state;
    uint8_t up;
    uint16_t wait;
    int x;
    int y;
} NpcLive;

static NpcLive s_npc[NPC_N];
static int s_npc_n;
static int s_npc_ready;
static uint16_t s_npc_rng = 0xACE1u;

static void npc_reset(void) {
    s_npc_n = 0;
    s_npc_ready = 0;
    s_phys_ready = 0;
    s_platformer = 0;
}

static int npc_gap(void) {
    uint16_t span = (uint16_t)(SLIME_JUMP_MAX - SLIME_JUMP_MIN);
    uint16_t mask = 1u;
    uint16_t r;
    s_npc_rng = (uint16_t)(s_npc_rng * 2053u + 13849u);
    while (mask < span) {
        mask = (uint16_t)((mask << 1) | 1u);
    }
    do {
        s_npc_rng = (uint16_t)(s_npc_rng * 2053u + 13849u);
        r = (uint16_t)(s_npc_rng & mask);
    } while (r > span);
    return (int)(SLIME_JUMP_MIN + r);
}

static int npc_fits(R01sBoard *b, int type, int x, int y) {
    const uint8_t *def;
    const uint8_t *parts;
    size_t room = 0;
    int n = 0;
    int hx, hy, hw, hh;
    def = catalog_def(b, type, &room);
    parts = frame_sprites(def, room, 0, 0, &n);
    if (!parts) {
        return r01s_board_aabb_ok(b, x, y, 8, 8);
    }
    hx = (int)(int8_t)parts[-4];
    hy = (int)(int8_t)parts[-3];
    hw = (int)parts[-2];
    hh = (int)parts[-1];
    if (hw < 1) {
        hw = 8;
    }
    if (hh < 1) {
        hh = 8;
    }
    return r01s_board_aabb_ok(b, x + hx, y + hy, hw, hh);
}

static void npc_boot(R01sBoard *b) {
    int i;
    s_npc_n = 0;
    for (i = 0; i < (int)b->cart_prg_spawn_n && s_npc_n < NPC_N; i++) {
        const uint8_t *irec = b->cart_prg_spawn + (size_t)i * 6u;
        NpcLive *n;
        if (b->cart_player_entity != 0xFF && irec[0] == b->cart_player_entity) {
            continue;
        }
        n = &s_npc[s_npc_n];
        n->type = irec[0];
        n->flags = irec[1];
        n->state = 0;
        n->up = 0;
        n->wait = (uint16_t)npc_gap();
        n->x = (int)((uint16_t)irec[2] | ((uint16_t)irec[3] << 8));
        n->y = (int)((uint16_t)irec[4] | ((uint16_t)irec[5] << 8));
        s_npc_n++;
    }
    s_npc_ready = 1;
}

static void npc_tick(R01sBoard *b) {
    int i;
    if (!b || !b->play.enabled) {
        return;
    }
    if (!s_npc_ready) {
        s_npc_rng ^= (uint16_t)b->play.player_x;
        s_npc_rng ^= (uint16_t)(b->play.player_y << 1);
        npc_boot(b);
    }
    for (i = 0; i < s_npc_n; i++) {
        NpcLive *n = &s_npc[i];
        int grounded;
        if (n->wait > 0) {
            n->wait--;
        }
        if (npc_fits(b, (int)n->type, n->x + 1, n->y)) {
            n->x++;
        }
        grounded = !npc_fits(b, (int)n->type, n->x, n->y + 1);
        if (n->wait == 0 && grounded) {
            n->up = (uint8_t)SLIME_JUMP_PX;
            n->wait = (uint16_t)npc_gap();
            grounded = 0;
        }
        if (n->up > 0) {
            if (n->y > 0 && npc_fits(b, (int)n->type, n->x, n->y - 1)) {
                n->y--;
                n->up--;
            } else {
                n->up = 0;
            }
            grounded = 0;
        } else if (!grounded) {
            n->y++;
        }
        n->state = grounded ? 0u : 1u;
    }
}

static void write_oam(R01sBoard *b) {
    int slot = 0;
    int ii;

    if (!b || !b->play.enabled) {
        return;
    }
    memset(b->mcu_s1.oam, 0xFF, sizeof(b->mcu_s1.oam));
    (void)write_player_oam(b, &slot);

    if (!s_npc_ready) {
        npc_boot(b);
    }
    for (ii = 0; ii < s_npc_n && slot < R01_OAM_MAX; ii++) {
        const NpcLive *n = &s_npc[ii];
        const uint8_t *def;
        const uint8_t *parts;
        size_t room = 0;
        int part_n = 0;
        def = catalog_def(b, (int)n->type, &room);
        parts = frame_sprites(def, room, 0, 0, &part_n);
        if (!parts) {
            continue;
        }
        slot = oam_emit(b, slot, n->x, n->y, parts, part_n, (n->flags & 1u) != 0, (n->flags & 2u) != 0,
                        n->state != 0);
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
    /* Host Play BG0: update every camera move (match emu, not only screen crosses). */
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
        int scroll_changed = pl->pending_scroll_x != r01s_board_peek_fe(b, 0x02u) ||
                             pl->pending_scroll_y != r01s_board_peek_fe(b, 0x03u);
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

static int move_ok_phys(void *ctx, uint16_t x, uint16_t y) {
    return player_move_ok((R01sBoard *)ctx, (int)x, (int)y);
}

static void step_move_from_pad(R01sBoard *b) {
    R01sPlay *pl;
    uint8_t pad;
    int8_t dx = 0;
    int8_t dy = 0;
    int8_t adx = 0;
    int8_t ady = 0;
    uint16_t px;
    uint16_t py;
    uint8_t jump;

    if (!b || !b->play.enabled) {
        return;
    }
    pl = &b->play;
    if (!s_phys_ready) {
        r01_play_physics_init(&s_phys);
        r01_play_physics_set_mode(&s_phys, s_platformer ? (uint8_t)R01_GAME_MODE_PLATFORMER
                                                       : (uint8_t)R01_GAME_MODE_TOPDOWN);
        r01_play_physics_set_gravity(&s_phys, (uint8_t)R01_PLAT_GRAVITY_DEFAULT);
        r01_play_physics_set_jump(&s_phys, (uint8_t)R01_PLAT_JUMP_DEFAULT);
        r01_play_physics_set_meter(&s_phys, (uint8_t)R01_PLAT_METER_DEFAULT);
        s_phys_ready = 1;
    }
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
    jump = (uint8_t)((pad & R01S_PAD_Y) != 0);
    r01_play_physics_set_run_mul(&s_phys, (dx != 0 && (pad & R01S_PAD_X)) ? 2u : 1u);
    px = (uint16_t)pl->player_x;
    py = (uint16_t)pl->player_y;
    r01_play_physics_tick(&s_phys, &px, &py, dx, dy, jump, move_ok_phys, b, &adx, &ady);
    pl->player_x = (int)px;
    pl->player_y = (int)py;
    r01_play_anim_set_airborne(&pl->anim, s_platformer && !s_phys.grounded);
    r01_play_anim_set_crouching(&pl->anim, s_platformer && s_phys.grounded && (pad & R01S_PAD_DOWN) != 0);
    r01_play_anim_update(&pl->anim, (int)adx, (int)ady);
    catalog_anim_tick(b);
    update_camera(b);
    if (b->cart_off_player_anim != 0) {
        const uint8_t *blob = b->cart_module.flash.mem + b->cart_off_player_anim;
        size_t blob_len = sizeof(b->cart_module.flash.mem) - (size_t)b->cart_off_player_anim;
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
    /* One game tick per painted field. UI frames run much faster than the beam. */
    step_move_from_pad(b);
    npc_tick(b);
    apply_video_latch(b);
    write_oam(b);
    r01s_frame_log_note(R01S_FLOG_PLAY, "VBlank Host Play: pad step + OAM");
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

static int path_is_file(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void cart_output_dir(const char *cart_path, char *out, size_t out_cap) {
    const char *slash;
    size_t n;
    if (!out || out_cap < 2) {
        return;
    }
    if (!cart_path || !cart_path[0]) {
        snprintf(out, out_cap, ".");
        return;
    }
    slash = strrchr(cart_path, '/');
    if (!slash) {
        slash = strrchr(cart_path, '\\');
    }
    if (!slash) {
        snprintf(out, out_cap, ".");
        return;
    }
    n = (size_t)(slash - cart_path);
    if (n + 1 > out_cap) {
        n = out_cap - 1;
    }
    memcpy(out, cart_path, n);
    out[n] = '\0';
}

/* Match emu: scan custom_logic for r01_bgm_play and feed that track into WAVE. */
static void play_start_bgm_viz(R01sBoard *board) {
    char out_dir[512];
    char logic[576];
    char bin[576];
    int track = 0;
    int i;
    const char *bgm_path = NULL;
    static const char *const logic_fallbacks[] = {
        "game_logic.c",
        "example_01/game_logic.c",
        "output/C/custom_logic.c",
        "../output/C/custom_logic.c",
        "../../output/C/custom_logic.c",
        NULL,
    };
#ifndef R01S_OUTPUT_DIR
#define R01S_OUTPUT_DIR "../output"
#endif

    if (!board || !board->apu_impl.apu) {
        return;
    }
    {
        const char *env = getenv("R01S_BGM_BIN");
        if (env && env[0]) {
            r01s_avr128db28_s2_viz_start(board->apu_impl.apu, 0, env);
            return;
        }
    }

    cart_output_dir(board->cart_path[0] ? board->cart_path : NULL, out_dir, sizeof(out_dir));
    logic[0] = '\0';
    if (snprintf(logic, sizeof(logic), "%s/game_logic.c", out_dir) >= (int)sizeof(logic) || !path_is_file(logic)) {
        logic[0] = '\0';
    }
    if (!logic[0] && r01_custom_logic_path_for_output(out_dir, logic, sizeof(logic)) == 0 && !path_is_file(logic)) {
        logic[0] = '\0';
    }
    if (!logic[0]) {
        for (i = 0; logic_fallbacks[i]; i++) {
            if (path_is_file(logic_fallbacks[i])) {
                snprintf(logic, sizeof(logic), "%s", logic_fallbacks[i]);
                break;
            }
        }
    }
    if (!logic[0] && path_is_file(R01S_OUTPUT_DIR "/game_logic.c")) {
        snprintf(logic, sizeof(logic), "%s/game_logic.c", R01S_OUTPUT_DIR);
    }
    if (!logic[0] && path_is_file(R01S_OUTPUT_DIR "/C/custom_logic.c")) {
        snprintf(logic, sizeof(logic), "%s/C/custom_logic.c", R01S_OUTPUT_DIR);
    }

    if (r01_custom_logic_scan_bgm_play(logic, &track) != 0) {
        fprintf(stderr, "play: no r01_bgm_play in %s\n", logic[0] ? logic : "(no game_logic.c)");
        r01s_avr128db28_s2_viz_stop(board->apu_impl.apu);
        return;
    }
    fprintf(stderr, "play: BGM track %d from %s\n", track, logic);

    {
        const uint8_t *img = board->cart_module.flash.mem;
        const uint8_t *ptrs;
        const uint8_t *blob;
        uint32_t boff;
        uint32_t blen;
        uint16_t payload_min = R01_BGM_HDR;
        uint16_t toff;
        uint16_t tlen;
        if (track >= 1 && track <= (int)R01_BGM_TRACKS && memcmp(img, "retr01", 6) == 0) {
            ptrs = img + R01S_CART_HDR_BYTES;
            boff = (uint32_t)ptrs[42] | ((uint32_t)ptrs[43] << 8) | ((uint32_t)ptrs[44] << 16);
            blen = (uint32_t)ptrs[45] | ((uint32_t)ptrs[46] << 8) | ((uint32_t)ptrs[47] << 16);
            if (blen >= R01_BGM_HDR && (size_t)boff + (size_t)blen <= sizeof(board->cart_module.flash.mem)) {
                blob = img + boff;
                if (blob[0] == R01_BGM_MAGIC0 && blob[1] == R01_BGM_MAGIC1) {
                    if (blob[3] == R01_BGM_INS_VER) {
                        payload_min = (uint16_t)R01_BGM_HDR_V1;
                    }
                    toff = (uint16_t)blob[4 + (track - 1) * 2] | ((uint16_t)blob[5 + (track - 1) * 2] << 8);
                    tlen = (uint16_t)blob[20 + (track - 1) * 2] | ((uint16_t)blob[21 + (track - 1) * 2] << 8);
                    if (toff >= payload_min && tlen >= 1u && (uint32_t)toff + (uint32_t)tlen <= blen) {
                        fprintf(stderr, "play: BGM bytecode %u bytes from cart\n", (unsigned)tlen);
                        r01s_avr128db28_s2_viz_start_bytecode(board->apu_impl.apu, 0, blob + toff, (int)tlen);
                        r01s_frame_log_note(R01S_FLOG_PLAY, "Host Play BGM from cart");
                        return;
                    }
                }
            }
        }
    }

    if (r01_bgm_track_bin_path(out_dir, track, bin, sizeof(bin)) == 0 && path_is_file(bin)) {
        bgm_path = bin;
    } else {
        static const char *const root_fallbacks[] = {
            R01S_OUTPUT_DIR,
            "output",
            "../output",
            "../../output",
            NULL,
        };
        for (i = 0; root_fallbacks[i]; i++) {
            char try_bin[576];
            if (r01_bgm_track_bin_path(root_fallbacks[i], track, try_bin, sizeof(try_bin)) == 0 &&
                path_is_file(try_bin)) {
                snprintf(bin, sizeof(bin), "%s", try_bin);
                bgm_path = bin;
                break;
            }
        }
    }

    if (!bgm_path && track != 1) {
        /* Non-1 tracks need an exported bin; don't substitute Track-1 builtin. */
        r01s_avr128db28_s2_viz_stop(board->apu_impl.apu);
        r01s_frame_log_note(R01S_FLOG_PLAY, "Host Play BGM: missing bgm_track bin");
        return;
    }
    r01s_avr128db28_s2_viz_start(board->apu_impl.apu, 0, bgm_path);
    r01s_frame_log_note(R01S_FLOG_PLAY, "Host Play BGM viz started");
}

int r01s_play_start(R01sBoard *board) {
    int col = 0, row = 0;
    int sx, sy;

    if (!board || !board->cart_loaded) {
        return 0;
    }
    r01s_play_reset(&board->play);
    npc_reset();
    play_load_cart_camera(board);
    play_load_author_rules(board);
    for (sx = 0; sx < (int)board->cart_prg_spawn_n; sx++) {
        const uint8_t *irec = board->cart_prg_spawn + (size_t)sx * 6u;
        if (irec[0] == board->cart_player_entity) {
            sy = (int)((uint16_t)irec[2] | ((uint16_t)irec[3] << 8));
            col = (int)((uint16_t)irec[4] | ((uint16_t)irec[5] << 8));
            r01s_board_mark_map_ready(board);
            anim_boot(board);
            place_player_xy(board, sy, col);
            goto play_latched;
        }
    }
    if (player_instance_spawn(board, &sx, &sy)) {
        r01s_board_mark_map_ready(board);
        anim_boot(board);
        place_player_xy(board, sx, sy);
    } else if (spawn_screen(board, &col, &row)) {
        r01s_board_mark_map_ready(board);
        anim_boot(board);
        place_player_on_screen(board, col, row);
    } else {
        return 0;
    }
play_latched:
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
    board->l0_show_half = 0;
    if (board->mcu_lb_impl.sram) {
        uint16_t ai;
        for (ai = 0; ai < 256u; ai++) {
            r01s_as6c62256_poke(board->mcu_lb_impl.sram, ai, 0);
        }
    }
    board->play.enabled = 1;
    write_oam(board);
    play_start_bgm_viz(board);
    r01s_frame_log_note(R01S_FLOG_PLAY, "Host Play enabled (scroll latched, OAM written, beam rewind)");
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
    pl->pad_held = pad;

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
