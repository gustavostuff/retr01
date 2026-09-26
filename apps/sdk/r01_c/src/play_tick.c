#include "r01_engine.h"

#include "r01_cart_caps.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"
#include "r01_play_sys.h"
#define R01_PLAY_SPAWN_CELL 0x20u
#define R01_PLAY_INST_COUNT 0xC0u
#define R01_PLAY_INST_TABLE 0xC1u
#define R01_SOLID_RAM 0x0200u

static R01PlayPhysics s_phys;
static uint8_t s_phys_ready;
static uint8_t s_draw_air;
static uint8_t s_draw_crouch;
static int8_t s_draw_adx;
static int8_t s_draw_ady;
static int8_t s_hit_dx;
static int8_t s_hit_dy;
static uint8_t s_hit_w = 8;
static uint8_t s_hit_h = 8;
static const R01GameCtx *s_coll_ctx;

void r01_game_play_reset(void) {
    s_phys_ready = 0;
}

static void put_u16_ram(uint16_t addr, uint16_t v) {
    R01_CPU8(addr) = (uint8_t)(v & 0xFFu);
    R01_CPU8(addr + 1u) = (uint8_t)(v >> 8);
}

static int pattern_solid(uint8_t bank, uint8_t tile) {
    uint8_t n = R01_CPU8(R01_SOLID_RAM);
    uint8_t i;
    for (i = 0; i < n; i++) {
        if (R01_CPU8(R01_SOLID_RAM + 1u + (uint16_t)i * 2u) == bank &&
            R01_CPU8(R01_SOLID_RAM + 2u + (uint16_t)i * 2u) == tile) {
            return 1;
        }
    }
    if (s_coll_ctx) {
        n = s_coll_ctx->solid_pat_count;
        for (i = 0; i < n; i++) {
            if (s_coll_ctx->solid_pat_bank[i] == bank && s_coll_ctx->solid_pat_tile[i] == tile) {
                return 1;
            }
        }
    }
    return 0;
}

static int play_solid_cell(uint8_t col, uint8_t row, uint8_t cell) {
    uint8_t tile;
    uint8_t attr;
    if (col > 15u || row > 15u) {
        return 1;
    }
    if (r01_map_tile_at(col, row, cell, &tile, &attr) != 0) {
        return 1;
    }
    return pattern_solid((uint8_t)(attr & 0x0Fu), tile);
}

static int cart_aabb_ok(uint16_t px, uint16_t py, uint8_t bw, uint8_t bh) {
    uint16_t x1;
    uint16_t y1;
    uint16_t tx;
    uint16_t ty;
    uint16_t tx0;
    uint16_t ty0;
    uint16_t tx1;
    uint16_t ty1;
    static uint16_t s_tx0 = 0xFFFFu;
    static uint16_t s_ty0;
    static uint16_t s_tx1;
    static uint16_t s_ty1;
    static uint8_t s_ok;
    if (bw < 1u || bh < 1u) {
        return 0;
    }
    x1 = (uint16_t)(px + (uint16_t)bw - 1u);
    y1 = (uint16_t)(py + (uint16_t)bh - 1u);
    if (x1 >= (uint16_t)(R01_GRID_MAX * R01_SCREEN_PX_W) || y1 >= (uint16_t)(R01_GRID_MAX * R01_SCREEN_PX_H)) {
        return 0;
    }
    tx0 = (uint16_t)(px >> 3);
    ty0 = (uint16_t)(py >> 3);
    tx1 = (uint16_t)(x1 >> 3);
    ty1 = (uint16_t)(y1 >> 3);
    if (tx0 == s_tx0 && ty0 == s_ty0 && tx1 == s_tx1 && ty1 == s_ty1) {
        return s_ok;
    }
    s_ok = 0;
    for (ty = ty0; ty <= ty1; ty++) {
        uint8_t row = 0;
        uint16_t ly = ty;
        while (ly >= (uint16_t)R01_SCREEN_TILES_Y) {
            ly = (uint16_t)(ly - (uint16_t)R01_SCREEN_TILES_Y);
            row++;
            if (row > 15u) {
                s_tx0 = tx0;
                s_ty0 = ty0;
                s_tx1 = tx1;
                s_ty1 = ty1;
                return 0;
            }
        }
        for (tx = tx0; tx <= tx1; tx++) {
            uint8_t col = (uint8_t)(tx >> 4);
            uint8_t cell = (uint8_t)(ly * (uint16_t)R01_SCREEN_TILES_X + (tx & 15u));
            if (play_solid_cell(col, row, cell)) {
                s_tx0 = tx0;
                s_ty0 = ty0;
                s_tx1 = tx1;
                s_ty1 = ty1;
                return 0;
            }
        }
    }
    s_tx0 = tx0;
    s_ty0 = ty0;
    s_tx1 = tx1;
    s_ty1 = ty1;
    s_ok = 1;
    return 1;
}

static int move_ok_cart(void *user, uint16_t ox, uint16_t oy) {
    int16_t hx;
    int16_t hy;
    (void)user;
    hx = (int16_t)((int16_t)ox + (int16_t)s_hit_dx);
    hy = (int16_t)((int16_t)oy + (int16_t)s_hit_dy);
    if (hx < 0 || hy < 0) {
        return 0;
    }
    return cart_aabb_ok((uint16_t)hx, (uint16_t)hy, s_hit_w, s_hit_h);
}

int r01_world_aabb_ok(int x, int y, uint8_t w, uint8_t h) {
    if (x < 0 || y < 0) {
        return 0;
    }
    if (w < 1u) {
        w = (uint8_t)R01_PLAY_PLAYER_W;
    }
    if (h < 1u) {
        h = (uint8_t)R01_PLAY_PLAYER_H;
    }
    return cart_aabb_ok((uint16_t)x, (uint16_t)y, w, h);
}

void r01_game_spawn(R01GameCtx *ctx) {
    uint8_t n;
    if (!ctx) {
        return;
    }
    n = R01_CPU8((uint16_t)(r01_play_base() + R01_PLAY_INST_COUNT));
    if (n > 0u) {
        uint16_t rec = (uint16_t)(r01_play_base() + R01_PLAY_INST_TABLE);
        ctx->player_x = (uint16_t)R01_CPU8(rec + 2u) | ((uint16_t)R01_CPU8(rec + 3u) << 8);
        ctx->player_y = (uint16_t)R01_CPU8(rec + 4u) | ((uint16_t)R01_CPU8(rec + 5u) << 8);
    } else {
        uint8_t cell = R01_CPU8((uint16_t)(r01_play_base() + R01_PLAY_SPAWN_CELL));
        uint8_t col = (uint8_t)(cell & 0x0Fu);
        uint8_t row = (uint8_t)((cell >> 4) & 0x0Fu);
        ctx->player_x = (uint16_t)((uint16_t)col * (uint16_t)R01_SCREEN_PX_W +
                                   (uint16_t)((R01_SCREEN_PX_W - R01_PLAY_PLAYER_W) / 2));
        ctx->player_y = (uint16_t)((uint16_t)row * (uint16_t)R01_SCREEN_PX_H +
                                   (uint16_t)((R01_SCREEN_PX_H - R01_PLAY_PLAYER_H) / 2));
    }
}

void r01_game_camera_snap(R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    r01_play_camera_snap(&ctx->cam_x, &ctx->cam_y, ctx->player_x, ctx->player_y, (uint8_t)R01_PLAY_PLAYER_W,
                         (uint8_t)R01_PLAY_PLAYER_H, (uint8_t)R01_SCREEN_PX_W, (uint8_t)R01_SCREEN_PX_H,
                         ctx->cam_deadzone_x, ctx->cam_deadzone_y, ctx->cam_axis_lock);
}

void r01_scroll_publish(const R01GameCtx *ctx) {
    uint8_t sx;
    uint8_t sy;
    if (!ctx) {
        return;
    }
    sx = (uint8_t)(ctx->cam_x & 127u);
    {
        uint16_t y = ctx->cam_y;
        while (y >= (uint16_t)R01_SCREEN_PX_H) {
            y -= (uint16_t)R01_SCREEN_PX_H;
        }
        sy = (uint8_t)y;
    }
    *R01_SCROLL_X = sx;
    *R01_SCROLL_Y = sy;
}

void r01_sys_publish(const R01GameCtx *ctx) {
    if (!ctx) {
        return;
    }
    put_u16_ram(R01_SYS_PLAYER_X, ctx->player_x);
    put_u16_ram(R01_SYS_PLAYER_Y, ctx->player_y);
    put_u16_ram(R01_SYS_CAM_X, ctx->cam_x);
    put_u16_ram(R01_SYS_CAM_Y, ctx->cam_y);
    R01_CPU8(R01_SYS_READY) = 1;
    R01_CPU8(R01_SYS_VID_FLAGS) =
        r01_sys_vid_flags_pack(ctx->bg0_wrap_x, ctx->bg0_wrap_y, ctx->bg0_clip_bg1);
    r01_scroll_publish(ctx);
}

void r01_game_play_tick(R01GameCtx *ctx) {
    uint16_t px;
    uint16_t py;
    int8_t dx = 0;
    int8_t dy = 0;
    int8_t adx = 0;
    int8_t ady = 0;
    uint8_t jump;

    if (!ctx) {
        return;
    }
    if (!s_phys_ready) {
        r01_play_physics_init(&s_phys);
        r01_play_physics_set_mode(&s_phys, ctx->game_mode);
        r01_play_physics_set_gravity(&s_phys, ctx->plat_gravity);
        r01_play_physics_set_jump(&s_phys, ctx->plat_jump);
        r01_play_physics_set_meter(&s_phys, ctx->plat_meter);
        s_phys_ready = 1;
        r01_player_hit_get(&s_hit_dx, &s_hit_dy, &s_hit_w, &s_hit_h);
        if (s_hit_w < 1u) {
            s_hit_w = (uint8_t)R01_PLAY_PLAYER_W;
        }
        if (s_hit_h < 1u) {
            s_hit_h = (uint8_t)R01_PLAY_PLAYER_H;
        }
    }
    r01_play_physics_set_run_mul(&s_phys, ctx->player_move_mul);

    if (ctx->pad & R01_PAD_RIGHT) {
        dx = 1;
    }
    if (ctx->pad & R01_PAD_LEFT) {
        dx = -1;
    }
    if (ctx->pad & R01_PAD_DOWN) {
        dy = 1;
    }
    if (ctx->pad & R01_PAD_UP) {
        dy = -1;
    }
    jump = (uint8_t)((ctx->pad & R01_PAD_Y) != 0);

    px = ctx->player_x;
    py = ctx->player_y;
    s_coll_ctx = ctx;
    r01_play_physics_tick(&s_phys, &px, &py, dx, dy, jump, move_ok_cart, ctx, &adx, &ady);
    ctx->player_x = px;
    ctx->player_y = py;
    ctx->player_anim_moving = (uint8_t)((adx != 0) || (ady != 0));

    r01_play_camera_update(&ctx->cam_x, &ctx->cam_y, px, py, (uint8_t)R01_PLAY_PLAYER_W, (uint8_t)R01_PLAY_PLAYER_H,
                           (uint8_t)R01_SCREEN_PX_W, (uint8_t)R01_SCREEN_PX_H, ctx->cam_deadzone_x,
                           ctx->cam_deadzone_y, ctx->cam_axis_lock);
    r01_sys_publish(ctx);
    s_draw_air = 0;
    s_draw_crouch = 0;
    s_draw_adx = adx;
    s_draw_ady = ady;
    if (ctx->game_mode == R01_GAME_MODE_PLATFORMER && !s_phys.grounded) {
        s_draw_air = 1;
    }
    if (ctx->game_mode == R01_GAME_MODE_PLATFORMER && s_phys.grounded && (ctx->pad & R01_PAD_DOWN)) {
        s_draw_crouch = 1;
    }
    r01_game_anim_tick(ctx, (int)s_draw_air, (int)s_draw_crouch, (int)s_draw_adx, (int)s_draw_ady);
    r01_game_draw_sprites(ctx, (int)s_draw_air, (int)s_draw_crouch, (int)s_draw_adx, (int)s_draw_ady);
}
