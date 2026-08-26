#ifndef RETR01_STUDIO_TYPES_H
#define RETR01_STUDIO_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define R01_SCREEN_TILES_X 16
#define R01_SCREEN_TILES_Y 15
#define R01_SCREEN_PX_W 128
#define R01_SCREEN_PX_H 120
#define R01_TILES_PER_SCREEN 240
#define R01_ATTRS_PER_SCREEN 240

#define R01_GRID_MAX 8
#define R01_DEFAULT_GRID 3
#define R01_MAX_SCREENS (R01_GRID_MAX * R01_GRID_MAX)
#define R01_START_COL 1
#define R01_START_ROW 1

#define R01_MAX_WORLDS 8
#define R01_BG_BANKS 4
#define R01_SPR_BANKS 4
#define R01_TILES_PER_BANK 256
#define R01_TILE_BYTES 16
#define R01_BANK_CHR_BYTES (R01_TILES_PER_BANK * R01_TILE_BYTES)

#define R01_MASTER_COLORS 64
#define R01_PAL_COLORS 4
#define R01_PALS_PER_ROW 4
#define R01_PAL_ROWS 8
#define R01_PAL_COUNT (R01_PAL_ROWS * R01_PALS_PER_ROW)
#define R01_PAL_PLANE_BYTES (R01_PAL_COUNT * R01_PAL_COLORS)

#define R01_CART_FLASH_BYTES (512u * 1024u)
#define R01_PRG_BYTES 32768u
#define R01_CHR_BANK_BYTES 4096u
#define R01_CART_FORMAT_VER 1

#define R01_NAME_MAX 64
#define R01_PATH_MAX 512
#define R01_JSON_VER 4

#define R01_GAME_DIR "test_game"
#define R01_DEFAULT_PROJECT R01_GAME_DIR "/test.r01proj"
#define R01_DEFAULT_CART_STEM R01_GAME_DIR "/test"

/* BG attr (docs/02) */
#define R01_ATTR_BANK_MASK 0x03u
#define R01_ATTR_PAL_MASK 0x0Cu
#define R01_ATTR_PAL_SHIFT 2
#define R01_ATTR_FLIP_H 0x10u
#define R01_ATTR_FLIP_V 0x20u

/* One 4-color palette (master indices into Color PROM). */
typedef struct R01PalRow {
    uint8_t idx[R01_PAL_COLORS];
} R01PalRow;

typedef struct R01Screen {
    int col;
    int row;
    int present;
    uint8_t pixels[R01_SCREEN_PX_W * R01_SCREEN_PX_H];
    uint8_t tiles[R01_TILES_PER_SCREEN];
    uint8_t attrs[R01_ATTRS_PER_SCREEN];
} R01Screen;

typedef struct R01ChrBank {
    int tile_count;
    uint8_t chr[R01_BANK_CHR_BYTES];
} R01ChrBank;

typedef R01ChrBank R01BgBank;
typedef R01ChrBank R01SprBank;

typedef struct R01World {
    int present;
    int grid_cols;
    int grid_rows;
    int default_bg_bank;
    int default_pal_row;
    R01Screen screens[R01_MAX_SCREENS];
    int screen_count;
    R01BgBank bg_banks[R01_BG_BANKS];
    R01SprBank spr_banks[R01_SPR_BANKS];
} R01World;

typedef struct R01Project {
    char name[R01_NAME_MAX];
    int active_screen; /* index 0..8 into world0.screens */
    /* 8 rows × 4 pals each (docs/02). Index [row][pal]. */
    R01PalRow global_pal_bg[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01PalRow global_pal_spr[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01World worlds[R01_MAX_WORLDS];
} R01Project;

static inline int r01_attr_bank(uint8_t a) {
    return (int)(a & R01_ATTR_BANK_MASK);
}
static inline int r01_attr_pal(uint8_t a) {
    return (int)((a & R01_ATTR_PAL_MASK) >> R01_ATTR_PAL_SHIFT);
}
static inline int r01_attr_flip_h(uint8_t a) {
    return (a & R01_ATTR_FLIP_H) != 0;
}
static inline int r01_attr_flip_v(uint8_t a) {
    return (a & R01_ATTR_FLIP_V) != 0;
}

static inline uint8_t r01_attr_pack(int bank, int pal, int flip_h, int flip_v) {
    uint8_t a = (uint8_t)((bank & 3) | ((pal & 3) << R01_ATTR_PAL_SHIFT));
    if (flip_h) {
        a |= R01_ATTR_FLIP_H;
    }
    if (flip_v) {
        a |= R01_ATTR_FLIP_V;
    }
    return a;
}

#endif
