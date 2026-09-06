#ifndef retr01_SIM_UI_INTERNAL_H
#define retr01_SIM_UI_INTERNAL_H

#include "ui.h"
#include "video_sink.h"

#include <SDL.h>
#include <stdint.h>

typedef struct R01sBoard R01sBoard;

#ifndef R01S_ASSETS_DIR
#define R01S_ASSETS_DIR "app/assets/other"
#endif
#ifndef R01S_ASSETS_PNG_DIR
#define R01S_ASSETS_PNG_DIR "app/assets/png"
#endif

#define R01S_UI_FONT_PX 16

/* Bounce/scroll for island titles and IC part labels that overflow their clip. */
#define R01S_UI_LABEL_PAUSE_MS 700
#define R01S_UI_LABEL_SCROLL_PX_PER_SEC 28

#define R01S_UI_CHIP_CART_R 13
#define R01S_UI_CHIP_CART_G 29
#define R01S_UI_CHIP_CART_B 57
#define R01S_UI_CHIP_ATTINY_R 57
#define R01S_UI_CHIP_ATTINY_G 13
#define R01S_UI_CHIP_ATTINY_B 25
#define R01S_UI_PIN_GRAY_R 179
#define R01S_UI_PIN_GRAY_G 179
#define R01S_UI_PIN_GRAY_B 204

#define R01S_UI_STATUS_ROW_H 16
#define R01S_UI_TOOLTIP_DELAY_MS 400

static inline int ui_board_sx(const R01sUi *ui, int board_x) {
    return R01S_UI_VIEW_X + board_x - ui->pan_x;
}

static inline int ui_board_sy(const R01sUi *ui, int board_y) {
    return R01S_UI_VIEW_Y + board_y - ui->pan_y;
}

static inline int ui_logic_in_view(int lx, int ly) {
    return lx >= R01S_UI_VIEW_X && lx < R01S_UI_VIEW_X + R01S_UI_VIEW_W && ly >= R01S_UI_VIEW_Y &&
           ly < R01S_UI_VIEW_Y + R01S_UI_VIEW_H;
}

static inline void ui_logic_to_board(const R01sUi *ui, int lx, int ly, int *bx, int *by) {
    *bx = lx - R01S_UI_VIEW_X + ui->pan_x;
    *by = ly - R01S_UI_VIEW_Y + ui->pan_y;
}

/* ui_font.c */
int font_ensure(void);
void font_shutdown(void);
int font_line_h(void);
void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B);
void fill_rect_a(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B, Uint8 A);
void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B);
void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B);
void font_draw_a(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B, Uint8 A);
int font_draw_ellipsize(SDL_Renderer *r, int x, int y, const char *text, int max_w, Uint8 R, Uint8 G, Uint8 B);
int font_text_width(const char *text);
void font_draw_a_rot90ccw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B, Uint8 A);

/* Pause-scroll-pause bounce offset (px). phase_seed desyncs multiple labels. */
int ui_label_bounce_scroll(int text_w, int view_w, unsigned phase_seed);
/* Horizontal label clipped to view_w x view_h at (x,y). Centers when it fits. */
void ui_draw_label_bounce(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
                          unsigned phase_seed, Uint8 R, Uint8 G, Uint8 B, Uint8 A);
/* Vertical (rot90 CCW) label clipped to view_w x view_h at (x,y). Centers when it fits. */
void ui_draw_label_bounce_rot90ccw(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
                                   unsigned phase_seed, Uint8 R, Uint8 G, Uint8 B, Uint8 A);

/* ui_draw_chip.c */
void pin_level_rgb(R01sLevel lvl, R01sPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb);
void draw_segment_btn(SDL_Renderer *r, const SDL_Rect *rc, int selected, const char *label);
void draw_led(SDL_Renderer *r, int x, int y, int on, Uint8 R, Uint8 G, Uint8 B, const char *label);
void draw_board_item(SDL_Renderer *r, R01sUi *ui, const R01sEntity *e, int selected);
void draw_video_pixels(SDL_Renderer *r, R01sUi *ui, R01sVideoSink *sink, int px, int py, int dw, int dh);

/* ui_placement.c */
void clamp_chip(R01sUi *ui, R01sEntity *e, int island_index);
void ui_expand_island_to_chips(R01sUi *ui, int island_index);
void move_chip_drag(R01sUi *ui, int chip_i, int board_mx, int board_my);
int ui_sel_count(const R01sUi *ui);
void ui_sel_clear(R01sUi *ui);
void ui_sel_set_one(R01sUi *ui, int chip_i);
void ui_sel_toggle(R01sUi *ui, int chip_i);
void ui_sel_from_box(R01sUi *ui, int additive);
void ui_begin_sel_drag(R01sUi *ui, int board_mx, int board_my);
void move_selection_drag(R01sUi *ui, int board_mx, int board_my);
void move_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my);
void resize_island_drag(R01sUi *ui, int island_index, int board_mx, int board_my);
void ui_toggle_compact(R01sUi *ui);
void ui_apply_compact_layout(R01sUi *ui);
int ui_sort_compact_by_type(R01sUi *ui);
int ui_undo_compact_pose(R01sUi *ui);
void compact_btn_rect(const R01sUi *ui, SDL_Rect *rc);
void save_btn_rect(const R01sUi *ui, SDL_Rect *rc);
void input_mode_btn_rect(const R01sUi *ui, SDL_Rect *rc);
void ui_save_layout_now(R01sUi *ui);
int ui_lcd_scale_2x(const R01sUi *ui);
int ui_screen_render_mode(const R01sUi *ui);
void ui_set_lcd_scale(R01sUi *ui, int scale_2x);
void ui_set_screen_render_mode(R01sUi *ui, int mode);
void ui_toggle_lcd_scale(R01sUi *ui);
int ui_chip_is_cart_flash(const R01sEntity *e);
int ui_chip_is_cart_eeprom(const R01sEntity *e);
int ui_chip_is_controller_attiny(const R01sEntity *e);
void ui_chip_body_rgb(const R01sEntity *e, int selected, Uint8 *r, Uint8 *g, Uint8 *b);
int ui_chip_hidden(const R01sUi *ui, const R01sEntity *e);

/* ui_draw.c */
int hit_board_top(const R01sUi *ui, int lx, int ly, int *chip_out, int *island_out, int *corner_out);
int ui_health_copy_at(R01sUi *ui, int lx, int ly);
void ui_tip_reset(R01sUi *ui, int mx, int my);
void ui_chip_dip_pin_pos(const R01sEntity *e, int pin_num, int *along, int *side_pin1);
int ui_chip_pin_screen_center(const R01sUi *ui, const R01sEntity *e, int pin_index, int *sx, int *sy);
void ui_chip_pin_rgb(const R01sUi *ui, R01sLevel lvl, R01sPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb);

/* ui_pin_net.c */
void r01s_ui_pin_net_build(R01sBoard *board);
int ui_hit_chip_pin(const R01sUi *ui, int lx, int ly, int *chip_out, int *pin_out);
int ui_pin_net_peer(const R01sUi *ui, int chip_i, int pin_i, int *peer_chip_out, int *peer_pin_out);
int ui_ic_connected_peers(SDL_Renderer *r, const R01sUi *ui, int chip_i);
void ui_draw_pin_wire_overlay(SDL_Renderer *r, R01sUi *ui);
int ui_islands_strip_contains(const R01sUi *ui, int lx, int ly);
void ui_islands_strip_clamp(R01sUi *ui);
int ui_legend_strip_contains(const R01sUi *ui, int lx, int ly);
void ui_legend_strip_clamp(R01sUi *ui);
int ui_wave_monitor_contains(const R01sUi *ui, int lx, int ly);
void ui_wave_monitor_clamp(R01sUi *ui);
int ui_wave_monitor_default_y(void);

#endif
