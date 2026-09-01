#ifndef retr01_STUDIO_WARPS_H
#define retr01_STUDIO_WARPS_H

#include "retr01_studio/types.h"

void r01_world_warps_init(R01World *w);
int r01_world_warp_entrance_add(R01World *w, int screen_col, int screen_row, int tile_col, int tile_row);
int r01_world_warp_exit_set(R01World *w, int entrance_idx, int dest_screen_col, int dest_screen_row,
                            int dest_tile_col, int dest_tile_row, uint8_t flags);
int r01_world_warp_entrance_at(const R01World *w, int screen_col, int screen_row, int tile_col,
                               int tile_row);
int r01_world_warp_exit_for_entrance(const R01World *w, int entrance_idx);
void r01_world_warp_tile_world_pos(int screen_col, int screen_row, int tile_col, int tile_row,
                                   int *out_wx, int *out_wy);
int r01_world_warp_entrance_hit(const R01World *w, int player_x, int player_y, int player_w,
                                int player_h);

#endif
