#include "retr01_sim/gamepad.h"

void r01s_gamepad_input_clear(R01sGamepadInput *gp) {
    if (!gp) {
        return;
    }
    gp->stick_x = 0;
    gp->stick_y = 0;
    gp->btn_x = 0;
    gp->btn_y = 0;
    gp->btn_coin = 0;
    gp->btn_start = 0;
}

uint8_t r01s_gamepad_stick_bits(int stick_x, int stick_y) {
    uint8_t bits = 0;
    if (stick_y < -R01S_GAMEPAD_STICK_DEAD) {
        bits |= R01S_PAD_UP;
    }
    if (stick_y > R01S_GAMEPAD_STICK_DEAD) {
        bits |= R01S_PAD_DOWN;
    }
    if (stick_x > R01S_GAMEPAD_STICK_DEAD) {
        bits |= R01S_PAD_RIGHT;
    }
    if (stick_x < -R01S_GAMEPAD_STICK_DEAD) {
        bits |= R01S_PAD_LEFT;
    }
    return bits;
}

uint8_t r01s_gamepad_encode(const R01sGamepadInput *gp) {
    uint8_t bits = 0;
    if (!gp) {
        return 0;
    }
    bits |= r01s_gamepad_stick_bits(gp->stick_x, gp->stick_y);
    if (gp->btn_x) {
        bits |= R01S_PAD_X;
    }
    if (gp->btn_y) {
        bits |= R01S_PAD_Y;
    }
    if (gp->btn_coin) {
        bits |= R01S_PAD_COIN;
    }
    if (gp->btn_start) {
        bits |= R01S_PAD_START;
    }
    return bits;
}

void r01s_gamepad_stick_clamp(int *stick_x, int *stick_y, int radius) {
    long dx;
    long dy;
    long dist2;
    long r2;
    if (!stick_x || !stick_y || radius <= 0) {
        return;
    }
    dx = *stick_x;
    dy = *stick_y;
    dist2 = dx * dx + dy * dy;
    r2 = (long)radius * radius;
    if (dist2 <= r2) {
        return;
    }
    /* Scale to circle edge (integer approx). */
    if (dist2 > 0) {
        int dist = 1;
        while ((long)dist * dist < dist2) {
            dist++;
        }
        if (dist > 0) {
            *stick_x = (int)(dx * radius / dist);
            *stick_y = (int)(dy * radius / dist);
        }
    }
}

void r01s_gamepad_stick_snap_digital(int *stick_x, int *stick_y, int travel, int dead) {
    if (!stick_x || !stick_y || travel <= 0) {
        return;
    }
    if (dead < 0) {
        dead = 0;
    }
    if (*stick_x > dead) {
        *stick_x = travel;
    } else if (*stick_x < -dead) {
        *stick_x = -travel;
    } else {
        *stick_x = 0;
    }
    if (*stick_y > dead) {
        *stick_y = travel;
    } else if (*stick_y < -dead) {
        *stick_y = -travel;
    } else {
        *stick_y = 0;
    }
}
