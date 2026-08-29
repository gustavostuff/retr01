#ifndef R01_WARP_H
#define R01_WARP_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
void r01_game_warp_to_tile(R01GameCtx *ctx, int screen_col, int screen_row, int tile_col,
                           int tile_row);
int r01_game_warp_by_id(R01GameCtx *ctx, const char *warp_id);
void r01_game_warp_check(R01GameCtx *ctx);

#endif
