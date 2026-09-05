#ifndef R01_BGM_HOST_H
#define R01_BGM_HOST_H

/* Host-side BGM preview (Studio Play / standalone emu). Not cart APU protocol. */

#define R01_BGM_STEPS 32
#define R01_BGM_CH 5
#define R01_BGM_TOKEN 5

int r01_bgm_host_init(void);
void r01_bgm_host_shutdown(void);

/* Play track 1..N. path may be NULL to use built-in demo for track 1.
 * path points at a flat bin: STEPS * CH * TOKEN bytes. */
int r01_bgm_host_play(int track, const char *path);
void r01_bgm_host_play_cells(char cells[R01_BGM_STEPS][R01_BGM_CH][R01_BGM_TOKEN]);
void r01_bgm_host_stop(void);
int r01_bgm_host_playing(void);
int r01_bgm_host_step(void); /* current step or -1 */

#endif
