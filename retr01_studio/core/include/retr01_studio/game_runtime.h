#ifndef retr01_STUDIO_GAME_RUNTIME_H
#define retr01_STUDIO_GAME_RUNTIME_H

#include "retr01_studio/types.h"

#define R01_BTN_X 0
#define R01_BTN_Y 1

#define R01_MAX_PROJECTILES 8
#define R01_PROJECTILE_TTL 180
#define R01_FADE_SPEED 8
#define R01_FADE_MAX 255

#define R01_CAM_AXIS_BOTH 0
#define R01_CAM_AXIS_H 1
#define R01_CAM_AXIS_V 2

#define R01_FADE_BLACK 0
#define R01_FADE_WHITE 1

typedef struct R01GameCtx R01GameCtx;

typedef struct R01Projectile {
    int active;
    int x; /* 16.16 fixed world px */
    int y;
    int vx;
    int vy;
    int ttl;
    uint8_t tile;
    uint8_t pal;
} R01Projectile;

struct R01GameCtx {
    int player_x;
    int player_y;
    int cam_x;
    int cam_y;
    uint8_t pad;
    uint8_t pad_prev;
    int cam_deadzone_x;
    int cam_deadzone_y;
    int cam_axis_lock;
    int fade_level;
    int fade_target;
    int fade_color;
    int fade_pending_entrance;
    R01Projectile projectiles[R01_MAX_PROJECTILES];
};

typedef void (*R01EventFn)(R01GameCtx *ctx);

void r01_game_ctx_init(R01GameCtx *ctx);
void r01_game_camera_update(R01GameCtx *ctx);
void r01_game_fade_start(R01GameCtx *ctx, int to_black_or_white, int target_level);
int r01_game_fade_active(const R01GameCtx *ctx);
int r01_game_fade_tick(R01GameCtx *ctx);
void r01_game_warp_to_tile(R01GameCtx *ctx, int screen_col, int screen_row, int tile_col,
                           int tile_row);
void r01_game_fade_warp_step(R01GameCtx *ctx, const R01World *w);
void r01_game_warp_check(R01GameCtx *ctx, const R01World *w);
int r01_game_warp_by_id(R01GameCtx *ctx, const R01World *w, const char *warp_id);
int r01_projectile_fire(R01GameCtx *ctx, int dx, int dy, int speed);
void r01_projectile_tick(R01GameCtx *ctx, const R01World *w);
int r01_projectile_count_active(const R01GameCtx *ctx);

uint8_t r01_pad_pressed(const R01GameCtx *ctx, uint8_t btn);
uint8_t r01_pad_just_pressed(R01GameCtx *ctx, uint8_t btn);
void r01_player_warp(R01GameCtx *ctx, int col, int row);
void r01_player_set_type(uint8_t type_id);
void r01_camera_set_deadzone(R01GameCtx *ctx, int dx, int dy);
void r01_camera_set_axis_lock(R01GameCtx *ctx, int mode);
int r01_event_on_button(uint8_t btn, R01EventFn fn);
void r01_runtime_dispatch_buttons(R01GameCtx *ctx);

#endif
