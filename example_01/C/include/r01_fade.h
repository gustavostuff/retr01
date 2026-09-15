#ifndef R01_FADE_H
#define R01_FADE_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
#define R01_FADE_BLACK 0
#define R01_FADE_WHITE 1
#define R01_FADE_MAX 255
void r01_game_fade_start(R01GameCtx *ctx, int to_black_or_white, int target_level);
int r01_game_fade_active(const R01GameCtx *ctx);
int r01_game_fade_tick(R01GameCtx *ctx);

#endif
