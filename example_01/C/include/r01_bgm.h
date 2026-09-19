#ifndef R01_BGM_H
#define R01_BGM_H

typedef struct R01GameCtx R01GameCtx;
/* Start looping BGM track (1-based). Host Play runs the NMI tracker into $7F40. */
void r01_bgm_play(R01GameCtx *ctx, int track);
void r01_bgm_stop(R01GameCtx *ctx);

#endif
