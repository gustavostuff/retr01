#ifndef R01_BG0_H
#define R01_BG0_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

#define R01_BG0_WRAP_OFF 0
#define R01_BG0_WRAP_ON 1
#define R01_BG0_CLIP_OFF 0
#define R01_BG0_CLIP_ON 1

void r01_bg0_set_wrap(R01GameCtx *ctx, uint8_t wrap_x, uint8_t wrap_y);
void r01_bg0_set_clip_to_bg1(R01GameCtx *ctx, uint8_t enable);

#endif
