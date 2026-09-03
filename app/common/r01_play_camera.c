#include "r01_play_camera.h"

static void deadzone_h_bounds(int screen_w, int dz_w, int *out_left, int *out_right) {
    int left;
    int right;
    if (dz_w <= 0 || dz_w >= screen_w) {
        left = 0;
        right = screen_w - 1;
    } else {
        left = (screen_w - dz_w) / 2;
        right = left + dz_w - 1;
    }
    if (out_left) {
        *out_left = left;
    }
    if (out_right) {
        *out_right = right;
    }
}

static void deadzone_v_bounds(int screen_h, int dz_h, int *out_top, int *out_bottom) {
    int top;
    int bottom;
    if (dz_h <= 0 || dz_h >= screen_h) {
        top = 0;
        bottom = screen_h - 1;
    } else {
        top = (screen_h - dz_h) / 2;
        bottom = top + dz_h - 1;
    }
    if (out_top) {
        *out_top = top;
    }
    if (out_bottom) {
        *out_bottom = bottom;
    }
}

void r01_play_camera_update(int *cam_x, int *cam_y, int anchor_x, int anchor_y, int player_w, int player_h,
                            int screen_w, int screen_h, int deadzone_x, int deadzone_y, int axis_lock) {
    int ax;
    int ay;
    int target_x;
    int target_y;
    if (!cam_x || !cam_y) {
        return;
    }
    if (deadzone_x > 0) {
        ax = anchor_x;
    } else {
        ax = anchor_x + player_w / 2;
    }
    if (deadzone_y > 0) {
        ay = anchor_y;
    } else {
        ay = anchor_y + player_h / 2;
    }
    target_x = ax - screen_w / 2;
    target_y = ay - screen_h / 2;
    if (axis_lock != R01_PLAY_CAM_AXIS_V) {
        if (deadzone_x > 0) {
            if (deadzone_x < screen_w) {
                int left;
                int right;
                int sx = ax - *cam_x;
                deadzone_h_bounds(screen_w, deadzone_x, &left, &right);
                if (sx < left) {
                    *cam_x = ax - left;
                } else if (sx > right) {
                    *cam_x = ax - right;
                }
            }
        } else {
            *cam_x = target_x;
        }
    }
    if (axis_lock != R01_PLAY_CAM_AXIS_H) {
        if (deadzone_y > 0) {
            if (deadzone_y < screen_h) {
                int top;
                int bottom;
                int sy = ay - *cam_y;
                deadzone_v_bounds(screen_h, deadzone_y, &top, &bottom);
                if (sy < top) {
                    *cam_y = ay - top;
                } else if (sy > bottom) {
                    *cam_y = ay - bottom;
                }
            }
        } else {
            *cam_y = target_y;
        }
    }
    if (*cam_x < 0) {
        *cam_x = 0;
    }
    if (*cam_y < 0) {
        *cam_y = 0;
    }
}

void r01_play_camera_snap(int *cam_x, int *cam_y, int anchor_x, int anchor_y, int player_w, int player_h,
                          int screen_w, int screen_h, int deadzone_x, int deadzone_y, int axis_lock) {
    int ax;
    int ay;
    int target_x;
    int target_y;
    if (!cam_x || !cam_y) {
        return;
    }
    if (deadzone_x > 0) {
        ax = anchor_x;
    } else {
        ax = anchor_x + player_w / 2;
    }
    if (deadzone_y > 0) {
        ay = anchor_y;
    } else {
        ay = anchor_y + player_h / 2;
    }
    target_x = ax - screen_w / 2;
    target_y = ay - screen_h / 2;
    if (axis_lock != R01_PLAY_CAM_AXIS_V) {
        *cam_x = target_x;
    }
    if (axis_lock != R01_PLAY_CAM_AXIS_H) {
        *cam_y = target_y;
    }
    if (*cam_x < 0) {
        *cam_x = 0;
    }
    if (*cam_y < 0) {
        *cam_y = 0;
    }
    r01_play_camera_update(cam_x, cam_y, anchor_x, anchor_y, player_w, player_h, screen_w, screen_h, deadzone_x,
                           deadzone_y, axis_lock);
}
