#ifndef R01_PROJECTILE_H
#define R01_PROJECTILE_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
int r01_projectile_fire(R01GameCtx *ctx, int dx, int dy, int speed);
void r01_projectile_tick(R01GameCtx *ctx);
int r01_projectile_count_active(const R01GameCtx *ctx);

#endif
