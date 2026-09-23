#ifndef R01_PLAY_PHYSICS_H
#define R01_PLAY_PHYSICS_H

#include <stdint.h>

/* Play movement. Top-down is axis-separated. Platformer adds gravity + jump.
 * Tune values are pixels at meter=16, except gravity which is 1/16 px per frame^2.
 * Walk is 1 px/frame at meter 16 (2 px/frame when run_mul is 2) and does not use gravity.
 * Smaller meter slows all of it. */

#ifndef R01_GAME_MODE_TOPDOWN
#define R01_GAME_MODE_TOPDOWN 0
#define R01_GAME_MODE_PLATFORMER 1
#endif

#ifndef R01_PLAT_GRAVITY_SCALE
#define R01_PLAT_GRAVITY_SCALE 16
#define R01_PLAT_GRAVITY_DEFAULT 4 /* 4/16 = 0.25 px/frame^2 at meter 16 */
#define R01_PLAT_GRAVITY_MAX 255
#define R01_PLAT_JUMP_DEFAULT 4 /* takeoff px/frame at meter 16. Peak ~34 px in ~16 frames */
#define R01_PLAT_FALL_MAX_DEFAULT 4
#define R01_PLAT_METER_DEFAULT 16
#endif
#define R01_PLAT_JUMP_RELEASE_MUL 3
#define R01_PHYS_SHIFT 8
#define R01_PHYS_ONE (1 << R01_PHYS_SHIFT)

typedef struct R01PlayPhysics {
    uint8_t mode;
    uint8_t gravity; /* author units: 1/16 px per frame^2 at meter 16 */
    uint8_t jump;
    uint8_t fall_max;
    uint8_t meter; /* px per meter, default 16 */
    int16_t vel_y; /* 8.8 px / frame (down +) */
    int16_t frac_x; /* 8.8 remainder, abs < 1 px */
    int16_t frac_y;
    uint8_t grounded;
    uint8_t jump_held;
    uint8_t run_mul; /* 1 = walk, 2 = hold-X run */
} R01PlayPhysics;

void r01_play_physics_init(R01PlayPhysics *ph);
void r01_play_physics_set_mode(R01PlayPhysics *ph, uint8_t mode);
void r01_play_physics_set_gravity(R01PlayPhysics *ph, uint8_t units);
void r01_play_physics_set_jump(R01PlayPhysics *ph, uint8_t impulse);
void r01_play_physics_set_meter(R01PlayPhysics *ph, uint8_t px_per_meter);
void r01_play_physics_set_run_mul(R01PlayPhysics *ph, uint8_t mul);
void r01_play_physics_reset_air(R01PlayPhysics *ph);

/*
 * One tick. move_ok(ctx, x, y) is 1 when the player origin may sit at (x,y).
 * in_dx / in_dy are pad intent (-1, 0, 1). jump_down is 1 while jump is held.
 * Platformer uses jump edge while grounded (pad Y). Top-down uses in_dy as walk.
 * Release while rising uses 3x gravity (short hop). out_anim_* is pad delta.
 */
void r01_play_physics_tick(R01PlayPhysics *ph, uint16_t *px, uint16_t *py, int8_t in_dx, int8_t in_dy,
                           uint8_t jump_down, int (*move_ok)(void *ctx, uint16_t x, uint16_t y), void *ctx,
                           int8_t *out_anim_dx, int8_t *out_anim_dy);

#endif
