#ifndef RETR01_TYPES_H
#define RETR01_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define RETR01_SCREEN_TILE_BYTES 960
#define RETR01_SCREEN_ATTR_BYTES 240
#define RETR01_SCREEN_BYTES 1200

#define RETR01_NT_W 32
#define RETR01_NT_H 30

#define RETR01_MASTER_PALETTE_ENTRIES 64
#define RETR01_MAX_SCREENS_PER_WORLD 64
#define RETR01_MAX_WORLDS 8

#define RETR01_CART_MAGIC "RETR01"
#define RETR01_CART_HEADER_SIZE 0x30

#define RETR01_MAP_MAGIC_0 'M'
#define RETR01_MAP_MAGIC_1 'A'
#define RETR01_MAP_MAGIC_2 'P'
#define RETR01_MAP_MAGIC_3 0x01

typedef struct retr01_rgb {
    uint8_t r, g, b;
} retr01_rgb_t;

typedef struct retr01_screen {
    uint8_t tiles[RETR01_SCREEN_TILE_BYTES];
    uint8_t attrs[RETR01_SCREEN_ATTR_BYTES];
    uint8_t col;
    uint8_t row;
    uint8_t flags;
    uint8_t authored_bank;
} retr01_screen_t;

typedef struct retr01_dir_entry {
    uint8_t col;
    uint8_t row;
    uint8_t flags;
    uint32_t data_off; /* 24-bit on wire */
} retr01_dir_entry_t;

typedef struct retr01_world_desc {
    uint8_t grid_w;
    uint8_t grid_h;
    uint8_t screen_count;
    uint32_t empty_off;
    retr01_dir_entry_t directory[RETR01_MAX_SCREENS_PER_WORLD];
} retr01_world_desc_t;

typedef struct retr01_cart {
    uint8_t *prg;
    size_t prg_size;
    uint8_t *chr;
    size_t chr_size;
    uint8_t *map;
    size_t map_size;
    uint8_t world_count;
} retr01_cart_t;

typedef struct retr01_master_palette {
    retr01_rgb_t entries[RETR01_MASTER_PALETTE_ENTRIES];
    uint8_t bg_palettes[4][4];
    uint8_t sprite_palettes[4][4];
    uint8_t backdrop_index;
} retr01_master_palette_t;

#endif
