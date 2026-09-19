#ifndef R01_BGM_FD_H
#define R01_BGM_FD_H

#include <stdint.h>

#include "r01_apu_fd.h"

/* Studio Host Play grid: steps x 5 channels x 5-char tokens. */
#define R01_BGM_FD_CH 5
#define R01_BGM_FD_TOKEN 5
#define R01_BGM_FD_STEPS_MAX 512

/* Match Studio WAVE grid tempo (eighth-note steps @ 140 BPM). */
#define R01_BGM_FD_TEMPO_BPM 140
#define R01_BGM_FD_STEPS_PER_BEAT 2
#define R01_BGM_FD_NMI_HZ 60

/* ASCII tracker token -> cart note/control byte (general_docs/sound.md). */
int r01_bgm_fd_token_payload(int ch, const char *tok, uint8_t *out);

/* NMI frames per Studio grid step (60 Hz tracker vs BPM x steps/beat). */
int r01_bgm_fd_frames_per_step(void);

/*
 * Encode cells as cart bytecode: per step FD(+mask+payload) then FE hold so the
 * note lasts one full grid step at 60 Hz NMI, then FA loop. Identical adjacent
 * steps are run-length encoded (one FD + longer FE) so long regions / sections
 * hold without re-triggers. Returns byte length or -1.
 */
int r01_bgm_fd_encode_cells(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int steps,
                            uint8_t *out, unsigned out_cap);

/* One grid step -> FD apply into regs (thin path without FE delays). */
int r01_bgm_fd_apply_step(const char cells[][R01_BGM_FD_CH][R01_BGM_FD_TOKEN], int step, int steps,
                          uint8_t *regs);

#endif
