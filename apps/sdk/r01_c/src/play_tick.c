#include "r01_engine.h"

#include "r01_cart_caps.h"
#include "r01_play_camera.h"
#include "r01_play_collision.h"
#include "r01_play_physics.h"

#define R01_SYS_PLAYER_X 0x02E0u
#define R01_SYS_PLAYER_Y 0x02E2u
#define R01_SYS_CAM_X 0x02E4u
#define R01_SYS_CAM_Y 0x02E6u
#define R01_SYS_READY 0x02E8u
#define R01_PLAY_PRESENT 0x8100u
#define R01_PLAY_SPAWN_CELL 0x8120u
#define R01_PLAY_INST_COUNT 0x81C0u
#define R01_PLAY_INST_TABLE 0x81C1u
#define R01_COLL_GRID 0x8500u

static R01PlayPhysics s_phys;
static uint8_t s_phys_ready;
static int s_draw_air;
static int s_draw_crouch;
static int s_draw_adx;
static int s_draw_ady;
#ifndef R01_HOST_TEST
static int s_hit_dx;
static int s_hit_dy;
static uint8_t s_hit_w = 8;
static uint8_t s_hit_h = 8;
#endif

void r01_game_play_reset(void) {
    s_phys_ready = 0;
}

#ifdef R01_HOST_TEST
static int move_ok_open(void *user, int x, int y) {
    (void)user;
    (void)x;
    (void)y;
    return 1;
}
#endif

#ifndef R01_HOST_TEST
#define R01_CPU8(addr) (*(volatile uint8_t *)(uint16_t)(addr))

static void put_u16_ram(uint16_t addr, uint16_t v) {
    R01_CPU8(addr) = (uint8_t)(v & 0xFFu);
    R01_CPU8(addr + 1u) = (uint8_t)(v >> 8);
}

static int play_has_screen(void *user, int col, int row) {
    (void)user;
    if (col < 0 || col > 15 || row < 0 || row > 15) {
        return 0;
    }
    return (int)((R01_CPU8(R01_PLAY_PRESENT + (uint16_t)row * 2u + (uint16_t)col / 8u) >>
                  ((unsigned)col % 8u)) &
                 1u);
}

static int play_solid_at(void *user, int wx, int wy) {
    int col;
    int row;
    int lx;
    int ly;
    int cell;
    uint16_t tab;
    uint16_t slot;
    (void)user;
    if (wx < 0 || wy < 0) {
        return 0;
    }
    col = wx >> 7;
    lx = wx & 127;
    row = 0;
    ly = wy;
    while (ly >= R01_SCREEN_PX_H) {
        ly -= R01_SCREEN_PX_H;
        row++;
        if (row > 15) {
            return 0;
        }
    }
    if (col > 15) {
        return 0;
    }
    slot = (uint16_t)(R01_COLL_GRID + ((uint16_t)row * 16u + (uint16_t)col) * 2u);
    tab = (uint16_t)R01_CPU8(slot) | ((uint16_t)R01_CPU8(slot + 1u) << 8);
    if (tab == 0u) {
        return 0;
    }
    cell = (ly >> 3) * R01_SCREEN_TILES_X + (lx >> 3);
    return R01_CPU8(tab + (uint16_t)cell) != 0u;
}

static int move_ok_cart(void *user, int ox, int oy) {
    ox += s_hit_dx;
    oy += s_hit_dy;
    if (ox < 0) {
        ox = 0;
    }
    if (oy < 0) {
        oy = 0;
    }
    return r01_play_aabb_ok(ox, oy, (int)s_hit_w, (int)s_hit_h, R01_SCREEN_PX_W, R01_SCREEN_PX_H,
                            play_has_screen, play_solid_at, user);
}
#endif

void r01_game_spawn(R01GameCtx *ctx) {
    uint8_t n;
    if (!ctx) {
        return;
    }
#ifdef R01_HOST_TEST
    (void)n;
    ctx->player_x = 40;
    ctx->player_y = 40;
#else
    n = R01_CPU8(R01_PLAY_INST_COUNT);
    if (n > 0u) {
        ctx->player_x = (uint16_t)R01_CPU8(R01_PLAY_INST_TABLE + 2u) |
                        ((uint16_t)R01_CPU8(R01_PLAY_INST_TABLE + 3u) << 8);
        ctx->player_y = (uint16_t)R01_CPU8(R01_PLAY_INST_TABLE + 4u) |
                        ((uint16_t)R01_CPU8(R01_PLAY_INST_TABLE + 5u) << 8);
    } else {
        uint8_t cell = R01_CPU8(R01_PLAY_SPAWN_CELL);
        int col = (int)(cell & 0x0Fu);
        int row = (int)((cell >> 4) & 0x0Fu);
        ctx->player_x = (uint16_t)(col * R01_SCREEN_PX_W + (R01_SCREEN_PX_W - R01_PLAY_PLAYER_W) / 2);
        ctx->player_y = (uint16_t)(row * R01_SCREEN_PX_H + (R01_SCREEN_PX_H - R01_PLAY_PLAYER_H) / 2);
    }
#endif
}

void r01_game_camera_snap(R01GameCtx *ctx) {
    int cx;
    int cy;
    if (!ctx) {
        return;
    }
    cx = (int)ctx->cam_x;
    cy = (int)ctx->cam_y;
    r01_play_camera_snap(&cx, &cy, (int)ctx->player_x, (int)ctx->player_y, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H,
                         R01_SCREEN_PX_W, R01_SCREEN_PX_H, (int)ctx->cam_deadzone_x, (int)ctx->cam_deadzone_y,
                         (int)ctx->cam_axis_lock);
    ctx->cam_x = (uint16_t)cx;
    ctx->cam_y = (uint16_t)cy;
}

void r01_sys_publish(const R01GameCtx *ctx) {
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
#ifndef R01_HOST_TEST
    put_u16_ram(R01_SYS_PLAYER_X, ctx->player_x);
    put_u16_ram(R01_SYS_PLAYER_Y, ctx->player_y);
    put_u16_ram(R01_SYS_CAM_X, ctx->cam_x);
    put_u16_ram(R01_SYS_CAM_Y, ctx->cam_y);
    R01_CPU8(R01_SYS_READY) = 1;
#endif
}

void r01_game_draw_player(const R01GameCtx *ctx) {
    r01_game_draw_sprites(ctx, s_draw_air, s_draw_crouch, s_draw_adx, s_draw_ady);
}

void r01_game_play_tick(R01GameCtx *ctx) {
    int px;
    int py;
    int dx = 0;
    int dy = 0;
    int adx = 0;
    int ady = 0;
    int cx;
    int cy;
    int jump;
    int (*ok)(void *, int, int);

    if (!ctx) {
        return;
    }
    if (!s_phys_ready) {
        r01_play_physics_init(&s_phys);
        s_phys_ready = 1;
    }
    r01_play_physics_set_mode(&s_phys, (int)ctx->game_mode);
    r01_play_physics_set_gravity(&s_phys, (int)ctx->plat_gravity);
    r01_play_physics_set_jump(&s_phys, (int)ctx->plat_jump);
    r01_play_physics_set_meter(&s_phys, (int)ctx->plat_meter);
    r01_play_physics_set_run_mul(&s_phys, (int)ctx->player_move_mul);

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
    jump = (ctx->pad & R01_PAD_Y) != 0;

    px = (int)ctx->player_x;
    py = (int)ctx->player_y;
#ifdef R01_HOST_TEST
    ok = move_ok_open;
#else
    r01_player_hit_get(&s_hit_dx, &s_hit_dy, &s_hit_w, &s_hit_h);
    if (s_hit_w < 1u) {
        s_hit_w = (uint8_t)R01_PLAY_PLAYER_W;
    }
    if (s_hit_h < 1u) {
        s_hit_h = (uint8_t)R01_PLAY_PLAYER_H;
    }
    ok = move_ok_cart;
#endif
    r01_play_physics_tick(&s_phys, &px, &py, dx, dy, jump, ok, ctx, &adx, &ady);
    ctx->player_x = (uint16_t)px;
    ctx->player_y = (uint16_t)py;
    ctx->player_anim_moving = (uint8_t)((adx != 0) || (ady != 0));

    cx = (int)ctx->cam_x;
    cy = (int)ctx->cam_y;
    r01_play_camera_update(&cx, &cy, px, py, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H, R01_SCREEN_PX_W, R01_SCREEN_PX_H,
                           (int)ctx->cam_deadzone_x, (int)ctx->cam_deadzone_y, (int)ctx->cam_axis_lock);
    ctx->cam_x = (uint16_t)cx;
    ctx->cam_y = (uint16_t)cy;
#ifndef R01_HOST_TEST
    r01_map_load_window(ctx->cam_x, ctx->cam_y);
    r01_bg0_publish(ctx);
#endif
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
}
