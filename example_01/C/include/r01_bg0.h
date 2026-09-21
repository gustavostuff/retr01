#ifndef R01_BG0_H
#define R01_BG0_H

typedef struct R01GameCtx R01GameCtx;
#define R01_BG0_WRAP_OFF 0
#define R01_BG0_WRAP_ON 1
#define R01_BG0_CLIP_OFF 0
#define R01_BG0_CLIP_ON 1
/* Tile present BG0 layout on X/Y when sampling leaves the box. Wrap axes use period n/n rate. */
void r01_bg0_set_wrap(R01GameCtx *ctx, int wrap_x, int wrap_y);
/* Hide BG0 outside present BG1 camera slots when enable != 0. */
void r01_bg0_set_clip_to_bg1(R01GameCtx *ctx, int enable);

#endif
