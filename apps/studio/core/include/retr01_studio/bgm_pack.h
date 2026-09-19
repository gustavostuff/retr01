#ifndef retr01_STUDIO_BGM_PACK_H
#define retr01_STUDIO_BGM_PACK_H

#include "retr01_studio/types.h"

#include "r01_bgm_fd.h"

#include <stdint.h>

/* Flatten one Studio track into a grid. Returns step count (>= 1). */
int r01_bgm_flatten_track(const R01BgmData *bgm, int track,
                          char cells[R01_BGM_FD_STEPS_MAX][R01_BGM_FD_CH][R01_BGM_FD_TOKEN]);

/*
 * Pack tracker bytecode into PRG at $B000 and boot track at $80FE from
 * custom_logic.c r01_bgm_play(ctx, N). N is 1-based. 0 = no autoplay.
 */
void r01_bgm_pack_prg(uint8_t prg[R01_PRG_BYTES], const R01BgmData *bgm, const char *custom_logic_path);

#endif
