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
#define R01_GRID_SIZE 8 /* max cols/rows (hardware atlas) */
#define R01_BG_BANKS 4
#define R01_SPR_BANKS 4
#define R01_TILES_PER_BANK 256
#define R01_TILE_BYTES 16 /* 8x8 @ 2bpp */
#define R01_BANK_CHR_BYTES (R01_TILES_PER_BANK * R01_TILE_BYTES)
#define R01_MAX_OAM_PER_SCREEN 64
#define R01_MAX_METASPRITES 16
#define R01_MAX_META_PARTS 8
#define R01_MAX_META_FRAMES 4
#define R01_NAME_MAX 64
#define R01_PATH_MAX 512

#define R01_CART_FLASH_BYTES (512u * 1024u)
#define R01_PRG_BYTES 32768u
#define R01_CHR_BANK_BYTES 4096u
#define R01_CART_FORMAT_VER 1
#define R01_CART_FLAG_I2C_SAVE 0x01u

#define R01_MASTER_COLORS 64
#define R01_PAL_COLORS 4
#define R01_PAL_ROWS 4 /* per plane (BG or sprite) */

/* Scroll modes C4-C7 */
#define R01_SCROLL_PIXEL 0
#define R01_SCROLL_DEADZONE 1
#define R01_SCROLL_INSTANT 2
#define R01_SCROLL_HYBRID 3

/* C8 transitions */
#define R01_XITION_CUT 0
#define R01_XITION_FADE 1

/* BG attr (docs/02) */
#define R01_ATTR_BANK_MASK 0x03u
#define R01_ATTR_PAL_MASK 0x0Cu
#define R01_ATTR_PAL_SHIFT 2
#define R01_ATTR_FLIP_H 0x10u
#define R01_ATTR_FLIP_V 0x20u
#define R01_ATTR_SOLID 0x40u /* BG soft */
#define R01_ATTR_ANIM 0x80u  /* BG soft */

/* OAM attr: same BANK/PAL/FLIP; bits 6-7 are PRIORITY / SIZE */
#define R01_OAM_PRIORITY 0x40u
#define R01_OAM_SIZE 0x80u /* 0=8x8, 1=8x16 (even base tile) */

typedef struct R01PalRow {
    uint8_t idx[R01_PAL_COLORS]; /* master Color PROM indices 0..63 */
} R01PalRow;

typedef struct R01Oam {
    uint8_t x;
    uint8_t y;
    uint8_t tile;
    uint8_t attr;
} R01Oam;

typedef struct R01Screen {
    int col; /* 0..7 grid */
    int row;
    int present;
    uint8_t pixels[R01_SCREEN_PX_W * R01_SCREEN_PX_H]; /* 0..3 */
    uint8_t tiles[R01_TILES_PER_SCREEN];
    uint8_t attrs[R01_ATTRS_PER_SCREEN];
    R01Oam oam[R01_MAX_OAM_PER_SCREEN];
    int oam_count;
} R01Screen;

/* Same 480 B payload as a screen; not placed on the world grid. */
typedef struct R01ParallaxPlane {
    int present;
    int slot; /* 0..1 -> HW VRAM slots 4..5 */
    uint8_t pixels[R01_SCREEN_PX_W * R01_SCREEN_PX_H];
    uint8_t tiles[R01_TILES_PER_SCREEN];
    uint8_t attrs[R01_ATTRS_PER_SCREEN];
} R01ParallaxPlane;

typedef struct R01ChrBank {
    int tile_count; /* 0..256 */
    uint8_t chr[R01_BANK_CHR_BYTES];
} R01ChrBank;

typedef R01ChrBank R01BgBank;
typedef R01ChrBank R01SprBank;

typedef struct R01MetaPart {
    int8_t dx;
    int8_t dy;
    uint8_t tile;
    uint8_t attr;
} R01MetaPart;

typedef struct R01MetaSprite {
    int present;
    int part_count;
    R01MetaPart parts[R01_MAX_META_PARTS];
    int frame_count; /* 1..4; preview uses frame 0; frames share part layout, tile += frame for SIZE strips later */
} R01MetaSprite;

/* Project / world behavior knobs (Studio Phase 4 — not silicon timing). */
typedef struct R01Constraints {
    int player_meta;     /* C1: meta index for player (-1 = marker only) */
    int enemy_anim_rate; /* C2: ticks per meta frame (1..120) */
    int anim_rate;       /* C3: ticks per BG ANIM frame (1..120) */
    int scroll_mode;     /* C4-C7: PIXEL / DEADZONE / INSTANT / HYBRID */
    int deadzone_x;      /* edge inset px; free box = (128-2x)×(120-2y), default 48/45 → 32×30 */
    int deadzone_y;
    int transition;      /* C8: CUT / FADE */
} R01Constraints;

typedef struct R01World {
    int present;
    int grid_cols; /* 1..R01_GRID_SIZE — virtual atlas width */
    int grid_rows; /* 1..R01_GRID_SIZE — virtual atlas height */
    int default_bg_bank; /* 0..3 */
    int default_pal_row; /* 0..3 BG row hint */
    int use_world_pals;  /* 1 = world pal_* override globals */
    int use_constraints; /* 1 = constraints override project defaults */
    R01Constraints constraints;
    R01PalRow pal_bg[R01_PAL_ROWS];
    R01PalRow pal_spr[R01_PAL_ROWS];
    R01Screen screens[R01_MAX_SCREENS_PER_WORLD];
    int screen_count;
    R01ParallaxPlane planes[R01_MAX_PARALLAX_PLANES];
    R01BgBank bg_banks[R01_BG_BANKS];
    R01SprBank spr_banks[R01_SPR_BANKS];
    R01MetaSprite metas[R01_MAX_METASPRITES];
    int meta_count;
} R01World;

typedef struct R01Project {
    char name[R01_NAME_MAX];
    int active_world;  /* 0..7 */
    int active_screen; /* index into worlds[aw].screens, or -1 */
    int active_plane;  /* 0..1 when editing a plane; -1 = grid screen */
    int generate_bank; /* 0..3 radio (BG or SPR depending on layer) */
    int paint_color;   /* 0..3 */
    int has_cart_save; /* 1 = cart I2C EEPROM present (metadata / cart flags) */
    R01Constraints constraints; /* project defaults */
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
static inline int r01_oam_priority(uint8_t a) {
    return (a & R01_OAM_PRIORITY) != 0;
}
static inline int r01_oam_size_16(uint8_t a) {
    return (a & R01_OAM_SIZE) != 0;
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

static inline uint8_t r01_oam_pack(int bank, int pal, int flip_h, int flip_v, int priority, int size16) {
    uint8_t a = (uint8_t)((bank & 3) | ((pal & 3) << R01_ATTR_PAL_SHIFT));
    if (flip_h) {
        a |= R01_ATTR_FLIP_H;
    }
    if (flip_v) {
        a |= R01_ATTR_FLIP_V;
    }
    if (priority) {
        a |= R01_OAM_PRIORITY;
    }
    if (size16) {
        a |= R01_OAM_SIZE;
    }
    return a;
}

#endif
