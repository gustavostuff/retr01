#ifndef R01_PLAY_CAMERA_H
#define R01_PLAY_CAMERA_H

#define R01_PLAY_CAM_DEADZONE_X_DEFAULT 32
#define R01_PLAY_CAM_DEADZONE_Y_DEFAULT 30

#define R01_PLAY_CAM_AXIS_BOTH 0
#define R01_PLAY_CAM_AXIS_H 1
#define R01_PLAY_CAM_AXIS_V 2

/* deadzone_x/y: width/height of the centered viewport rectangle (see docs/selling_points.md). */
void r01_play_camera_update(int *cam_x, int *cam_y, int anchor_x, int anchor_y, int player_w, int player_h,
                            int screen_w, int screen_h, int deadzone_x, int deadzone_y, int axis_lock);

/* Ensure anchor (entity origin) is inside the dead zone; call before first frame after spawn/warp. */
void r01_play_camera_snap(int *cam_x, int *cam_y, int anchor_x, int anchor_y, int player_w, int player_h,
                          int screen_w, int screen_h, int deadzone_x, int deadzone_y, int axis_lock);

#endif
