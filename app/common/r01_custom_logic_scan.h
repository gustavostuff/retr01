#ifndef R01_CUSTOM_LOGIC_SCAN_H
#define R01_CUSTOM_LOGIC_SCAN_H

#include <stddef.h>

/* Parse r01_camera_set_deadzone(ctx, dx, dy) from custom_logic.c. Returns 0 on match. */
int r01_custom_logic_scan_deadzone(const char *path, int *out_dx, int *out_dy);

/* Parse r01_bgm_play(ctx, track) — track is 1-based. Returns 0 on match. */
int r01_custom_logic_scan_bgm_play(const char *path, int *out_track);

/* foo.r01proj -> sibling C/custom_logic.c */
int r01_custom_logic_path_for_project(const char *proj_path, char *out, size_t out_cap);

#endif
