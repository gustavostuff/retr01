#ifndef R01_PLAYER_H
#define R01_PLAYER_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

uint8_t r01_player_moving_x(const R01GameCtx *ctx);
void r01_player_set_move_mul(R01GameCtx *ctx, uint8_t mul);

#endif
