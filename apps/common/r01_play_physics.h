#ifndef R01_PLAY_PHYSICS_H
#define R01_PLAY_PHYSICS_H

/* Host Play movement. Top-down is axis-separated. Platformer adds gravity + jump.
 * Tune values are pixels at meter=16. Smaller meter slows pixel motion. */

#define R01_GAME_MODE_TOPDOWN 0
#define R01_GAME_MODE_PLATFORMER 1

#define R01_PLAT_GRAVITY_DEFAULT 1
#define R01_PLAT_JUMP_DEFAULT 32
#define R01_PLAT_FALL_MAX_DEFAULT 4
#define R01_PLAT_METER_DEFAULT 32
#define R01_PLAT_JUMP_RELEASE_MUL 3
#define R01_PHYS_SHIFT 8
#define R01_PHYS_ONE (1 << R01_PHYS_SHIFT)

typedef struct R01PlayPhysics {
    int mode;
    int gravity; /* author units (px at meter 16) */
    int jump;
    int fall_max;
    int meter; /* px per meter, default 16 */
    int vel_y; /* 8.8 px / frame (down +) */
    int frac_x; /* 8.8 remainder, abs < 1 px */
    int frac_y;
    int grounded;
    int jump_held;
} R01PlayPhysics;

void r01_play_physics_init(R01PlayPhysics *ph);
void r01_play_physics_set_mode(R01PlayPhysics *ph, int mode);
void r01_play_physics_set_gravity(R01PlayPhysics *ph, int units);
void r01_play_physics_set_jump(R01PlayPhysics *ph, int impulse);
void r01_play_physics_set_meter(R01PlayPhysics *ph, int px_per_meter);
void r01_play_physics_reset_air(R01PlayPhysics *ph);

/*
 * One tick. move_ok(ctx, x, y) is 1 when the player origin may sit at (x,y).
 * in_dx / in_dy are pad intent (-1, 0, 1). jump_down is 1 while jump is held.
 * Platformer uses jump edge while grounded (pad Y). Top-down uses in_dy as walk.
 * Release while rising uses 3x gravity (short hop). out_anim_* is pad delta.
 */
void r01_play_physics_tick(R01PlayPhysics *ph, int *px, int *py, int in_dx, int in_dy, int jump_down,
                           int (*move_ok)(void *ctx, int x, int y), void *ctx, int *out_anim_dx,
                           int *out_anim_dy);

#endif
