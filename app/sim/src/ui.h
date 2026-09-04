#ifndef retr01_SIM_UI_H
#define retr01_SIM_UI_H

#include "retr01_sim/entity.h"
#include "retr01_sim/gamepad.h"
#include "retr01_sim/health.h"
#include "retr01_sim/island_group.h"
#include "retr01_sim/types.h"

/* R01S_MAX_ISLANDS comes from island_group.h */

#include <SDL.h>
#include <stdint.h>

#define R01S_BOARD_MAX_CHIPS 64
#define R01S_UI_GAMEPAD_COUNT 2

/* Fixed HUD chrome -- board draws only in the center viewport. */
#define R01S_UI_UNIT 8
#define R01S_UI_HUD_TOP (R01S_UI_UNIT + 16)
#define R01S_UI_HUD_BOTTOM (R01S_UI_UNIT + 16)
#define R01S_UI_SIDEBAR_L 0
#define R01S_UI_SIDEBAR_R 0
#define R01S_UI_VIEW_X R01S_UI_SIDEBAR_L
#define R01S_UI_VIEW_Y R01S_UI_HUD_TOP
#define R01S_UI_VIEW_W (R01S_LOGIC_W - R01S_UI_SIDEBAR_L - R01S_UI_SIDEBAR_R)
#define R01S_UI_VIEW_H (R01S_LOGIC_H - R01S_UI_HUD_TOP - R01S_UI_HUD_BOTTOM)

#define R01S_UI_MODAL_NONE 0
#define R01S_UI_MODAL_QUIT 1

#define R01S_UI_MODAL_RES_NONE 0
#define R01S_UI_MODAL_RES_SAVE 1
#define R01S_UI_MODAL_RES_DISCARD 2
#define R01S_UI_MODAL_RES_CANCEL 3

/* Host input path: arcade microswitch bitfield vs Retr01-C ATtiny UART pads. */
#define R01S_INPUT_ARCADE 0
#define R01S_INPUT_PADS 1

#define R01S_UI_FLOAT_STRIP_H 20
#define R01S_UI_ISLANDS_STRIP_DEFAULT_X 8
#define R01S_UI_ISLANDS_STRIP_DEFAULT_Y 8
#define R01S_UI_LEGEND_STRIP_DEFAULT_X 8
#define R01S_UI_LEGEND_STRIP_DEFAULT_Y (R01S_UI_ISLANDS_STRIP_DEFAULT_Y + R01S_UI_FLOAT_STRIP_H + 4)
#define R01S_UI_LEGEND_STRIP_H (12 + 5 * 8 + 4 * 2)

typedef struct R01sUi {
    R01sIslandGroup *group;
    R01sEntity *chips[R01S_BOARD_MAX_CHIPS];
    uint8_t chip_island[R01S_BOARD_MAX_CHIPS];
    int chip_count;
    int selected; /* primary index or -1 (compat / status) */
    uint8_t chip_sel[R01S_BOARD_MAX_CHIPS]; /* compact multi-select */
    int box_sel; /* 1 = dragging marquee in compact mode */
    int box_bx0, box_by0, box_bx1, box_by1; /* board-space marquee */
    int sel_drag_ox, sel_drag_oy; /* board mouse at multi-drag start */
    int sel_start_x[R01S_BOARD_MAX_CHIPS];
    int sel_start_y[R01S_BOARD_MAX_CHIPS];
    int pan_x;
    int pan_y;
    int drag_pan; /* middle/right button pan */
    int drag_chip; /* chip index while left-dragging, else -1 */
    int drag_island; /* island index while moving frame, else -1 */
    int resize_island; /* island index while resizing, else -1 */
    int resize_corner; /* R01S_ISLAND_CORNER_* while resizing */
    /* Draw order back->front: island_z_order[0] is bottom, [count-1] is top. */
    uint8_t island_z_order[R01S_MAX_ISLANDS];
    int island_z_count;
    /* Compact-mode chip draw order (same convention as island_z_order). */
    uint8_t chip_z_order[R01S_BOARD_MAX_CHIPS];
    int chip_z_count;
    int drag_grab_bx;
    int drag_grab_by;
    int drag_last_x;
    int drag_last_y;
    char status[192];
    R01sSystemHealth health;
    int probe_vdd;
    int probe_phi2;
    int probe_resb_low;
    uint8_t probe_pad_p1;
    uint8_t probe_pad_p2;
    R01sGamepadInput gamepad[R01S_UI_GAMEPAD_COUNT];
    int input_mode; /* R01S_INPUT_ARCADE or R01S_INPUT_PADS */
    int mouse_lx; /* last logic-space mouse (for tooltips) */
    int mouse_ly;
    int tip_stable_mx; /* mouse position when hover timer last reset */
    int tip_stable_my;
    Uint32 tip_show_at; /* SDL tick before tooltip may appear */
    int islands_strip_x; /* viewport-relative origin */
    int islands_strip_y;
    int drag_islands_strip;
    int drag_islands_ox;
    int drag_islands_oy;
    int islands_strip_moved;
    int legend_strip_x; /* viewport-relative origin */
    int legend_strip_y;
    int drag_legend_strip;
    int drag_legend_ox;
    int drag_legend_oy;
    int legend_strip_moved;
    int modal;        /* R01S_UI_MODAL_* */
    int modal_result; /* R01S_UI_MODAL_RES_* when user picks an action */
    int fps;            /* rolling 1s frame rate for HUD */
    int sim_steps;      /* board steps taken in the last UI frame */
    int pins_quiet;     /* 1 = IC pin stubs static gray (no level colors) */
    int ctx_chip;       /* context-menu chip index, or -1 */
    int ctx_x;          /* menu anchor in logic space */
    int ctx_y;
    int layout_compact; /* 1 = pack chips like a PCB (no island frames) */
    /* Snapshot of island-mode geometry while compact (restored on toggle off). */
    int layout_saved;
    /* Island-mode chip positions relative to island board_x/board_y. */
    int save_chip_x[R01S_BOARD_MAX_CHIPS];
    int save_chip_y[R01S_BOARD_MAX_CHIPS];
    uint8_t save_chip_orient[R01S_BOARD_MAX_CHIPS]; /* R01sPkgOrient */
    int save_island_x[R01S_MAX_ISLANDS];
    int save_island_y[R01S_MAX_ISLANDS];
    int save_island_w[R01S_MAX_ISLANDS];
    int save_island_h[R01S_MAX_ISLANDS];
    /* Persisted compact-mode chip placements (by chip index). */
    int compact_saved;
    int compact_chip_x[R01S_BOARD_MAX_CHIPS];
    int compact_chip_y[R01S_BOARD_MAX_CHIPS];
    uint8_t compact_chip_orient[R01S_BOARD_MAX_CHIPS];
    /* One-shot undo for Ctrl+. compact sort (board pose before the sort). */
    int undo_pose_valid;
    int undo_chip_x[R01S_BOARD_MAX_CHIPS];
    int undo_chip_y[R01S_BOARD_MAX_CHIPS];
    uint8_t undo_chip_orient[R01S_BOARD_MAX_CHIPS];
    int layout_dirty; /* 1 = unsaved layout edits (SAVE / S to write ui_layout.json) */
    SDL_Texture *lcd_tex; /* 256x240 LCD framebuffer upload (streaming) */
} R01sUi;

int r01s_ui_init(R01sUi *ui);
void r01s_ui_shutdown(R01sUi *ui);

void r01s_ui_bind_group(R01sUi *ui, R01sIslandGroup *group);

void r01s_ui_island_z_init(R01sUi *ui);
void r01s_ui_island_z_apply(R01sUi *ui, const int *z_by_index, int n);
int r01s_ui_island_z_rank(const R01sUi *ui, int island_index);
void r01s_ui_island_z_raise(R01sUi *ui, int island_index);

void r01s_ui_chip_z_init(R01sUi *ui);
void r01s_ui_chip_z_apply(R01sUi *ui, const int *z_by_index, int n);
int r01s_ui_chip_z_rank(const R01sUi *ui, int chip_index);
void r01s_ui_chip_z_raise(R01sUi *ui, int chip_index);

int r01s_ui_add_chip(R01sUi *ui, R01sEntity *chip, int island_index);

void r01s_ui_clamp_pan(R01sUi *ui);
void r01s_ui_sync_gamepads(R01sUi *ui);
uint8_t r01s_ui_gamepad_port(const R01sUi *ui, int player);
void r01s_ui_draw(R01sUi *ui, SDL_Renderer *r);

/* Black boot screen during IC catchup. spin_frame advances on worker->main ticks. */
void r01s_ui_draw_boot(R01sUi *ui, SDL_Renderer *r, int spin_frame);
int r01s_ui_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y);

void r01s_ui_modal_open_quit(R01sUi *ui);
void r01s_ui_modal_cancel(R01sUi *ui);
int r01s_ui_modal_active(const R01sUi *ui);
int r01s_ui_modal_take_result(R01sUi *ui);
int r01s_ui_modal_handle_event(R01sUi *ui, const SDL_Event *e, int logic_x, int logic_y);

/* Rotate selected DIP (H<->V). Returns 1 if rotated. */
int r01s_ui_rotate_selected(R01sUi *ui);

/* Persist island + compact layouts to JSON (default path search / create). */
int r01s_ui_layout_load(R01sUi *ui);
int r01s_ui_layout_save(R01sUi *ui);

/* Copy live island frames + chip positions into save_* snapshots. */
void r01s_ui_snapshot_island_layout(R01sUi *ui);

/* Copy live island frame geometry only (compact mode -- chip snapshot stays island-relative). */
void r01s_ui_snapshot_island_frames(R01sUi *ui);

/* Apply saved island frames + island-relative chip positions (no compact/ heal side effects). */
void r01s_ui_apply_saved_island_layout(R01sUi *ui);

/* Apply saved island layout after JSON load (handles v1 absolute coords + v2 relative). */
void r01s_ui_load_island_layout(R01sUi *ui, int file_version);

/* v1 JSON stored absolute board coords -- convert to island-relative using live frames. */
void r01s_ui_layout_migrate_v1_chips(R01sUi *ui);

#endif
