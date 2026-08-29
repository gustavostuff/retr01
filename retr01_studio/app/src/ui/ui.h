#ifndef retr01_STUDIO_UI_H
#define retr01_STUDIO_UI_H

#include "retr01_studio/metasprites.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <SDL.h>

#define UI_LOGIC_W 640
#define UI_LOGIC_H 360

/* Phase 2 chrome: dark / darker gray, 8px grid. */
#define UI_COL_BG_R 34
#define UI_COL_BG_G 34
#define UI_COL_BG_B 38
#define UI_COL_PANEL_R 26
#define UI_COL_PANEL_G 26
#define UI_COL_PANEL_B 30
#define UI_COL_WELL_R 63
#define UI_COL_WELL_G 63
#define UI_COL_WELL_B 74
#define UI_COL_ACTIVE_R 45
#define UI_COL_ACTIVE_G 125
#define UI_COL_ACTIVE_B 70
#define UI_COL_PRESENT_R 55
#define UI_COL_PRESENT_G 130
#define UI_COL_PRESENT_B 220
#define UI_COL_MARK_R 245
#define UI_COL_MARK_G 245
#define UI_COL_MARK_B 245
#define UI_COL_CHESS_A_R 58
#define UI_COL_CHESS_A_G 58
#define UI_COL_CHESS_A_B 66
#define UI_COL_CHESS_B_R 50
#define UI_COL_CHESS_B_G 50
#define UI_COL_CHESS_B_B 58

#define UI_UNIT 8
#define UI_BTN_H 16
#define UI_SIDEBAR_W 128
#define UI_WORLD_BTN 16
#define UI_WORLD_CELL 16
#define UI_WORLD_VIEW 128
#define UI_PAL_SWATCH 8

#define UI_WORLDS_X 0
#define UI_WORLDS_BODY_H (UI_WORLD_BTN + UI_WORLD_VIEW)
#define UI_PAL_BODY_H (UI_PAL_SWATCH * 2 + UI_BTN_H)

#define UI_ACC_NONE (-1)
#define UI_ACC_WORLDS 0
#define UI_ACC_PALS 1
#define UI_ACC_SPRITES 2
#define UI_ACC_METASPRITES 3
#define UI_ACC_ENTITIES 4

#define UI_SPRITES_BODY_H 96
#define UI_SPRITE_ROW_H 16
#define UI_SPRITE_ICON 8
#define UI_PREVIEW_ICON 16 /* metasprite / entity sidebar + modal list thumbs */
#define UI_METASPRITES_BODY_H 96
#define UI_ENTITIES_BODY_H 96

#define UI_ENTITY_MODAL_W 448
#define UI_ENTITY_MODAL_H 352
#define UI_ENTITY_BANK_GRID 128 /* 16x16 tiles @ 8px */
#define UI_ENTITY_COMPOSE 128   /* 16px @ 8x scale */
#define UI_ENTITY_LIST_H 128
#define UI_DOT_SIZE 8
#define UI_DOT_GAP 4
#define UI_DOT_STRIP_N 4

#define UI_CATALOG_DRAG_SPRITE 1
#define UI_CATALOG_DRAG_METASPRITE 2
#define UI_CATALOG_DRAG_ENTITY 3

#define UI_MAIN_W (UI_LOGIC_W - UI_SIDEBAR_W)
#define UI_SCREEN_SCALE 2
#define UI_SCREEN_W (R01_SCREEN_PX_W * UI_SCREEN_SCALE)
#define UI_SCREEN_H (R01_SCREEN_PX_H * UI_SCREEN_SCALE)

#define UI_MODE_ROW_H UI_BTN_H
#define UI_MODE_RADIO 8
#define UI_MODE_GAP 4
#define UI_CHECKBOX 8

#define UI_SCREEN_MODE_SEL 0
#define UI_SCREEN_MODE_PAINT 1

#define UI_SCREEN_LAYER_BG 0
#define UI_SCREEN_LAYER_SPR 1

#define UI_MODAL_W 288
#define UI_MODAL_H 160
#define UI_TILE_CANVAS 128
#define UI_MODAL_BODY_Y (UI_BTN_H + UI_UNIT)

#define UI_PAL_MODAL_W 320
#define UI_PAL_MODAL_H 184
#define UI_MASTER_COLS 16
#define UI_MASTER_ROWS 4
#define UI_MASTER_CELL 8
#define UI_PAL_EDIT_CELL 10

#define UI_TOAST_MS 2800

#define UI_MENU_MAX 16
#define UI_MENU_KIND_TILE 1
#define UI_MENU_KIND_WORLD 2
#define UI_MENU_KIND_SPRITE 3
#define UI_MENU_KIND_METASPRITE 4
#define UI_MENU_KIND_ENTITY 5
#define UI_MENU_KIND_INSTANCE 6
#define UI_MENU_SUB_NONE 0
#define UI_MENU_SUB_BANK 1
#define UI_MENU_SUB_PAL 2
#define UI_MENU_SUB_SPR_BANK 3
#define UI_MENU_SUB_SPR_PAL 4
#define UI_MENU_SUB_WARP 5

typedef struct UiMenu {
    int open;
    int kind;
    int root_x, root_y;
    int root_w;
    int item_count;
    char items[UI_MENU_MAX][32];
    uint8_t item_sub[UI_MENU_MAX]; /* UI_MENU_SUB_* or 0 */
    uint8_t item_disabled[UI_MENU_MAX];

    int submenu;
    int sub_x, sub_y;
    int sub_w;
    int sub_count;
    char sub_items[UI_MENU_MAX][24];

    int screen_tx, screen_ty;
    int world_screen_idx;
    int sprite_catalog_idx;
    int metasprite_idx;
    int entity_type_idx;
    int instance_idx; /* UI_MENU_KIND_INSTANCE */
} UiMenu;

typedef struct UiTileEdit {
    int open;
    int pal;   /* 0..3 within active pal row */
    int color; /* 0..3 within pal */
    int tile_id;
    int bank;
    int flip_h;
    int flip_v;
    int is_new;
    uint8_t chr[R01_TILE_BYTES];
    int paint_tx, paint_ty; /* screen tile that opened the editor (-1 if none) */
} UiTileEdit;

typedef struct UiPalEdit {
    int open;
    int row;   /* 0..7 global palette row (BG + SPR) */
    int plane; /* 0 = BG, 1 = SPR */
    int pal;   /* 0..3 */
    int color; /* 0..3 */
    int snap_valid;
    int snap_default_row;
    R01PalRow snap_bg[R01_PAL_ROWS][R01_PALS_PER_ROW];
    R01PalRow snap_spr[R01_PAL_ROWS][R01_PALS_PER_ROW];
} UiPalEdit;

typedef struct UiSpriteEdit {
    int open;
    int is_new;
    int catalog_idx; /* -1 when creating */
    int bank;
    int tile_id;
    int pal;   /* 0..3 */
    int color; /* 0..3 paint color */
    int flip_h;
    int flip_v;
    uint8_t chr[R01_TILE_BYTES];
} UiSpriteEdit;

typedef struct UiMetaspriteEdit {
    int open;
    int is_new;
    int meta_idx; /* -1 when creating */
    R01MetaspriteDef draft;
    int bank;
    int sel_part;
    int paint_color;
    int paint_pal;
    int dragging; /* 0 none, 1 part, 4 bank tile ghost, 5 paint stroke */
    int drag_tile;
    int drag_off_x;
    int drag_off_y;
} UiMetaspriteEdit;

typedef struct UiEntityEdit {
    int open;
    int is_new;
    int type_idx; /* -1 when creating; commit on save */
    R01EntityType draft;
    int state;      /* 0..R01_ENTITY_STATES_MAX-1 */
    int frame;      /* 0..R01_ENTITY_FRAMES_MAX-1 */
    int sel_part;   /* -1 or index in current frame */
    int paint_color;
    int paint_pal;
    int show_guides; /* origin cross + hitbox */
    int meta_scroll; /* left metasprite list */
    int dragging;    /* 0 none, 1 part, 2 hitbox, 3 origin, 6 metasprite ghost, 5 paint */
    int drag_meta;   /* metasprite catalog idx when dragging */
    int drag_off_x;
    int drag_off_y;
} UiEntityEdit;

typedef struct UiBrush {
    int armed; /* after Save in tile editor */
    int bank;
    int tile_id;
    int pal;
    int flip_h;
    int flip_v;
    uint8_t chr[R01_TILE_BYTES];
} UiBrush;

typedef struct UiCatalogDrag {
    int active; /* 0 none, UI_CATALOG_DRAG_* */
    int index;
    int off_x;
    int off_y;
} UiCatalogDrag;

/* Single active text field (web-like caret / selection / scroll). */
typedef struct UiTextEdit {
    char *buf;
    int cap;
    int field_id; /* 0 = none; modal-specific otherwise */
    int caret;
    int anchor; /* selection other end; equals caret when collapsed */
    int scroll; /* horizontal px */
    int drag;   /* mouse-drag selecting */
} UiTextEdit;

typedef struct UiState {
    R01Project *project;
    R01PlayState play;
    char project_path[R01_PATH_MAX];
    char toast[96];
    Uint32 toast_until;
    int toast_error;
    char tooltip[160];
    int tooltip_x;
    int tooltip_y;
    int tooltip_active;
    int scale;
    Uint32 play_last_tick;
    Uint8 keys[512];
    int mouse_x;
    int mouse_y;
    UiMenu menu;
    UiTileEdit tile_edit;
    UiPalEdit pal_edit;
    UiSpriteEdit sprite_edit;
    UiMetaspriteEdit metasprite_edit;
    UiEntityEdit entity_edit;
    UiTextEdit text;
    UiBrush brush;
    UiCatalogDrag catalog_drag;
    int paint_stamp_valid;
    uint8_t paint_stamp_tile;
    uint8_t paint_stamp_attr;
    int screen_mode;  /* UI_SCREEN_MODE_SEL or UI_SCREEN_MODE_PAINT */
    int screen_layer; /* UI_SCREEN_LAYER_BG or UI_SCREEN_LAYER_SPR */
    int sel_x0, sel_y0, sel_x1, sel_y1; /* inclusive tile rect; invalid when sel_x0 < 0 */
    int sel_anchor_x, sel_anchor_y;
    int sel_drag;
    int inst_drag;
    int inst_drag_off_x;
    int inst_drag_off_y;
    int sel_instance; /* -1 or index into world.instances */
    int last_paint_tx;
    int last_paint_ty;
    Uint32 last_click_ms;
    int last_click_col;
    int last_click_row;
    int accordion_open; /* UI_ACC_* or UI_ACC_NONE */
    int sprites_scroll;
    int metasprites_scroll;
    int entities_scroll;
} UiState;

int ui_init(UiState *ui);
void ui_shutdown(UiState *ui);
void ui_tick(UiState *ui);
void ui_draw(UiState *ui, SDL_Renderer *r);
int ui_handle_event(UiState *ui, const SDL_Event *e, int lx, int ly);
int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly);

#endif
