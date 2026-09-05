#ifndef R01_BGM_HOST_H
#define R01_BGM_HOST_H

/* Host-side BGM preview (Studio Play / standalone emu). Not cart APU protocol. */

/* Note denominator: 4=quarter, 8=eighth, 16=sixteenth. One host step = one such note. */
#define R01_BGM_NOTE_DIV 4
#define R01_BGM_STEPS_PER_BEAT (R01_BGM_NOTE_DIV / 4)
#if R01_BGM_STEPS_PER_BEAT < 1
#error R01_BGM_NOTE_DIV must be >= 4
#endif

#define R01_BGM_STEPS 256
#define R01_BGM_CH 5
#define R01_BGM_TOKEN 5

int r01_bgm_host_init(void);
void r01_bgm_host_shutdown(void);

/* Play track 1..N. path may be NULL to use built-in demo for track 1.
 * path points at a flat bin: steps * CH * TOKEN bytes (steps <= R01_BGM_STEPS). */
int r01_bgm_host_play(int track, const char *path);
/* cells[step][ch][tok]; steps is loop length (1..R01_BGM_STEPS). */
void r01_bgm_host_play_cells(char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN], int steps);
void r01_bgm_host_stop(void);
void r01_bgm_host_pause(void);
void r01_bgm_host_resume(void);
/* Fixed short one-shots (P1 face buttons). id: R01_SFX_X / R01_SFX_Y */
#define R01_SFX_X 1
#define R01_SFX_Y 2
void r01_bgm_host_sfx_play(int id);
int r01_bgm_host_playing(void); /* advancing (not paused) */
int r01_bgm_host_paused(void);
int r01_bgm_host_step(void); /* current step or -1 if stopped */
/* Fractional playhead in steps: step + (1 - samples_left/samples_per_step). -1 if idle. */
float r01_bgm_host_position(void);
int r01_bgm_host_track_steps(void);

#endif
