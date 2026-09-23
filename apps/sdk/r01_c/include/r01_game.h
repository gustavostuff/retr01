#ifndef R01_GAME_H
#define R01_GAME_H

#include <stdint.h>

typedef struct R01GameCtx {
    uint16_t player_x;
    uint16_t player_y;
    uint16_t cam_x;
    uint16_t cam_y;
    uint8_t pad;
    uint8_t pad_prev;
    uint8_t cam_deadzone_x;
    uint8_t cam_deadzone_y;
    uint8_t cam_axis_lock;
    uint8_t bg0_wrap_x;
    uint8_t bg0_wrap_y;
    uint8_t bg0_clip_bg1;
    uint8_t game_mode;
    uint8_t plat_gravity;
    uint8_t plat_jump;
    uint8_t plat_meter;
    uint8_t player_move_mul;
    uint8_t player_anim_delay_override;
    uint8_t player_idle_state;
    uint8_t player_walk_state[8];
    uint8_t player_crouch_state;
    uint8_t player_jump_state;
    uint8_t player_anim_moving;
    uint8_t bgm_track;
    uint8_t solid_pat_count;
    uint8_t solid_pat_bank[64];
    uint8_t solid_pat_tile[64];
} R01GameCtx;

#ifndef R01_NOINLINE
#define R01_NOINLINE __attribute__((noinline))
#endif

void r01_game_ctx_init(R01GameCtx *ctx);
void R01_NOINLINE r01_game_on_init(R01GameCtx *ctx);
void R01_NOINLINE r01_game_on_tick(R01GameCtx *ctx);
/* NMI after the tracker tick. Keep the body short. */
void R01_NOINLINE r01_game_on_vblank(R01GameCtx *ctx);
void r01_game_play_tick(R01GameCtx *ctx);
void r01_game_play_reset(void);
void r01_game_spawn(R01GameCtx *ctx);
void r01_game_camera_snap(R01GameCtx *ctx);
void r01_scroll_publish(const R01GameCtx *ctx);
void r01_sys_publish(const R01GameCtx *ctx);
void r01_world_enter(uint8_t world_id);
void r01_game_draw_player(const R01GameCtx *ctx);
void r01_game_anim_tick(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady);
void r01_game_draw_sprites(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady);
void r01_bg0_publish(const R01GameCtx *ctx);

#endif
