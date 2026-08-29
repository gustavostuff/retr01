#ifndef R01_CAMERA_H
#define R01_CAMERA_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
#define R01_CAM_AXIS_BOTH 0
#define R01_CAM_AXIS_H 1
#define R01_CAM_AXIS_V 2
void r01_game_camera_update(R01GameCtx *ctx);
void r01_game_camera_snap(R01GameCtx *ctx);
void r01_camera_set_deadzone(R01GameCtx *ctx, int dx, int dy);
void r01_camera_set_axis_lock(R01GameCtx *ctx, int mode);

#endif
