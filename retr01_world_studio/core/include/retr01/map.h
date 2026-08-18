#ifndef RETR01_MAP_H
#define RETR01_MAP_H

#include <stddef.h>
#include <stdint.h>

#include "retr01/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct retr01_map_build_screen {
    retr01_screen_t screen;
} retr01_map_build_screen_t;

typedef struct retr01_map_build_world {
    retr01_world_desc_t desc;
    const retr01_map_build_screen_t *screens;
    size_t screen_count;
} retr01_map_build_world_t;

/* Build MAP-ROM blob (header + worlds + RLE payloads). Caller frees *out. */
int retr01_map_build(const retr01_map_build_world_t *worlds, size_t world_count,
                     uint8_t **out, size_t *out_len);

/* Find screen at (col,row) in world_index; decode into screen_out. */
int retr01_map_load_screen(const retr01_cart_t *cart, int world_index,
                           uint8_t col, uint8_t row, retr01_screen_t *screen_out);

typedef struct retr01_map_cell {
    uint8_t col;
    uint8_t row;
    uint8_t flags;
} retr01_map_cell_t;

/* List directory cells for a world. Returns 0 and writes *out_count. */
int retr01_map_list_cells(const retr01_cart_t *cart, int world_index, retr01_map_cell_t *out,
                          int max_out, int *out_count);

int retr01_map_world_count(const retr01_cart_t *cart);

#ifdef __cplusplus
}
#endif

#endif
