#ifndef R01_PLAY_CAMERA_H
#define R01_PLAY_CAMERA_H

#include <stdint.h>

#define R01_PLAY_CAM_DEADZONE_X_DEFAULT 32
#define R01_PLAY_CAM_DEADZONE_Y_DEFAULT 30

#define R01_PLAY_CAM_AXIS_BOTH 0
#define R01_PLAY_CAM_AXIS_H 1
#define R01_PLAY_CAM_AXIS_V 2

#ifndef R01_CAM_AXIS_BOTH
#define R01_CAM_AXIS_BOTH R01_PLAY_CAM_AXIS_BOTH
#define R01_CAM_AXIS_H R01_PLAY_CAM_AXIS_H
#define R01_CAM_AXIS_V R01_PLAY_CAM_AXIS_V
#endif

#ifndef R01_CAM_DEADZONE_X_DEFAULT
#define R01_CAM_DEADZONE_X_DEFAULT R01_PLAY_CAM_DEADZONE_X_DEFAULT
#define R01_CAM_DEADZONE_Y_DEFAULT R01_PLAY_CAM_DEADZONE_Y_DEFAULT
#endif
#ifndef R01_CAM_DEADZONE_OFF
#define R01_CAM_DEADZONE_OFF 0
#endif

/* Packed dead-zone width/height. Follow snaps edges to viewport-center
 * parity (docs/general/world-scrolling.md). Live box may be 1 px smaller. */
void r01_play_camera_update(uint16_t *cam_x, uint16_t *cam_y, uint16_t anchor_x, uint16_t anchor_y,
                            uint8_t player_w, uint8_t player_h, uint8_t screen_w, uint8_t screen_h,
                            uint8_t deadzone_x, uint8_t deadzone_y, uint8_t axis_lock);

/* Ensure anchor (entity origin) is inside the dead zone; call before first frame after spawn/warp. */
void r01_play_camera_snap(uint16_t *cam_x, uint16_t *cam_y, uint16_t anchor_x, uint16_t anchor_y, uint8_t player_w,
                          uint8_t player_h, uint8_t screen_w, uint8_t screen_h, uint8_t deadzone_x,
                          uint8_t deadzone_y, uint8_t axis_lock);

#endif
