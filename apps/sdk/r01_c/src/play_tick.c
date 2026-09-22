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
#define R01_PLAY_COLL_COUNT 0x8121u
#define R01_PLAY_COLL_DIR 0x8122u
#define R01_PLAY_INST_COUNT 0x81C0u
#define R01_PLAY_INST_TABLE 0x81C1u

static R01PlayPhysics s_phys;
static uint8_t s_phys_ready;

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
    uint8_t n;
    uint8_t i;
    int col;
    int row;
    int cell;
    (void)user;
    if (wx < 0 || wy < 0) {
        return 0;
    }
    col = wx / R01_SCREEN_PX_W;
    row = wy / R01_SCREEN_PX_H;
    cell = (wy % R01_SCREEN_PX_H) / 8 * R01_SCREEN_TILES_X + (wx % R01_SCREEN_PX_W) / 8;
    n = R01_CPU8(R01_PLAY_COLL_COUNT);
    for (i = 0; i < n; i++) {
        uint16_t e = (uint16_t)(R01_PLAY_COLL_DIR + (uint16_t)i * 4u);
        uint16_t tab;
        if ((int)R01_CPU8(e) != col || (int)R01_CPU8(e + 1u) != row) {
            continue;
        }
        tab = (uint16_t)R01_CPU8(e + 2u) | ((uint16_t)R01_CPU8(e + 3u) << 8);
        return R01_CPU8(tab + (uint16_t)cell) != 0u;
    }
    return 0;
}

static int move_ok_cart(void *user, int ox, int oy) {
    return r01_play_aabb_ok(ox, oy, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H, R01_SCREEN_PX_W, R01_SCREEN_PX_H,
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
    sx = (uint8_t)(ctx->cam_x % (uint16_t)R01_SCREEN_PX_W);
    sy = (uint8_t)(ctx->cam_y % (uint16_t)R01_SCREEN_PX_H);
    if (sx > 127u) {
        sx = 127u;
    }
    if (sy > 119u) {
        sy = 119u;
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
    int sx;
    int sy;
    uint8_t n;
    uint8_t i;
    if (!ctx) {
        return;
    }
    sx = (int)ctx->player_x - (int)ctx->cam_x;
    sy = (int)ctx->player_y - (int)ctx->cam_y;
    r01_oam_reset();
    if (sx + R01_PLAY_PLAYER_W > 0 && sy + R01_PLAY_PLAYER_H > 0 && sx < R01_SCREEN_PX_W &&
        sy < R01_SCREEN_PX_H) {
        r01_oam_write((uint8_t)sy, 0, 0, (uint8_t)sx);
    }
#ifndef R01_HOST_TEST
    n = R01_CPU8(R01_PLAY_INST_COUNT);
    for (i = 1u; i < n && i < 63u; i++) {
        uint16_t rec = (uint16_t)(R01_PLAY_INST_TABLE + (uint16_t)i * 6u);
        uint16_t wx = (uint16_t)R01_CPU8(rec + 2u) | ((uint16_t)R01_CPU8(rec + 3u) << 8);
        uint16_t wy = (uint16_t)R01_CPU8(rec + 4u) | ((uint16_t)R01_CPU8(rec + 5u) << 8);
        sx = (int)wx - (int)ctx->cam_x;
        sy = (int)wy - (int)ctx->cam_y;
        if (sx + 8 <= 0 || sy + 8 <= 0 || sx >= R01_SCREEN_PX_W || sy >= R01_SCREEN_PX_H) {
            continue;
        }
        r01_oam_write((uint8_t)sy, 0, 0, (uint8_t)sx);
    }
#else
    (void)n;
    (void)i;
#endif
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
    r01_sys_publish(ctx);
    r01_game_draw_player(ctx);
}
