#ifndef R01_BGM_H
#define R01_BGM_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

void r01_bgm_play(R01GameCtx *ctx, uint8_t track);
void r01_bgm_stop(R01GameCtx *ctx);

#endif
