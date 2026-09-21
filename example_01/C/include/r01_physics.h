#ifndef R01_PHYSICS_H
#define R01_PHYSICS_H

typedef struct R01GameCtx R01GameCtx;
#define R01_GAME_MODE_TOPDOWN 0
#define R01_GAME_MODE_PLATFORMER 1
#define R01_PLAT_GRAVITY_SCALE 16
#define R01_PLAT_GRAVITY_DEFAULT 4
#define R01_PLAT_GRAVITY_MAX 255
#define R01_PLAT_JUMP_DEFAULT 4
#define R01_PLAT_METER_DEFAULT 16
void r01_game_set_mode(R01GameCtx *ctx, int mode);
void r01_solid_pattern_add(R01GameCtx *ctx, int bank, int tile);
void r01_platformer_set_gravity(R01GameCtx *ctx, int units);
void r01_platformer_set_jump(R01GameCtx *ctx, int impulse);
void r01_platformer_set_meter(R01GameCtx *ctx, int px_per_meter);

#endif
