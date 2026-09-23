#ifndef R01_PLAY_ANIM_H
#define R01_PLAY_ANIM_H

#include <stdint.h>

#define R01_PLAY_ANIM_STATES_MAX 4
#define R01_PLAY_ANIM_DELAY_DEFAULT 6
#define R01_PLAY_ANIM_UNMAPPED 0xFFu

#define R01_PLAYER_DIR_RIGHT 0
#define R01_PLAYER_DIR_DOWN_RIGHT 1
#define R01_PLAYER_DIR_DOWN 2
#define R01_PLAYER_DIR_DOWN_LEFT 3
#define R01_PLAYER_DIR_LEFT 4
#define R01_PLAYER_DIR_UP_LEFT 5
#define R01_PLAYER_DIR_UP 6
#define R01_PLAYER_DIR_UP_RIGHT 7

#define R01_PLAYER_FACE_RIGHT 0
#define R01_PLAYER_FACE_DOWN 1
#define R01_PLAYER_FACE_LEFT 2
#define R01_PLAYER_FACE_UP 3

#define R01_PLAY_ANIM_MAPPED(v) ((uint8_t)(v) < (uint8_t)R01_PLAY_ANIM_STATES_MAX)

typedef struct R01PlayAnimCtx {
    uint8_t player_anim_state;
    uint8_t player_anim_frame;
    uint8_t player_anim_ctr;
    uint8_t player_anim_flip_h;
    uint8_t player_anim_dir;
    uint8_t player_anim_moving;
    uint8_t player_default_face;
    uint8_t player_idle_state; /* 0xFF = unmapped: freeze state 0 frame 0 */
    uint8_t player_walk_state[8]; /* 0xFF = unmapped */
    uint8_t player_state_delay[R01_PLAY_ANIM_STATES_MAX];
    uint8_t player_crouch_state; /* 0xFF = none */
    uint8_t player_crouching;
    uint8_t player_jump_state; /* 0xFF = none */
    uint8_t player_airborne;
    uint8_t player_anim_delay_override; /* 0 = authored delay, else live ticks this frame */
    /* If set (default), release (stop moving) snaps that state back to idle.
     * Clear with r01_play_anim_set_release_to_idle(ctx, state, 0) to hold pose. */
    uint8_t player_release_to_idle[R01_PLAY_ANIM_STATES_MAX];
} R01PlayAnimCtx;

void r01_play_anim_init(R01PlayAnimCtx *ctx);
void r01_play_anim_set_idle_state(R01PlayAnimCtx *ctx, int entity_state_idx);
void r01_play_anim_set_walk_state(R01PlayAnimCtx *ctx, int dir8, int entity_state_idx);
void r01_play_anim_set_walk_all(R01PlayAnimCtx *ctx, int entity_state_idx);
void r01_play_anim_set_crouch_state(R01PlayAnimCtx *ctx, int entity_state_idx);
void r01_play_anim_set_jump_state(R01PlayAnimCtx *ctx, int entity_state_idx);
void r01_play_anim_set_crouching(R01PlayAnimCtx *ctx, int on);
void r01_play_anim_set_airborne(R01PlayAnimCtx *ctx, int on);
void r01_play_anim_set_frame_delay(R01PlayAnimCtx *ctx, int ticks);
void r01_play_anim_set_release_to_idle(R01PlayAnimCtx *ctx, int entity_state_idx, int enable);
void r01_play_default_face_set(R01PlayAnimCtx *ctx, int face);
void r01_play_state_frame_delay_set(R01PlayAnimCtx *ctx, int entity_state_idx, int ticks);
void r01_play_anim_update(R01PlayAnimCtx *ctx, int8_t dx, int8_t dy);
/* Cart / Play frame delay. Live override from r01_play_anim_set_frame_delay wins. Min 1. */
uint8_t r01_play_anim_frame_delay(const R01PlayAnimCtx *ctx, uint8_t delay);

uint8_t r01_play_anim_entity_state(const R01PlayAnimCtx *ctx);
uint8_t r01_play_anim_frame(const R01PlayAnimCtx *ctx);
uint8_t r01_play_anim_flip_h(const R01PlayAnimCtx *ctx);
uint8_t r01_play_anim_dir(const R01PlayAnimCtx *ctx);
uint8_t r01_play_anim_moving(const R01PlayAnimCtx *ctx);

/* Copy shared anim fields to/from R01GameCtx (same member names).
 * PULL inits dst first so player_release_to_idle defaults to 1. */
#define R01_PLAY_ANIM_PULL(dst, src)                                                               \
    do {                                                                                           \
        uint8_t _i;                                                                                \
        r01_play_anim_init(dst);                                                                   \
        (dst)->player_anim_state = (uint8_t)(src)->player_anim_state;                               \
        (dst)->player_anim_frame = (uint8_t)(src)->player_anim_frame;                               \
        (dst)->player_anim_ctr = (uint8_t)(src)->player_anim_ctr;                                   \
        (dst)->player_anim_flip_h = (uint8_t)(src)->player_anim_flip_h;                             \
        (dst)->player_anim_dir = (uint8_t)(src)->player_anim_dir;                                   \
        (dst)->player_anim_moving = (uint8_t)(src)->player_anim_moving;                             \
        (dst)->player_default_face = (uint8_t)(src)->player_default_face;                           \
        (dst)->player_idle_state = (uint8_t)(src)->player_idle_state;                               \
        for (_i = 0; _i < 8u; _i++) {                                                              \
            (dst)->player_walk_state[_i] = (uint8_t)(src)->player_walk_state[_i];                   \
        }                                                                                          \
        for (_i = 0; _i < (uint8_t)R01_PLAY_ANIM_STATES_MAX; _i++) {                               \
            (dst)->player_state_delay[_i] = (uint8_t)(src)->player_state_delay[_i];                 \
        }                                                                                          \
        (dst)->player_crouch_state = (uint8_t)(src)->player_crouch_state;                           \
        (dst)->player_crouching = (uint8_t)(src)->player_crouching;                                 \
        (dst)->player_jump_state = (uint8_t)(src)->player_jump_state;                               \
        (dst)->player_airborne = (uint8_t)(src)->player_airborne;                                   \
        (dst)->player_anim_delay_override = (uint8_t)(src)->player_anim_delay_override;             \
    } while (0)

#define R01_PLAY_ANIM_PUSH(src, dst)                                                               \
    do {                                                                                           \
        uint8_t _i;                                                                                \
        (dst)->player_anim_state = (src)->player_anim_state;                                       \
        (dst)->player_anim_frame = (src)->player_anim_frame;                                       \
        (dst)->player_anim_ctr = (src)->player_anim_ctr;                                           \
        (dst)->player_anim_flip_h = (src)->player_anim_flip_h;                                     \
        (dst)->player_anim_dir = (src)->player_anim_dir;                                           \
        (dst)->player_anim_moving = (src)->player_anim_moving;                                     \
        (dst)->player_default_face = (src)->player_default_face;                                   \
        (dst)->player_idle_state = (src)->player_idle_state;                                       \
        for (_i = 0; _i < 8u; _i++) {                                                              \
            (dst)->player_walk_state[_i] = (src)->player_walk_state[_i];                           \
        }                                                                                          \
        for (_i = 0; _i < (uint8_t)R01_PLAY_ANIM_STATES_MAX; _i++) {                               \
            (dst)->player_state_delay[_i] = (src)->player_state_delay[_i];                         \
        }                                                                                          \
        (dst)->player_crouch_state = (src)->player_crouch_state;                                   \
        (dst)->player_crouching = (src)->player_crouching;                                         \
        (dst)->player_jump_state = (src)->player_jump_state;                                       \
        (dst)->player_airborne = (src)->player_airborne;                                           \
        (dst)->player_anim_delay_override = (src)->player_anim_delay_override;                     \
    } while (0)

#endif
