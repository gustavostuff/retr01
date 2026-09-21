#include "r01_play_physics.h"

static int clamp_gravity(int v) {
    if (v < 1) {
        return R01_PLAT_GRAVITY_DEFAULT;
    }
    if (v > R01_PLAT_GRAVITY_MAX) {
        return R01_PLAT_GRAVITY_MAX;
    }
    return v;
}

static int clamp_jump(int v) {
    if (v < 1) {
        return R01_PLAT_JUMP_DEFAULT;
    }
    if (v > 32) {
        return 32;
    }
    return v;
}

static int clamp_meter(int v) {
    if (v < 1) {
        return R01_PLAT_METER_DEFAULT;
    }
    if (v > 64) {
        return 64;
    }
    return v;
}

/* Author units are pixels at meter=16. Result is 8.8. */
static int units_to_fp(int units, int meter) {
    return (units * meter * R01_PHYS_ONE) / R01_PLAT_METER_DEFAULT;
}

static int gravity_to_fp(int units, int meter) {
    return units_to_fp(units, meter) / R01_PLAT_GRAVITY_SCALE;
}

static int fp_to_px(int fp) {
    if (fp >= 0) {
        return fp >> R01_PHYS_SHIFT;
    }
    return -((-fp) >> R01_PHYS_SHIFT);
}

static int step_axis(int *pos, int delta, int other, int horiz, int (*move_ok)(void *ctx, int x, int y),
                     void *ctx) {
    int nx;
    int ny;
    if (!pos || !move_ok || delta == 0) {
        return 0;
    }
    if (horiz) {
        nx = *pos + delta;
        ny = other;
        if (!move_ok(ctx, nx, ny)) {
            return 0;
        }
        *pos = nx;
        return 1;
    }
    nx = other;
    ny = *pos + delta;
    if (!move_ok(ctx, nx, ny)) {
        return 0;
    }
    *pos = ny;
    return 1;
}

/* Add delta_fp into *frac, step 1 px at a time. Returns 0 if a step hit a solid. */
static int integrate_axis(int *pos, int *frac, int delta_fp, int other, int horiz,
                          int (*move_ok)(void *ctx, int x, int y), void *ctx) {
    int steps;
    int dir;
    int i;
    if (!pos || !frac || !move_ok) {
        return 0;
    }
    *frac += delta_fp;
    steps = fp_to_px(*frac);
    *frac -= steps << R01_PHYS_SHIFT;
    dir = 1;
    if (steps < 0) {
        dir = -1;
        steps = -steps;
    }
    for (i = 0; i < steps; i++) {
        if (!step_axis(pos, dir, other, horiz, move_ok, ctx)) {
            *frac = 0;
            return 0;
        }
    }
    return 1;
}

void r01_play_physics_init(R01PlayPhysics *ph) {
    if (!ph) {
        return;
    }
    ph->mode = R01_GAME_MODE_TOPDOWN;
    ph->gravity = R01_PLAT_GRAVITY_DEFAULT;
    ph->jump = R01_PLAT_JUMP_DEFAULT;
    ph->fall_max = R01_PLAT_FALL_MAX_DEFAULT;
    ph->meter = R01_PLAT_METER_DEFAULT;
    ph->vel_y = 0;
    ph->frac_x = 0;
    ph->frac_y = 0;
    ph->grounded = 0;
    ph->jump_held = 0;
    ph->run_mul = 1;
}

void r01_play_physics_set_mode(R01PlayPhysics *ph, int mode) {
    if (!ph) {
        return;
    }
    if (mode != R01_GAME_MODE_PLATFORMER) {
        mode = R01_GAME_MODE_TOPDOWN;
    }
    ph->mode = mode;
    if (mode == R01_GAME_MODE_TOPDOWN) {
        r01_play_physics_reset_air(ph);
    }
}

void r01_play_physics_set_gravity(R01PlayPhysics *ph, int units) {
    if (!ph) {
        return;
    }
    ph->gravity = clamp_gravity(units);
}

void r01_play_physics_set_jump(R01PlayPhysics *ph, int impulse) {
    if (!ph) {
        return;
    }
    ph->jump = clamp_jump(impulse);
}

void r01_play_physics_set_meter(R01PlayPhysics *ph, int px_per_meter) {
    if (!ph) {
        return;
    }
    ph->meter = clamp_meter(px_per_meter);
}

void r01_play_physics_set_run_mul(R01PlayPhysics *ph, int mul) {
    if (!ph) {
        return;
    }
    if (mul < 1) {
        mul = 1;
    }
    if (mul > 2) {
        mul = 2;
    }
    ph->run_mul = mul;
}

void r01_play_physics_reset_air(R01PlayPhysics *ph) {
    if (!ph) {
        return;
    }
    ph->vel_y = 0;
    ph->frac_x = 0;
    ph->frac_y = 0;
    ph->grounded = 0;
    ph->jump_held = 0;
}

void r01_play_physics_tick(R01PlayPhysics *ph, int *px, int *py, int in_dx, int in_dy, int jump_down,
                           int (*move_ok)(void *ctx, int x, int y), void *ctx, int *out_anim_dx,
                           int *out_anim_dy) {
    int x;
    int y;
    int anim_dx = 0;
    int anim_dy = 0;
    int jump_pressed;
    int walk_fp;
    int gravity_fp;
    int jump_fp;
    int fall_max_fp;

    if (!ph || !px || !py || !move_ok) {
        return;
    }
    x = *px;
    y = *py;
    if (in_dx < -1) {
        in_dx = -1;
    }
    if (in_dx > 1) {
        in_dx = 1;
    }
    if (in_dy < -1) {
        in_dy = -1;
    }
    if (in_dy > 1) {
        in_dy = 1;
    }
    jump_down = jump_down ? 1 : 0;
    jump_pressed = jump_down && !ph->jump_held;
    ph->jump_held = jump_down;
    {
        int mul = ph->run_mul;
        if (mul < 1) {
            mul = 1;
        }
        if (mul > 2) {
            mul = 2;
        }
        walk_fp = units_to_fp(1, ph->meter) * mul;
    }

    if (ph->mode != R01_GAME_MODE_PLATFORMER) {
        (void)integrate_axis(&x, &ph->frac_x, walk_fp * in_dx, y, 1, move_ok, ctx);
        (void)integrate_axis(&y, &ph->frac_y, walk_fp * in_dy, x, 0, move_ok, ctx);
        anim_dx = in_dx;
        anim_dy = in_dy;
    } else {
        int g;
        gravity_fp = gravity_to_fp(ph->gravity, ph->meter);
        jump_fp = units_to_fp(ph->jump, ph->meter);
        fall_max_fp = units_to_fp(ph->fall_max, ph->meter);
        if (jump_pressed && ph->grounded) {
            ph->vel_y = -jump_fp;
            ph->grounded = 0;
            ph->frac_y = 0;
        } else {
            g = gravity_fp;
            if (ph->vel_y < 0 && !jump_down) {
                g = gravity_fp * R01_PLAT_JUMP_RELEASE_MUL;
            }
            ph->vel_y += g;
            if (ph->vel_y > fall_max_fp) {
                ph->vel_y = fall_max_fp;
            }
        }
        (void)integrate_axis(&x, &ph->frac_x, walk_fp * in_dx, y, 1, move_ok, ctx);
        if (!integrate_axis(&y, &ph->frac_y, ph->vel_y, x, 0, move_ok, ctx)) {
            ph->vel_y = 0;
        }
        ph->grounded = !move_ok(ctx, x, y + 1);
        if (ph->grounded && ph->vel_y > 0) {
            ph->vel_y = 0;
            ph->frac_y = 0;
        }
        anim_dx = in_dx;
        anim_dy = 0;
    }

    *px = x;
    *py = y;
    if (out_anim_dx) {
        *out_anim_dx = anim_dx;
    }
    if (out_anim_dy) {
        *out_anim_dy = anim_dy;
    }
}
