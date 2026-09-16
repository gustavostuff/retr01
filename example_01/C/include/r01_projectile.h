#ifndef R01_PROJECTILE_H
#define R01_PROJECTILE_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

/* Aim vector components for r01_projectile_fire (normalized by the runtime). */
#define R01_AIM_X_NONE 0
#define R01_AIM_X_RIGHT 1
#define R01_AIM_X_LEFT (-1)
#define R01_AIM_Y_NONE 0
#define R01_AIM_Y_DOWN 1
#define R01_AIM_Y_UP (-1)

#define R01_PROJ_SPEED_DEFAULT 3
#define R01_PROJ_SPEED_FAST 4

int r01_projectile_fire(R01GameCtx *ctx, int dx, int dy, int speed);
void r01_projectile_tick(R01GameCtx *ctx);
int r01_projectile_count_active(const R01GameCtx *ctx);

#endif
