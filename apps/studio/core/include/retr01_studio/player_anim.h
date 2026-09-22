#ifndef retr01_STUDIO_PLAYER_ANIM_H
#define retr01_STUDIO_PLAYER_ANIM_H

#include "retr01_studio/types.h"
#include "r01_play_anim.h"

typedef struct R01GameCtx R01GameCtx;

void r01_player_anim_init(R01GameCtx *ctx);
void r01_player_anim_set_idle_state(R01GameCtx *ctx, int entity_state_idx);
void r01_player_anim_set_walk_state(R01GameCtx *ctx, int dir8, int entity_state_idx);
void r01_player_anim_set_walk_all(R01GameCtx *ctx, int entity_state_idx);
void r01_player_anim_set_crouch_state(R01GameCtx *ctx, int entity_state_idx);
void r01_player_anim_set_jump_state(R01GameCtx *ctx, int entity_state_idx);
void r01_player_anim_set_frame_delay(R01GameCtx *ctx, int ticks);
int r01_player_anim_frame_delay(const R01GameCtx *ctx);
void r01_player_default_face_set(R01GameCtx *ctx, int face);
void r01_entity_state_frame_delay_set(R01GameCtx *ctx, int entity_state_idx, int ticks);
void r01_player_anim_update(R01GameCtx *ctx, int dx, int dy);
void r01_player_anim_tick(R01GameCtx *ctx, const R01Project *p, int player_type);

int r01_player_anim_entity_state(const R01GameCtx *ctx);
int r01_player_anim_frame(const R01GameCtx *ctx);
int r01_player_anim_flip_h(const R01GameCtx *ctx);
int r01_player_anim_dir(const R01GameCtx *ctx);
int r01_player_anim_moving(const R01GameCtx *ctx);

#endif
