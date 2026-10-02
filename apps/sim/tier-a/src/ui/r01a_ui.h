#ifndef R01A_TIER_A_UI_H
#define R01A_TIER_A_UI_H

#include "r01a_board.h"

#include "discrete_ic/entity.h"
#include "discrete_ic/passive.h"
#include "discrete_ic/types.h"
#include "discrete_ic/video_sink.h"

#include <SDL.h>
#include <stddef.h>
#include <stdint.h>

#ifndef R01A_LAYOUT_FILE
#define R01A_LAYOUT_FILE "ui_layout.json"
#endif

#define R01A_UI_SCALE 2
#define R01A_SIM_BUDGET_MS 8
#define R01A_SIM_MAX_STEPS_PER_FRAME 24
#define R01A_DOTS_PER_STEP 32
#define R01A_BOARD_MAX_CHIPS 64
#define R01A_PIN_NET_SLOTS 512
#define R01A_TRACE_MAX 256
#define R01A_TRACE_PTS 32
#define R01A_GRID 3
#define R01A_PAD 3
#define R01A_PAD_GAP 3
#define R01A_PAD_PITCH (R01A_PAD + R01A_PAD_GAP)
#define R01A_HDR_PAD 3 /* plastic around each 3×3 header pin */
#define R01A_HDR_CELL (R01A_PAD + 2 * R01A_HDR_PAD)
#define R01A_ZOOM_MAX 8
#define R01A_HOP_BYPASS_MM 15
#define R01A_HOP_FAST_MM 35
#define R01A_SEL_R 255
#define R01A_SEL_G 220
#define R01A_SEL_B 80
#define R01A_HIST_MAX 64
#define R01A_HIST_REF 16

typedef struct R01aTrace {
    int n;
    int16_t x[R01A_TRACE_PTS];
    int16_t y[R01A_TRACE_PTS];
} R01aTrace;

typedef struct R01aHistPart {
    char ref[R01A_HIST_REF];
    int x;
    int y;
    int px;
    int py;
    int orient;
    int scale_2x;
} R01aHistPart;

typedef struct R01aHistSnap {
    int part_n;
    R01aHistPart parts[R01A_BOARD_MAX_CHIPS];
    int trace_n;
    R01aTrace traces[R01A_TRACE_MAX];
} R01aHistSnap;

typedef struct R01aHistory {
    R01aHistSnap snap[R01A_HIST_MAX];
    int n;
    int cur;
} R01aHistory;

typedef struct R01aUi {
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *target;
    SDL_Texture *lcd_tex;
    int scale;
    int zoom;
    int pan_x;
    int pan_y;
    int mouse_lx;
    int mouse_ly;
    int drag_pan;
    int drag_chip;
    int drag_grab_bx;
    int drag_grab_by;
    int drag_from_x;
    int drag_from_y;
    int selected;
    int hover_chip;
    int hover_pin;
    int chip_count;
    NsEntity *chips[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_z[R01A_BOARD_MAX_CHIPS];
    int trace_n;
    R01aTrace traces[R01A_TRACE_MAX];
    int arm;
    int arm_x[R01A_TRACE_PTS];
    int arm_y[R01A_TRACE_PTS];
    int arm_n;
    int dest_chip;
    int dest_pin;
    int show_nets;
    int air_hard;
    R01aHistory hist;
} R01aUi;

int div_floor(int a, int b);
int canvas_zoom(const R01aUi *ui);
int board_sx(const R01aUi *ui, int bx);
int board_sy(const R01aUi *ui, int by);
void logic_to_board(const R01aUi *ui, int lx, int ly, int *bx, int *by);
void canvas_zoom_by(R01aUi *ui, int delta, int lx, int ly);
void pan_lock_point(R01aUi *ui, int gx, int gy, int lx, int ly);

void bind_chips(R01aUi *ui, R01aBoard *board);
int snap_grid(int v);
int snap_pin(int v);
void move_entity(NsEntity *e, int bx, int by);
void snap_placed_chips(R01aUi *ui);
int is_passive_glyph(const NsEntity *e);
int is_axial_passive(const NsEntity *e);
int ic_body_w(const NsEntity *e);
int ic_body_h(const NsEntity *e);
int glyph_w(const NsEntity *e);
int glyph_h(const NsEntity *e);
int pin_center(const NsEntity *e, int pin_index, int *cx, int *cy);
int hit_pin_at(const R01aUi *ui, int bx, int by, int *chip_out, int *pin_out);
int hit_top_chip(const R01aUi *ui, int bx, int by);
NsEntity *ui_ent(const R01aUi *ui, const char *ref);
void snap_octant(int x0, int y0, int x1, int y1, int *ox, int *oy);

void pin_net_build(R01aUi *ui);
void copper_rebuild(const R01aUi *ui);
int screen_picture_live(const R01aUi *ui);
void nearest_open_partner(const R01aUi *ui, int chip, int pin, int *oc, int *op);
void arm_begin(R01aUi *ui, int chip, int pin);
void arm_cancel(R01aUi *ui);
void arm_commit(R01aUi *ui);
void arm_add_point(R01aUi *ui, int x, int y);
void arm_add_to_pad(R01aUi *ui, int px, int py);
void draw_air_wires(SDL_Renderer *r, const R01aUi *ui);
void draw_traces(SDL_Renderer *r, const R01aUi *ui);
void draw_arm(SDL_Renderer *r, const R01aUi *ui, int mx, int my);

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb);
void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb);
void plot_a(SDL_Renderer *r, int x, int y, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);
void draw_soft_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);
void draw_hard_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb);
void draw_ants_line(SDL_Renderer *r, int x0, int y0, int x1, int y1, Uint32 now);
void mode_btn_rect(const R01aUi *ui, SDL_Rect *rc);
void draw_mode_btn(SDL_Renderer *r, const R01aUi *ui);
void draw_legend(SDL_Renderer *r);
void fill_tooltip(const R01aUi *ui, char *out, size_t out_len);
void draw_tooltip(SDL_Renderer *r, int lx, int ly, const char *text);
void draw_lcd(SDL_Renderer *r, R01aUi *ui, NsVideoSink *sink, int selected);
void draw_ic(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected, int hover_pin);
void draw_passive_glyph(SDL_Renderer *r, const R01aUi *ui, NsEntity *e, int selected, int hover_pin);
void update_scale(R01aUi *ui);
void logic_from_window(const R01aUi *ui, int wx, int wy, int *lx, int *ly);
void present_frame(R01aUi *ui);
void draw_frame(R01aUi *ui, R01aBoard *board);

void hist_init(R01aUi *ui);
void hist_after(R01aUi *ui);
int hist_undo(R01aUi *ui);
int hist_redo(R01aUi *ui);

void rotate_selected(R01aUi *ui);
int handle_event(R01aUi *ui, R01aBoard *board, const SDL_Event *e, int lx, int ly);

#endif
