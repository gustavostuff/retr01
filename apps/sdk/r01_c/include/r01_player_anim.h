#ifndef R01_PLAYER_ANIM_H
#define R01_PLAYER_ANIM_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

void r01_player_anim_set_idle_state(R01GameCtx *ctx, uint8_t entity_state_idx);
void r01_player_anim_set_walk_all(R01GameCtx *ctx, uint8_t entity_state_idx);
void r01_player_anim_set_crouch_state(R01GameCtx *ctx, uint8_t entity_state_idx);
void r01_player_anim_set_jump_state(R01GameCtx *ctx, uint8_t entity_state_idx);
void r01_player_anim_set_frame_delay(R01GameCtx *ctx, uint8_t ticks);

#endif
