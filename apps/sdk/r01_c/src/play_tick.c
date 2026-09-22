#include "r01_engine.h"

#include "r01_cart_caps.h"
#include "r01_play_camera.h"
#include "r01_play_physics.h"

#define R01_SYS_PLAYER_X 0x02E0u
#define R01_SYS_PLAYER_Y 0x02E2u
#define R01_SYS_CAM_X 0x02E4u
#define R01_SYS_CAM_Y 0x02E6u
#define R01_SYS_READY 0x02E8u
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
static uint16_t s_sol_tab;
static uint8_t s_sol_col = 0xFFu;
static uint8_t s_sol_row = 0xFFu;
#endif

void r01_game_play_reset(void) {
    s_phys_ready = 0;
#ifndef R01_HOST_TEST
    s_sol_col = 0xFFu;
    s_sol_row = 0xFFu;
    s_sol_tab = 0;
#endif
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

static int play_solid_cell(uint8_t col, uint8_t row, uint8_t cell) {
    uint16_t tab;
    if (col > 15u || row > 15u) {
        return 1;
    }
    if (col != s_sol_col || row != s_sol_row) {
        uint16_t slot = (uint16_t)(R01_COLL_GRID + ((uint16_t)row * 16u + (uint16_t)col) * 2u);
        tab = (uint16_t)R01_CPU8(slot) | ((uint16_t)R01_CPU8(slot + 1u) << 8);
        s_sol_col = col;
        s_sol_row = row;
        s_sol_tab = tab;
    } else {
        tab = s_sol_tab;
    }
    if (tab == 0u) {
        return 1;
    }
    return R01_CPU8(tab + (uint16_t)cell) != 0u;
}

static int cart_aabb_ok(int px, int py, int bw, int bh) {
    int x1;
    int y1;
    int tx;
    int ty;
    int tx0;
    int ty0;
    int tx1;
    int ty1;
    static int s_tx0 = -1;
    static int s_ty0;
    static int s_tx1;
    static int s_ty1;
    static int s_ok;
    if (px < 0 || py < 0 || bw < 1 || bh < 1) {
        return 0;
    }
    x1 = px + bw - 1;
    y1 = py + bh - 1;
    if (x1 >= R01_GRID_MAX * R01_SCREEN_PX_W || y1 >= R01_GRID_MAX * R01_SCREEN_PX_H) {
        return 0;
    }
    tx0 = px >> 3;
    ty0 = py >> 3;
    tx1 = x1 >> 3;
    ty1 = y1 >> 3;
    if (tx0 == s_tx0 && ty0 == s_ty0 && tx1 == s_tx1 && ty1 == s_ty1) {
        return s_ok;
    }
    s_ok = 0;
    for (ty = ty0; ty <= ty1; ty++) {
        uint8_t row = 0;
        int ly = ty;
        while (ly >= R01_SCREEN_TILES_Y) {
            ly -= R01_SCREEN_TILES_Y;
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
            uint8_t col = (uint8_t)((unsigned)tx >> 4);
            uint8_t cell = (uint8_t)(ly * R01_SCREEN_TILES_X + (tx & 15));
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

static int move_ok_cart(void *user, int ox, int oy) {
    (void)user;
    ox += s_hit_dx;
    oy += s_hit_dy;
    if (ox < 0 || oy < 0) {
        return 0;
    }
    return cart_aabb_ok(ox, oy, (int)s_hit_w, (int)s_hit_h);
}
#endif

int r01_world_aabb_ok(int x, int y, uint8_t w, uint8_t h) {
#ifdef R01_HOST_TEST
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    return 1;
#else
    int ly;
    uint8_t row;
    if (x < 0 || y < 0) {
        return 0;
    }
    if (w <= 1u && h <= 1u) {
        ly = y >> 3;
        row = 0;
        while (ly >= R01_SCREEN_TILES_Y) {
            ly -= R01_SCREEN_TILES_Y;
            row++;
            if (row > 15u) {
                return 0;
            }
        }
        return !play_solid_cell((uint8_t)((unsigned)(x >> 3) >> 4), row,
                                (uint8_t)(ly * R01_SCREEN_TILES_X + ((x >> 3) & 15)));
    }
    if (w < 1u) {
        w = (uint8_t)R01_PLAY_PLAYER_W;
    }
    if (h < 1u) {
        h = (uint8_t)R01_PLAY_PLAYER_H;
    }
    return cart_aabb_ok(x, y, (int)w, (int)h);
#endif
}

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
#ifndef R01_HOST_TEST
    put_u16_ram(R01_SYS_PLAYER_X, ctx->player_x);
    put_u16_ram(R01_SYS_PLAYER_Y, ctx->player_y);
    put_u16_ram(R01_SYS_CAM_X, ctx->cam_x);
    put_u16_ram(R01_SYS_CAM_Y, ctx->cam_y);
    R01_CPU8(R01_SYS_READY) = 1;
#else
    r01_scroll_publish(ctx);
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
        r01_play_physics_set_mode(&s_phys, (int)ctx->game_mode);
        r01_play_physics_set_gravity(&s_phys, (int)ctx->plat_gravity);
        r01_play_physics_set_jump(&s_phys, (int)ctx->plat_jump);
        r01_play_physics_set_meter(&s_phys, (int)ctx->plat_meter);
        s_phys_ready = 1;
#ifndef R01_HOST_TEST
        r01_player_hit_get(&s_hit_dx, &s_hit_dy, &s_hit_w, &s_hit_h);
        if (s_hit_w < 1u) {
            s_hit_w = (uint8_t)R01_PLAY_PLAYER_W;
        }
        if (s_hit_h < 1u) {
            s_hit_h = (uint8_t)R01_PLAY_PLAYER_H;
        }
#endif
    }
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
    if (px < 0) {
        px = 0;
    }
    if (py < 0) {
        py = 0;
    }
    ctx->player_x = (uint16_t)px;
    ctx->player_y = (uint16_t)py;
    ctx->player_anim_moving = (uint8_t)((adx != 0) || (ady != 0));

    cx = (int)ctx->cam_x;
    cy = (int)ctx->cam_y;
    r01_play_camera_update(&cx, &cy, px, py, R01_PLAY_PLAYER_W, R01_PLAY_PLAYER_H, R01_SCREEN_PX_W, R01_SCREEN_PX_H,
                           (int)ctx->cam_deadzone_x, (int)ctx->cam_deadzone_y, (int)ctx->cam_axis_lock);
    ctx->cam_x = (uint16_t)cx;
    ctx->cam_y = (uint16_t)cy;
#ifdef R01_HOST_TEST
    r01_sys_publish(ctx);
#endif
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
    r01_game_anim_tick(ctx, s_draw_air, s_draw_crouch, s_draw_adx, s_draw_ady);
#ifndef R01_HOST_TEST
    r01_game_draw_sprites(ctx, s_draw_air, s_draw_crouch, s_draw_adx, s_draw_ady);
#endif
}
