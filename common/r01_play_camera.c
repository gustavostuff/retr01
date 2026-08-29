#include "r01_play_camera.h"

void r01_play_camera_update(int *cam_x, int *cam_y, int player_x, int player_y, int player_w, int player_h,
                            int screen_w, int screen_h, int deadzone_x, int deadzone_y, int axis_lock) {
    int ax, ay, target_x, target_y;
    if (!cam_x || !cam_y) {
        return;
    }
    ax = player_x + player_w / 2;
    ay = player_y + player_h / 2;
    target_x = ax - screen_w / 2;
    target_y = ay - screen_h / 2;
    if (axis_lock != R01_PLAY_CAM_AXIS_V) {
        if (deadzone_x > 0) {
            if (ax - *cam_x < deadzone_x) {
                *cam_x = ax - deadzone_x;
            } else if (ax - *cam_x > screen_w - deadzone_x - player_w) {
                *cam_x = ax - (screen_w - deadzone_x - player_w);
            }
        } else {
            *cam_x = target_x;
        }
    }
    if (axis_lock != R01_PLAY_CAM_AXIS_H) {
        if (deadzone_y > 0) {
            if (ay - *cam_y < deadzone_y) {
                *cam_y = ay - deadzone_y;
            } else if (ay - *cam_y > screen_h - deadzone_y - player_h) {
                *cam_y = ay - (screen_h - deadzone_y - player_h);
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
