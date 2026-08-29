#ifndef R01_PLAY_ANIM_H
#define R01_PLAY_ANIM_H

#include <stdint.h>

#define R01_PLAY_ANIM_STATES_MAX 4
#define R01_PLAY_ANIM_DELAY_DEFAULT 6

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

typedef struct R01PlayAnimCtx {
    int player_anim_state;
    int player_anim_frame;
    int player_anim_ctr;
    int player_anim_flip_h;
    int player_anim_dir;
    int player_anim_moving;
    int player_default_face;
    int player_idle_state;
    int player_walk_state[8];
    int player_state_delay[R01_PLAY_ANIM_STATES_MAX];
} R01PlayAnimCtx;

void r01_play_anim_init(R01PlayAnimCtx *ctx);
void r01_play_anim_set_idle_state(R01PlayAnimCtx *ctx, int entity_state_idx);
void r01_play_anim_set_walk_state(R01PlayAnimCtx *ctx, int dir8, int entity_state_idx);
void r01_play_anim_set_walk_all(R01PlayAnimCtx *ctx, int entity_state_idx);
void r01_play_default_face_set(R01PlayAnimCtx *ctx, int face);
void r01_play_state_frame_delay_set(R01PlayAnimCtx *ctx, int entity_state_idx, int ticks);
void r01_play_anim_update(R01PlayAnimCtx *ctx, int dx, int dy);

int r01_play_anim_entity_state(const R01PlayAnimCtx *ctx);
int r01_play_anim_frame(const R01PlayAnimCtx *ctx);
int r01_play_anim_flip_h(const R01PlayAnimCtx *ctx);
int r01_play_anim_dir(const R01PlayAnimCtx *ctx);
int r01_play_anim_moving(const R01PlayAnimCtx *ctx);

#endif
