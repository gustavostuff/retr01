#ifndef retr01_STUDIO_TYPES_H
#define retr01_STUDIO_TYPES_H

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
#define R01_MAX_PRESENT_SCREENS 32 /* cart cap (docs/02): 8 worlds x 32 + 32 KiB PRG in 512 KiB */
#define R01_PARALLAX_MIN 0
#define R01_PARALLAX_MAX 8 /* per world; live VRAM slots 4-5 only */
#define R01_PARALLAX_SLICE_MAX 120 /* max bands; variable thickness (docs/02) */
#define R01_START_COL 2
#define R01_START_ROW 0

#define R01_MAX_WORLDS 8
#define R01_BG_BANKS 4
#define R01_SPR_BANKS 4
#define R01_TILES_PER_BANK 256
#define R01_TILE_BYTES 16
#define R01_BANK_CHR_BYTES (R01_TILES_PER_BANK * R01_TILE_BYTES)
/* SPR bank 0 tile reserved for cart/Play player stub (solid color-1). */
#define R01_SPR_PLAYER_TILE_ID 1

#define R01_MASTER_COLORS 64
#define R01_PAL_COLORS 4
#define R01_PALS_PER_ROW 4
#define R01_PAL_ROWS 8
#define R01_PAL_COUNT (R01_PAL_ROWS * R01_PALS_PER_ROW)
#define R01_PAL_PLANE_BYTES (R01_PAL_COUNT * R01_PAL_COLORS)

#define R01_CART_FLASH_BYTES (512u * 1024u)
#define R01_PRG_BYTES 32768u
#define R01_CHR_BANK_BYTES 4096u
#define R01_CART_FORMAT_VER 2
#define R01_CART_HDR_BYTES 16u
#define R01_CART_PTR_TABLE_BYTES 36u
#define R01_CART_SCREEN_PAYLOAD 480u
#define R01_CART_OTHER_MAX 48
#define R01_CART_OTHER_TITLE 0
#define R01_CART_OTHER_INTER 1
#define R01_CART_OTHER_CREDITS_FIRST 2
#define R01_CART_CREDITS_MIN 0
#define R01_CART_CREDITS_MAX (R01_CART_OTHER_MAX - R01_CART_OTHER_CREDITS_FIRST) /* 46 */
#define R01_CART_OTHER_HDR_BYTES 4u
#define R01_CART_OTHER_DIR_BYTES 8u
#define R01_CART_OTHER_FLAG_RLE 0x01u
#define R01_CART_OTHER_BYTES_MAX (64u * 1024u) /* soft export budget */

#define R01_NAME_MAX 64
#define R01_PATH_MAX 512
#define R01_JSON_VER 7

#define R01_ROM_DIR "rom"
#define R01_DEFAULT_PROJECT R01_ROM_DIR "/test.r01proj"
#define R01_DEFAULT_CART_STEM R01_ROM_DIR "/test"

/* Per-world sprite catalog (CHR patterns in spr_banks + authoring metadata). */
#define R01_MAX_SPRITES 256
#define R01_MAX_METASPRITES 64

/* Entity types (docs/07). UI may lock state_count to 1; arrays sized for later. */
#define R01_MAX_ENTITY_TYPES 64
#define R01_ENTITY_STATES_MAX 4
#define R01_ENTITY_FRAMES_MAX 4
#define R01_ENTITY_PARTS_MAX 4 /* 2x2 tile frame budget */
#define R01_ENTITY_COMPOSE_PX 16
#define R01_ENTITY_NAME_MAX 32
#define R01_ENTITY_HITBOX_W 8
#define R01_ENTITY_HITBOX_H 8
#define R01_MAX_ENTITY_INSTANCES 64
#define R01_OAM_MAX 64

/* BG attr (docs/02) */
#define R01_ATTR_BANK_MASK 0x03u
#define R01_ATTR_PAL_MASK 0x0Cu
#define R01_ATTR_PAL_SHIFT 2
#define R01_ATTR_FLIP_H 0x10u
#define R01_ATTR_FLIP_V 0x20u
#define R01_ATTR_SOLID 0x40u
#define R01_ATTR_ANIM 0x80u

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

/* Catalog entry: one 8x8 pattern in a SPR bank + default palette. */
typedef struct R01SpriteDef {
    int bank;    /* 0..R01_SPR_BANKS-1 */
    int tile_id; /* index in spr_banks[bank] */
    int pal;     /* 0..3 within the active sprite palette row */
} R01SpriteDef;

/* One OAM-like part in an entity frame (dx/dy relative to state origin). */
typedef struct R01EntityPart {
    int bank;
    int tile_id;
    int pal;
    int flip_h;
    int flip_v;
    int dx;
    int dy;
} R01EntityPart;

typedef struct R01EntityFrame {
    R01EntityPart parts[R01_ENTITY_PARTS_MAX];
    int part_count;
} R01EntityFrame;

/* Reusable multi-part sprite group (no origin/hitbox). */
typedef struct R01MetaspriteDef {
    char name[R01_ENTITY_NAME_MAX];
    R01EntityFrame frame;
} R01MetaspriteDef;

typedef struct R01EntityState {
    char name[R01_ENTITY_NAME_MAX]; /* project-only authoring label */
    int origin_x;
    int origin_y;
    int hitbox_x;
    int hitbox_y;
    int hitbox_w; /* fixed 8 for now */
    int hitbox_h;
    R01EntityFrame frames[R01_ENTITY_FRAMES_MAX];
    int frame_count; /* 1..R01_ENTITY_FRAMES_MAX */
} R01EntityState;

typedef struct R01EntityType {
    int present;
    R01EntityState states[R01_ENTITY_STATES_MAX];
    int state_count; /* UI may lock to 1; wire up to R01_ENTITY_STATES_MAX */
} R01EntityType;

/* Placed instance in world pixel space (world_x/y = user state origin). */
typedef struct R01EntityInstance {
    int type_id;
    int world_x;
    int world_y;
} R01EntityInstance;

/* Global off-grid MAP payloads (title, interstitial, credits pages). See docs/02. */
typedef struct R01OtherScreen {
    int present; /* 0 = omit from cart; title/inter always present after init */
    uint8_t tiles[R01_TILES_PER_SCREEN];
    uint8_t attrs[R01_ATTRS_PER_SCREEN];
} R01OtherScreen;

typedef struct R01World {
    int present;
    int grid_cols;
    int grid_rows;
    int default_bg_bank;
    int default_pal_row;
    int default_screen; /* index into screens[]; spawn / play start */
    R01Screen screens[R01_MAX_SCREENS];
    int screen_count;
    R01BgBank bg_banks[R01_BG_BANKS];
    R01SprBank spr_banks[R01_SPR_BANKS];
    R01SpriteDef sprites[R01_MAX_SPRITES];
    int sprite_count;
    R01MetaspriteDef metasprites[R01_MAX_METASPRITES];
    int metasprite_count;
    R01EntityType entities[R01_MAX_ENTITY_TYPES];
    int entity_count;
    R01EntityInstance instances[R01_MAX_ENTITY_INSTANCES];
    int instance_count;
} R01World;

typedef struct R01Project {
    char name[R01_NAME_MAX];
    int default_world; /* Play entry world (begin_play); cart export always uses worlds[0] */
    int active_world;  /* 0..R01_MAX_WORLDS-1 */
    int active_screen; /* index into worlds[active_world].screens */
    /* 8 rows x 4 pals each (docs/02). Index [row][pal]. */
    R01PalRow global_pal_bg[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01PalRow global_pal_spr[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01OtherScreen other_screens[R01_CART_OTHER_MAX]; /* [0]=title [1]=inter [2+]=credits */
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

static inline int r01_attr_anim(uint8_t a) {
    return (a & R01_ATTR_ANIM) != 0;
}
static inline int r01_attr_solid(uint8_t a) {
    return (a & R01_ATTR_SOLID) != 0;
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

static inline uint8_t r01_attr_merge(uint8_t old, int bank, int pal, int flip_h, int flip_v) {
    return (uint8_t)((old & (R01_ATTR_SOLID | R01_ATTR_ANIM)) |
                     r01_attr_pack(bank, pal, flip_h, flip_v));
}

#endif
