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

#define R01_GRID_MAX 16
#define R01_DEFAULT_GRID 3
#define R01_MAX_SCREENS (R01_GRID_MAX * R01_GRID_MAX)
#define R01_MAX_PRESENT_SCREENS 64 /* cart cap: present BG1 screens per world (docs/general/memory.md) */
/* Virtual grid cell: col/row 0-15 packed as nibbles (cart dir + world spawn). */
#define R01_CELL_PACK(col, row) ((uint8_t)(((unsigned)(col)&0x0fu) | (((unsigned)(row)&0x0fu) << 4)))
#define R01_CELL_COL(b) ((int)((unsigned)(b)&0x0fu))
#define R01_CELL_ROW(b) ((int)(((unsigned)(b) >> 4) & 0x0fu))
#define R01_PARALLAX_MIN 0
#define R01_PARALLAX_MAX 16 /* BG0 present screens per world */
#define R01_PARALLAX_SLICE_MAX 120 /* max bands; variable thickness (docs/general/graphics) */
#define R01_START_COL 2
#define R01_START_ROW 0

#define R01_MAX_WORLDS 8
#define R01_BG_BANKS 16
#define R01_SPR_BANKS 16
#define R01_TILES_PER_BANK 256
#define R01_TILE_BYTES 16
#define R01_BANK_CHR_BYTES (R01_TILES_PER_BANK * R01_TILE_BYTES)
/* SPR bank 0 tile reserved for cart/Play player stub (solid color-1). */
#define R01_SPR_PLAYER_TILE_ID 1

#define R01_BG0_SCREENS_MAX 16

#define R01_MASTER_COLORS 64
#define R01_PAL_COLORS 4
#define R01_PALS_PER_ROW 4
#define R01_PAL_ROWS 8
#define R01_PAL_COUNT (R01_PAL_ROWS * R01_PALS_PER_ROW)
#define R01_PAL_PLANE_BYTES (R01_PAL_COUNT * R01_PAL_COLORS)

#define R01_CART_FLASH_BYTES (512u * 1024u)
#define R01_PRG_BYTES 32768u
#ifndef R01_CHR_BANK_BYTES
#define R01_CHR_BANK_BYTES 4096u
#endif
#define R01_CART_FORMAT_VER 5
#define R01_CART_HDR_BYTES 16u
#define R01_CART_PTR_TABLE_BYTES 42u /* 7 x (u24 off, u24 len) */
#define R01_CART_SCREEN_PAYLOAD 480u
#define R01_CART_OTHER_MAX 16
#define R01_CART_OTHER_TITLE 0
#define R01_CART_OTHER_INTER 1
#define R01_CART_OTHER_CREDITS_FIRST 2
#define R01_CART_CREDITS_MIN 0
/* Other pool is shared 16 (title+inter+credits); credits start at index 2. */
#define R01_CART_CREDITS_MAX (R01_CART_OTHER_MAX - R01_CART_OTHER_CREDITS_FIRST) /* 14 */
#define R01_CART_OTHER_HDR_BYTES 4u
#define R01_CART_OTHER_DIR_BYTES 8u
#define R01_CART_OTHER_FLAG_RLE 0x01u
#define R01_CART_OTHER_BYTES_MAX (64u * 1024u) /* soft export budget */
#define R01_CART_GLOBAL_CHR_BANKS (R01_BG_BANKS + R01_SPR_BANKS)
#define R01_CART_GLOBAL_CHR_BYTES ((uint32_t)R01_CART_GLOBAL_CHR_BANKS * R01_CHR_BANK_BYTES)

#define R01_NAME_MAX 64
#define R01_PATH_MAX 512
#define R01_JSON_VER 17

#define R01_ASEPRITE_ENTITIES_DIR "aseprite_entities"
#define R01_ASEPRITE_LISTING_MAX 64
#define R01_ASEPRITE_REL_MAX 96

#define R01_OUTPUT_DIR "output"
/* Empty: no default fixture paths. Export stem is project-relative when set. */
#define R01_DEFAULT_PROJECT ""
#define R01_DEFAULT_CART_STEM ""

/* Per-world sprite catalog (CHR patterns in spr_banks + authoring metadata). */
#define R01_MAX_SPRITES 256
/* Authoring leftovers in project JSON (not cart / C API). See docs/general/video-graphics.md. */
#define R01_MAX_METASPRITES 64
#define R01_MAX_METATILES 64

/* Entity types (docs/general/video-graphics.md). Soft on-screen live cap is 16. */
#define R01_MAX_ENTITY_TYPES 32
#define R01_ENTITY_STATES_MAX 4
#define R01_ENTITY_FRAMES_MAX 8
#define R01_ENTITY_PARTS_MAX 6 /* hard: no frame may exceed 6 sprites */
#define R01_ENTITY_ONSCREEN_MAX 16 /* live instances (OAM headroom allows 32 at 4 parts) */
#define R01_ENTITY_COMPOSE_PX 32 /* authoring canvas (px). Studio shows full grid at fixed scale */
#define R01_ENTITY_NAME_MAX 32
#define R01_ID_MAX 96
#define R01_ENTITY_HITBOX_W 8
#define R01_ENTITY_HITBOX_H 8
#define R01_MAX_ENTITY_INSTANCES 64 /* world placement table (may exceed live 16) */
#define R01_OAM_MAX 64

#define R01_MAX_WARP_ENTRANCES 32
#define R01_MAX_WARP_EXITS 32
#define R01_WARP_FADE_OUT 0x01u
#define R01_WARP_FADE_IN 0x02u
#define R01_WARP_FADE_WHITE 0x04u

/* Studio BGM editor (Audio tab). Host flatten uses R01_BGM_* from r01_bgm_host.h. */
#define R01_BGM_TRACKS_MAX 8
#define R01_BGM_REGIONS_MAX 128
#define R01_BGM_CH_COUNT 5
#define R01_BGM_INS_GUITAR 0
#define R01_BGM_INS_EGUITAR 1
#define R01_BGM_INS_PIANO 2
#define R01_BGM_INS_FLUTE 3
#define R01_BGM_INS_COUNT 4
#define R01_BGM_TOK_MAX 5
#define R01_BGM_NAME_MAX 24

typedef struct R01BgmRegion {
    int start; /* ticks */
    int len;   /* ticks, >= 1 */
    int midi;
    char tok[R01_BGM_TOK_MAX];
    int sharp; /* 1 = sostenido */
    int flat;  /* 1 = bemol */
} R01BgmRegion;

typedef struct R01BgmData {
    int present; /* 1 = authoring data saved/loaded (else empty tracks) */
    int track_count;
    char track_name[R01_BGM_TRACKS_MAX][R01_BGM_NAME_MAX];
    int note_solfa; /* 0 letter, 1 solfege (UI labels) */
    int ch_ins[R01_BGM_TRACKS_MAX][R01_BGM_CH_COUNT]; /* R01_BGM_INS_* per channel */
    int region_count[R01_BGM_TRACKS_MAX][R01_BGM_CH_COUNT];
    R01BgmRegion region[R01_BGM_TRACKS_MAX][R01_BGM_CH_COUNT][R01_BGM_REGIONS_MAX];
} R01BgmData;

/* BG / sprite attr (docs/general/video-graphics.md) */
#define R01_ATTR_BANK_MASK 0x0Fu
#define R01_ATTR_PAL_MASK 0x30u
#define R01_ATTR_PAL_SHIFT 4
#define R01_ATTR_FLIP_H 0x40u
#define R01_ATTR_FLIP_V 0x80u
#define R01_GLOBAL_SPR_BANK_BASE 0

static inline int r01_is_global_spr_bank(int bank) {
    (void)bank;
    return 0;
}
static inline int r01_global_spr_index(int bank) {
    return bank & 15;
}

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
    uint8_t solids[R01_TILES_PER_SCREEN]; /* 0/1 per cell */
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
    int tile_id; /* index in project spr_banks[bank] */
    int pal;     /* 0..3 within the active sprite palette row */
} R01SpriteDef;

/* One OAM-like part in an entity frame (dx/dy in compose-grid pixels). */
typedef struct R01EntityPart {
    int bank; /* 0..R01_SPR_BANKS-1 */
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
    int delay; /* display frames, min 1 (0 reads as 1) */
    int origin_x;
    int origin_y;
} R01EntityFrame;

/* Reusable multi-part sprite group (no origin/hitbox). */
typedef struct R01MetaspriteDef {
    char name[R01_ENTITY_NAME_MAX];
    R01EntityFrame frame;
} R01MetaspriteDef;

/* 2x2 BG tile group (TL, TR, BL, BR). */
typedef struct R01MetatileDef {
    char name[R01_ENTITY_NAME_MAX];
    uint8_t tile[4];
    uint8_t attr[4];
} R01MetatileDef;

typedef struct R01EntityState {
    char name[R01_ENTITY_NAME_MAX]; /* authoring label (idle, walk, ...) */
    R01EntityFrame frames[R01_ENTITY_FRAMES_MAX];
    int frame_count; /* 1..R01_ENTITY_FRAMES_MAX */
    int hitbox_x;
    int hitbox_y;
    int hitbox_w;
    int hitbox_h;
} R01EntityState;

typedef struct R01EntityType {
    int present;
    char name[R01_ENTITY_NAME_MAX]; /* authoring label (player, slime, ...) */
    R01EntityState states[R01_ENTITY_STATES_MAX];
    int state_count; /* 1..R01_ENTITY_STATES_MAX */
} R01EntityType;

/* Placed instance in world pixel space (world_x/y = user frame origin). */
typedef struct R01EntityInstance {
    int type_id;
    int world_x;
    int world_y;
    int flip_h; /* 1 = mirror parts around frame origin at draw/OAM time */
    int flip_v;
} R01EntityInstance;

/* Intra-world warp: entrance tile triggers a jump to an exit destination tile. */
typedef struct R01WarpEntrance {
    int present;
    char id[R01_ID_MAX]; /* autogen: w_00, w_01, ... */
    int screen_col;
    int screen_row;
    int tile_col;
    int tile_row;
} R01WarpEntrance;

typedef struct R01WarpExit {
    int present;
    int entrance_idx; /* index into warp_entrances[] */
    int dest_screen_col;
    int dest_screen_row;
    int dest_tile_col;
    int dest_tile_row;
    uint8_t flags; /* R01_WARP_FADE_* */
} R01WarpExit;

/* Global off-grid MAP payloads (title, interstitial, credits pages). See docs/general/graphics. */
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
    /* Structured BG0 plane: up to 16 present screens anywhere on the 16x16 map. */
    int bg0_cols; /* present enclosing extent W (derived) */
    int bg0_rows; /* present enclosing extent H (derived) */
    R01Screen bg0_screens[R01_BG0_SCREENS_MAX];
    int bg0_screen_count;  /* slots used in bg0_screens[] (0..16, may include holes) */
    int bg0_active_screen; /* index into bg0_screens[]; -1 none */
    R01EntityInstance instances[R01_MAX_ENTITY_INSTANCES];
    int instance_count;
    R01WarpEntrance warp_entrances[R01_MAX_WARP_ENTRANCES];
    int warp_entrance_count;
    R01WarpExit warp_exits[R01_MAX_WARP_EXITS];
    int warp_exit_count;
} R01World;

typedef struct R01Project {
    char name[R01_NAME_MAX];
    int default_world; /* Play entry world (begin_play); cart export always uses worlds[0] */
    int active_world;  /* 0..R01_MAX_WORLDS-1 */
    int active_screen; /* index into worlds[active_world].screens */
    /* 8 rows x 4 pals each (docs/general/graphics). Index [row][pal]. */
    R01PalRow global_pal_bg[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01PalRow global_pal_spr[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01BgBank bg_banks[R01_BG_BANKS];
    R01SprBank spr_banks[R01_SPR_BANKS];
    R01SpriteDef sprites[R01_MAX_SPRITES];
    int sprite_count;
    R01MetaspriteDef metasprites[R01_MAX_METASPRITES];
    int metasprite_count;
    R01MetatileDef metatiles[R01_MAX_METATILES];
    int metatile_count;
    R01EntityType entities[R01_MAX_ENTITY_TYPES];
    int entity_count;
    int player_entity; /* type index marked as Play player; -1 = stub tile */
    R01OtherScreen other_screens[R01_CART_OTHER_MAX]; /* [0]=title [1]=inter [2+]=credits */
    R01World worlds[R01_MAX_WORLDS];
    R01BgmData bgm;
    /* Last scanned aseprite_entities/ relative paths (sibling of the .r01proj). */
    char aseprite_entities_files[R01_ASEPRITE_LISTING_MAX][R01_ASEPRITE_REL_MAX];
    int aseprite_entities_file_count;
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
    uint8_t a = (uint8_t)((bank & 15) | ((pal & 3) << R01_ATTR_PAL_SHIFT));
    if (flip_h) {
        a |= R01_ATTR_FLIP_H;
    }
    if (flip_v) {
        a |= R01_ATTR_FLIP_V;
    }
    return a;
}

static inline uint8_t r01_attr_merge(uint8_t old, int bank, int pal, int flip_h, int flip_v) {
    (void)old;
    return r01_attr_pack(bank, pal, flip_h, flip_v);
}

#endif
