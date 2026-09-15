#ifndef R01_SFX_H
#define R01_SFX_H

typedef struct R01GameCtx R01GameCtx;
#define R01_SFX_X 1 /* pulse blip (P1 X / fire) */
#define R01_SFX_Y 2 /* noise tick (P1 Y / face) */
/* Short SFX. Host Play/emu softsynth uses fixed voices for X/Y. */
void r01_sfx_play(R01GameCtx *ctx, int id);

#endif
