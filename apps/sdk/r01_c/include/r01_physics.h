#ifndef R01_PHYSICS_H
#define R01_PHYSICS_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

#define R01_GAME_MODE_TOPDOWN 0
#define R01_GAME_MODE_PLATFORMER 1
#define R01_PLAT_GRAVITY_DEFAULT 4
#define R01_PLAT_JUMP_DEFAULT 4
#define R01_PLAT_METER_DEFAULT 16

void r01_game_set_mode(R01GameCtx *ctx, uint8_t mode);
void r01_solid_pattern_add(R01GameCtx *ctx, uint8_t bank, uint8_t tile);
void r01_platformer_set_gravity(R01GameCtx *ctx, uint8_t units);
void r01_platformer_set_jump(R01GameCtx *ctx, uint8_t impulse);
void r01_platformer_set_meter(R01GameCtx *ctx, uint8_t px_per_meter);

#endif
