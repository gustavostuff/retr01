#ifndef R01_PLAYER_H
#define R01_PLAYER_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
void r01_player_warp(R01GameCtx *ctx, int col, int row);
void r01_player_set_type(uint8_t type_id);
/* Hold face X for 2x walk. Host Play packs this into world flags bit 5. */
void r01_player_set_run_on_x(R01GameCtx *ctx);

#include "r01_player_anim.h"

#endif
