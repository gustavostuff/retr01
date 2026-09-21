#ifndef R01_SFX_H
#define R01_SFX_H

typedef struct R01GameCtx R01GameCtx;
#define R01_SFX_X 1 /* pulse blip */
#define R01_SFX_Y 2 /* noise tick */
/* Short SFX on voices 6-8. */
void r01_sfx_play(R01GameCtx *ctx, int id);

#endif
