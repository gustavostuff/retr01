#ifndef retr01_STUDIO_PLAYER_ANIM_H
#define retr01_STUDIO_PLAYER_ANIM_H

#include "retr01_studio/types.h"

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

#define R01_PLAYER_ANIM_DELAY_DEFAULT 6

typedef struct R01GameCtx R01GameCtx;

void r01_player_anim_init(R01GameCtx *ctx);
void r01_player_anim_set_idle_state(R01GameCtx *ctx, int entity_state_idx);
void r01_player_anim_set_walk_state(R01GameCtx *ctx, int dir8, int entity_state_idx);
void r01_player_anim_set_walk_all(R01GameCtx *ctx, int entity_state_idx);
void r01_player_default_face_set(R01GameCtx *ctx, int face);
void r01_entity_state_frame_delay_set(R01GameCtx *ctx, int entity_state_idx, int ticks);
void r01_player_anim_update(R01GameCtx *ctx, int dx, int dy);
void r01_player_anim_tick(R01GameCtx *ctx, const R01World *w, int player_type);

int r01_player_anim_entity_state(const R01GameCtx *ctx);
int r01_player_anim_frame(const R01GameCtx *ctx);
int r01_player_anim_flip_h(const R01GameCtx *ctx);
int r01_player_anim_dir(const R01GameCtx *ctx);
int r01_player_anim_moving(const R01GameCtx *ctx);

#endif
