#ifndef R01_BG0_H
#define R01_BG0_H

typedef struct R01GameCtx R01GameCtx;
/* Tile present BG0 layout on X/Y when sampling leaves the box. Rate unchanged. */
void r01_bg0_set_wrap(R01GameCtx *ctx, int wrap_x, int wrap_y);

#endif
