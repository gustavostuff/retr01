#ifndef retr01_STUDIO_BGM_PACK_H
#define retr01_STUDIO_BGM_PACK_H

#include "retr01_studio/types.h"

#include "r01_bgm_fd.h"

#include <stdint.h>

/* Flatten one Studio track into a grid. Returns step count (>= 1). */
int r01_bgm_flatten_track(const R01BgmData *bgm, int track,
                          char cells[R01_BGM_FD_STEPS_MAX][R01_BGM_FD_CH][R01_BGM_FD_TOKEN]);

/*
 * Pack tracker bytecode and per-track wavetable ids into a cart BGM blob.
 * Returns blob length (>= R01_BGM_HDR_V1) or -1.
 * Boot track at $80FE comes from custom_logic.c r01_bgm_play(ctx, N).
 */
int r01_bgm_pack_blob(uint8_t *blob, unsigned cap, const R01BgmData *bgm);
void r01_bgm_pack_boot(uint8_t prg[R01_PRG_BYTES], const uint8_t *blob, int blob_len,
                       const char *custom_logic_path);

#endif
