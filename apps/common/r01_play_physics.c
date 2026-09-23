#include "r01_play_physics.h"

static uint8_t clamp_gravity(uint8_t v) {
    if (v < 1u) {
        return (uint8_t)R01_PLAT_GRAVITY_DEFAULT;
    }
    return v;
}

static uint8_t clamp_jump(uint8_t v) {
    if (v < 1u) {
        return (uint8_t)R01_PLAT_JUMP_DEFAULT;
    }
    if (v > 32u) {
        return 32u;
    }
    return v;
}

static uint8_t clamp_meter(uint8_t v) {
    if (v < 1u) {
        return (uint8_t)R01_PLAT_METER_DEFAULT;
    }
    if (v > 64u) {
        return 64u;
    }
    return v;
}

/* Author units are pixels at meter=16. Result is 8.8. */
static int16_t units_to_fp(uint8_t units, uint8_t meter) {
    int v = ((int)units * (int)meter * R01_PHYS_ONE) / R01_PLAT_METER_DEFAULT;
    if (v > 32767) {
        v = 32767;
    }
    return (int16_t)v;
}

static int16_t gravity_to_fp(uint8_t units, uint8_t meter) {
    return (int16_t)(units_to_fp(units, meter) / R01_PLAT_GRAVITY_SCALE);
}

static int16_t fp_to_px(int16_t fp) {
    if (fp >= 0) {
        return (int16_t)(fp >> R01_PHYS_SHIFT);
    }
    return (int16_t)(-((-fp) >> R01_PHYS_SHIFT));
}

static uint8_t step_axis(uint16_t *pos, int8_t delta, uint16_t other, uint8_t horiz,
                         int (*move_ok)(void *ctx, uint16_t x, uint16_t y), void *ctx) {
    int16_t n;
    uint16_t nx;
    uint16_t ny;
    if (!pos || !move_ok || delta == 0) {
        return 0;
    }
    n = (int16_t)((int16_t)*pos + (int16_t)delta);
    if (n < 0) {
        return 0;
    }
    if (horiz) {
        nx = (uint16_t)n;
        ny = other;
        if (!move_ok(ctx, nx, ny)) {
            return 0;
        }
        *pos = nx;
        return 1;
    }
    nx = other;
    ny = (uint16_t)n;
    if (!move_ok(ctx, nx, ny)) {
        return 0;
    }
    *pos = ny;
    return 1;
}

/* Add delta_fp into *frac, step 1 px at a time. Returns 0 if a step hit a solid. */
static uint8_t integrate_axis(uint16_t *pos, int16_t *frac, int16_t delta_fp, uint16_t other, uint8_t horiz,
                              int (*move_ok)(void *ctx, uint16_t x, uint16_t y), void *ctx) {
    int16_t steps;
    int8_t dir;
    int16_t i;
    if (!pos || !frac || !move_ok) {
        return 0;
    }
    *frac = (int16_t)(*frac + delta_fp);
    steps = fp_to_px(*frac);
    *frac = (int16_t)(*frac - (int16_t)(steps << R01_PHYS_SHIFT));
    dir = 1;
    if (steps < 0) {
        dir = -1;
        steps = (int16_t)(-steps);
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
    ph->mode = (uint8_t)R01_GAME_MODE_TOPDOWN;
    ph->gravity = (uint8_t)R01_PLAT_GRAVITY_DEFAULT;
    ph->jump = (uint8_t)R01_PLAT_JUMP_DEFAULT;
    ph->fall_max = (uint8_t)R01_PLAT_FALL_MAX_DEFAULT;
    ph->meter = (uint8_t)R01_PLAT_METER_DEFAULT;
    ph->vel_y = 0;
    ph->frac_x = 0;
    ph->frac_y = 0;
    ph->grounded = 0;
    ph->jump_held = 0;
    ph->run_mul = 1;
}

void r01_play_physics_set_mode(R01PlayPhysics *ph, uint8_t mode) {
    if (!ph) {
        return;
    }
    if (mode != (uint8_t)R01_GAME_MODE_PLATFORMER) {
        mode = (uint8_t)R01_GAME_MODE_TOPDOWN;
    }
    ph->mode = mode;
    if (mode == (uint8_t)R01_GAME_MODE_TOPDOWN) {
        r01_play_physics_reset_air(ph);
    }
}

void r01_play_physics_set_gravity(R01PlayPhysics *ph, uint8_t units) {
    if (!ph) {
        return;
    }
    ph->gravity = clamp_gravity(units);
}

void r01_play_physics_set_jump(R01PlayPhysics *ph, uint8_t impulse) {
    if (!ph) {
        return;
    }
    ph->jump = clamp_jump(impulse);
}

void r01_play_physics_set_meter(R01PlayPhysics *ph, uint8_t px_per_meter) {
    if (!ph) {
        return;
    }
    ph->meter = clamp_meter(px_per_meter);
}

void r01_play_physics_set_run_mul(R01PlayPhysics *ph, uint8_t mul) {
    if (!ph) {
        return;
    }
    if (mul < 1u) {
        mul = 1u;
    }
    if (mul > 2u) {
        mul = 2u;
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

void r01_play_physics_tick(R01PlayPhysics *ph, uint16_t *px, uint16_t *py, int8_t in_dx, int8_t in_dy,
                           uint8_t jump_down, int (*move_ok)(void *ctx, uint16_t x, uint16_t y), void *ctx,
                           int8_t *out_anim_dx, int8_t *out_anim_dy) {
    uint16_t x;
    uint16_t y;
    int8_t anim_dx = 0;
    int8_t anim_dy = 0;
    uint8_t jump_pressed;
    int16_t walk_fp;
    int16_t gravity_fp;
    int16_t jump_fp;
    int16_t fall_max_fp;

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
    jump_down = jump_down ? 1u : 0u;
    jump_pressed = (uint8_t)(jump_down && !ph->jump_held);
    ph->jump_held = jump_down;
    {
        uint8_t mul = ph->run_mul;
        if (mul < 1u) {
            mul = 1u;
        }
        if (mul > 2u) {
            mul = 2u;
        }
        walk_fp = (int16_t)(units_to_fp(1u, ph->meter) * (int16_t)mul);
    }

    if (ph->mode != (uint8_t)R01_GAME_MODE_PLATFORMER) {
        (void)integrate_axis(&x, &ph->frac_x, (int16_t)(walk_fp * (int16_t)in_dx), y, 1u, move_ok, ctx);
        (void)integrate_axis(&y, &ph->frac_y, (int16_t)(walk_fp * (int16_t)in_dy), x, 0u, move_ok, ctx);
        anim_dx = in_dx;
        anim_dy = in_dy;
    } else {
        int16_t g;
        gravity_fp = gravity_to_fp(ph->gravity, ph->meter);
        jump_fp = units_to_fp(ph->jump, ph->meter);
        fall_max_fp = units_to_fp(ph->fall_max, ph->meter);
        if (jump_pressed && ph->grounded) {
            ph->vel_y = (int16_t)(-jump_fp);
            ph->grounded = 0;
            ph->frac_y = 0;
        } else {
            g = gravity_fp;
            if (ph->vel_y < 0 && !jump_down) {
                g = (int16_t)(gravity_fp * R01_PLAT_JUMP_RELEASE_MUL);
            }
            ph->vel_y = (int16_t)(ph->vel_y + g);
            if (ph->vel_y > fall_max_fp) {
                ph->vel_y = fall_max_fp;
            }
        }
        (void)integrate_axis(&x, &ph->frac_x, (int16_t)(walk_fp * (int16_t)in_dx), y, 1u, move_ok, ctx);
        if (!integrate_axis(&y, &ph->frac_y, ph->vel_y, x, 0u, move_ok, ctx)) {
            ph->vel_y = 0;
        }
        ph->grounded = (uint8_t)!move_ok(ctx, x, (uint16_t)(y + 1u));
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
