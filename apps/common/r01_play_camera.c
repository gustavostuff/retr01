#include "r01_play_camera.h"

/*
 * Snap both edges to the same parity as the viewport center (screen/2).
 * A 2 px step from a centered spawn then lands on an edge, so the first
 * follow is +2 / -2 instead of a 1 px overflow hitch (run-on-X).
 */
static void align_deadzone_parity(int16_t center, int16_t *lo, int16_t *hi) {
    if (!lo || !hi) {
        return;
    }
    if (((*lo) & 1) != (center & 1) && *lo < *hi) {
        (*lo)++;
    }
    if (((*hi) & 1) != (center & 1) && *hi > *lo) {
        (*hi)--;
    }
}

static void deadzone_h_bounds(uint8_t screen_w, uint8_t dz_w, int16_t *out_left, int16_t *out_right) {
    int16_t left;
    int16_t right;
    if (dz_w == 0u || dz_w >= screen_w) {
        left = 0;
        right = (int16_t)screen_w - 1;
    } else {
        left = (int16_t)((screen_w - dz_w) / 2u);
        right = (int16_t)(left + (int16_t)dz_w - 1);
        align_deadzone_parity((int16_t)(screen_w / 2u), &left, &right);
    }
    if (out_left) {
        *out_left = left;
    }
    if (out_right) {
        *out_right = right;
    }
}

static void deadzone_v_bounds(uint8_t screen_h, uint8_t dz_h, int16_t *out_top, int16_t *out_bottom) {
    int16_t top;
    int16_t bottom;
    if (dz_h == 0u || dz_h >= screen_h) {
        top = 0;
        bottom = (int16_t)screen_h - 1;
    } else {
        top = (int16_t)((screen_h - dz_h) / 2u);
        bottom = (int16_t)(top + (int16_t)dz_h - 1);
        align_deadzone_parity((int16_t)(screen_h / 2u), &top, &bottom);
    }
    if (out_top) {
        *out_top = top;
    }
    if (out_bottom) {
        *out_bottom = bottom;
    }
}

static int16_t clamp_cam(int16_t v) {
    if (v < 0) {
        return 0;
    }
    return v;
}

void r01_play_camera_update(uint16_t *cam_x, uint16_t *cam_y, uint16_t anchor_x, uint16_t anchor_y,
                            uint8_t player_w, uint8_t player_h, uint8_t screen_w, uint8_t screen_h,
                            uint8_t deadzone_x, uint8_t deadzone_y, uint8_t axis_lock) {
    int16_t ax;
    int16_t ay;
    int16_t cx;
    int16_t cy;
    int16_t target_x;
    int16_t target_y;
    if (!cam_x || !cam_y) {
        return;
    }
    cx = (int16_t)*cam_x;
    cy = (int16_t)*cam_y;
    if (deadzone_x > 0u) {
        ax = (int16_t)anchor_x;
    } else {
        ax = (int16_t)(anchor_x + (uint16_t)(player_w / 2u));
    }
    if (deadzone_y > 0u) {
        ay = (int16_t)anchor_y;
    } else {
        ay = (int16_t)(anchor_y + (uint16_t)(player_h / 2u));
    }
    target_x = (int16_t)(ax - (int16_t)(screen_w / 2u));
    target_y = (int16_t)(ay - (int16_t)(screen_h / 2u));
    if (axis_lock != R01_PLAY_CAM_AXIS_V) {
        if (deadzone_x > 0u) {
            if (deadzone_x < screen_w) {
                int16_t left;
                int16_t right;
                int16_t sx = (int16_t)(ax - cx);
                deadzone_h_bounds(screen_w, deadzone_x, &left, &right);
                if (sx < left) {
                    cx = (int16_t)(ax - left);
                } else if (sx > right) {
                    cx = (int16_t)(ax - right);
                }
            }
        } else {
            cx = target_x;
        }
    }
    if (axis_lock != R01_PLAY_CAM_AXIS_H) {
        if (deadzone_y > 0u) {
            if (deadzone_y < screen_h) {
                int16_t top;
                int16_t bottom;
                int16_t sy = (int16_t)(ay - cy);
                deadzone_v_bounds(screen_h, deadzone_y, &top, &bottom);
                if (sy < top) {
                    cy = (int16_t)(ay - top);
                } else if (sy > bottom) {
                    cy = (int16_t)(ay - bottom);
                }
            }
        } else {
            cy = target_y;
        }
    }
    *cam_x = (uint16_t)clamp_cam(cx);
    *cam_y = (uint16_t)clamp_cam(cy);
}

void r01_play_camera_snap(uint16_t *cam_x, uint16_t *cam_y, uint16_t anchor_x, uint16_t anchor_y, uint8_t player_w,
                          uint8_t player_h, uint8_t screen_w, uint8_t screen_h, uint8_t deadzone_x,
                          uint8_t deadzone_y, uint8_t axis_lock) {
    int16_t ax;
    int16_t ay;
    int16_t cx;
    int16_t cy;
    if (!cam_x || !cam_y) {
        return;
    }
    cx = (int16_t)*cam_x;
    cy = (int16_t)*cam_y;
    if (deadzone_x > 0u) {
        ax = (int16_t)anchor_x;
    } else {
        ax = (int16_t)(anchor_x + (uint16_t)(player_w / 2u));
    }
    if (deadzone_y > 0u) {
        ay = (int16_t)anchor_y;
    } else {
        ay = (int16_t)(anchor_y + (uint16_t)(player_h / 2u));
    }
    if (axis_lock != R01_PLAY_CAM_AXIS_V) {
        cx = (int16_t)(ax - (int16_t)(screen_w / 2u));
    }
    if (axis_lock != R01_PLAY_CAM_AXIS_H) {
        cy = (int16_t)(ay - (int16_t)(screen_h / 2u));
    }
    *cam_x = (uint16_t)clamp_cam(cx);
    *cam_y = (uint16_t)clamp_cam(cy);
    r01_play_camera_update(cam_x, cam_y, anchor_x, anchor_y, player_w, player_h, screen_w, screen_h, deadzone_x,
                           deadzone_y, axis_lock);
}
