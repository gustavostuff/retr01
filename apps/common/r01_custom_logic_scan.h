#ifndef R01_CUSTOM_LOGIC_SCAN_H
#define R01_CUSTOM_LOGIC_SCAN_H

#include <stddef.h>

/* Parse r01_camera_set_deadzone(ctx, dx, dy) or r01_camera_disable_deadzone(ctx).
 * Returns 0 on match (disable packs as 0,0). Skips // comments. */
int r01_custom_logic_scan_deadzone(const char *path, int *out_dx, int *out_dy);

/* Parse r01_bg0_set_wrap(ctx, wrap_x, wrap_y). Non-zero = wrap that axis. Returns 0 on match. */
int r01_custom_logic_scan_bg0_wrap(const char *path, int *out_wrap_x, int *out_wrap_y);

/* Parse r01_bg0_set_clip_to_bg1(ctx, enable). Non-zero = clip BG0 to BG1 slots. Returns 0 on match. */
int r01_custom_logic_scan_bg0_clip_bg1(const char *path, int *out_enable);

/* Parse r01_bgm_play(ctx, track) - track is 1-based. Returns 0 on match. */
int r01_custom_logic_scan_bgm_play(const char *path, int *out_track);

/* Parse r01_game_set_mode(ctx, mode). Returns 0 on match. */
int r01_custom_logic_scan_game_mode(const char *path, int *out_mode);

/* Parse r01_platformer_set_gravity(ctx, n). Returns 0 on match. */
int r01_custom_logic_scan_plat_gravity(const char *path, int *out_gravity);

/* Parse r01_platformer_set_jump(ctx, n). Returns 0 on match. */
int r01_custom_logic_scan_plat_jump(const char *path, int *out_jump);

/* Parse r01_platformer_set_meter(ctx, n). Returns 0 on match. */
int r01_custom_logic_scan_plat_meter(const char *path, int *out_meter);

/* foo.r01proj -> sibling C/custom_logic.c */
int r01_custom_logic_path_for_project(const char *proj_path, char *out, size_t out_cap);

/* output_root/C/custom_logic.c (cart lives beside C/ and data/). */
int r01_custom_logic_path_for_output(const char *output_root, char *out, size_t out_cap);

/* output_root/data/bgm_trackN.bin - track is 1-based. */
int r01_bgm_track_bin_path(const char *output_root, int track_1based, char *out, size_t out_cap);

#endif
