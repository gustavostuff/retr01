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
#define R01_MAX_WORLDS 8
#define R01_MAX_SCREENS_PER_WORLD 32
#define R01_MAX_PARALLAX_PLANES 2 /* HW VRAM slots 4-5 */
#define R01_GRID_SIZE 8
#define R01_BG_BANKS 4
#define R01_TILES_PER_BANK 256
#define R01_TILE_BYTES 16 /* 8x8 @ 2bpp */
#define R01_BANK_CHR_BYTES (R01_TILES_PER_BANK * R01_TILE_BYTES)
#define R01_NAME_MAX 64
#define R01_PATH_MAX 512

#define R01_MASTER_COLORS 64
#define R01_PAL_COLORS 4
#define R01_PAL_ROWS 4 /* per plane (BG or sprite) */

/* BG / OAM attr layout from docs/02 */
#define R01_ATTR_BANK_MASK 0x03u
#define R01_ATTR_PAL_MASK 0x0Cu
#define R01_ATTR_PAL_SHIFT 2
#define R01_ATTR_FLIP_H 0x10u
#define R01_ATTR_FLIP_V 0x20u
#define R01_ATTR_SOLID 0x40u /* soft / video-ignored */
#define R01_ATTR_ANIM 0x80u  /* soft: 4-frame strip, base 4-aligned */

typedef struct R01PalRow {
    uint8_t idx[R01_PAL_COLORS]; /* master Color PROM indices 0..63 */
} R01PalRow;

typedef struct R01Screen {
    int col; /* 0..7 grid */
    int row;
    int present;
    uint8_t pixels[R01_SCREEN_PX_W * R01_SCREEN_PX_H]; /* 0..3 */
    uint8_t tiles[R01_TILES_PER_SCREEN];
    uint8_t attrs[R01_ATTRS_PER_SCREEN];
} R01Screen;

/* Same 480 B payload as a screen; not placed on the world grid. */
typedef struct R01ParallaxPlane {
    int present;
    int slot; /* 0..1 -> HW VRAM slots 4..5 */
    uint8_t pixels[R01_SCREEN_PX_W * R01_SCREEN_PX_H];
    uint8_t tiles[R01_TILES_PER_SCREEN];
    uint8_t attrs[R01_ATTRS_PER_SCREEN];
} R01ParallaxPlane;

typedef struct R01BgBank {
    int tile_count; /* 0..256 */
    uint8_t chr[R01_BANK_CHR_BYTES];
} R01BgBank;

typedef struct R01World {
    int present;
    int default_bg_bank; /* 0..3 */
    int default_pal_row; /* 0..3 BG row hint */
    int use_world_pals;  /* 1 = world pal_* override globals */
    R01PalRow pal_bg[R01_PAL_ROWS];
    R01PalRow pal_spr[R01_PAL_ROWS];
    R01Screen screens[R01_MAX_SCREENS_PER_WORLD];
    int screen_count;
    R01ParallaxPlane planes[R01_MAX_PARALLAX_PLANES];
    R01BgBank bg_banks[R01_BG_BANKS];
} R01World;

typedef struct R01Project {
    char name[R01_NAME_MAX];
    int active_world;  /* 0..7 */
    int active_screen; /* index into worlds[aw].screens, or -1 */
    int active_plane;  /* 0..1 when editing a plane; -1 = grid screen */
    int generate_bank; /* 0..3 radio */
    int paint_color;   /* 0..3 */
    R01PalRow global_pal_bg[R01_PAL_ROWS];
    R01PalRow global_pal_spr[R01_PAL_ROWS];
    R01World worlds[R01_MAX_WORLDS];
} R01Project;

/* Mutable view of the buffers currently under the paint/attr tools. */
typedef struct R01EditSurface {
    uint8_t *pixels;
    uint8_t *tiles;
    uint8_t *attrs;
    int is_plane; /* 1 = parallax plane, 0 = grid screen */
    int index;
} R01EditSurface;

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
static inline int r01_attr_solid(uint8_t a) {
    return (a & R01_ATTR_SOLID) != 0;
}
static inline int r01_attr_anim(uint8_t a) {
    return (a & R01_ATTR_ANIM) != 0;
}

static inline uint8_t r01_attr_pack(int bank, int pal, int flip_h, int flip_v, int solid, int anim) {
    uint8_t a = (uint8_t)((bank & 3) | ((pal & 3) << R01_ATTR_PAL_SHIFT));
    if (flip_h) {
        a |= R01_ATTR_FLIP_H;
    }
    if (flip_v) {
        a |= R01_ATTR_FLIP_V;
    }
    if (solid) {
        a |= R01_ATTR_SOLID;
    }
    if (anim) {
        a |= R01_ATTR_ANIM;
    }
    return a;
}

#endif
