#ifndef retr01_SIM_UI_INTERNAL_H
#define retr01_SIM_UI_INTERNAL_H

#include "ui.h"
#include "video_sink.h"

#include <SDL.h>
#include <stdint.h>

#ifndef R01S_ASSETS_DIR
#define R01S_ASSETS_DIR "retr01_sim/assets"
#endif

#define R01S_UI_FONT_PX 16

#define R01S_UI_PIN_GRAY_R 179
#define R01S_UI_PIN_GRAY_G 179
#define R01S_UI_PIN_GRAY_B 204

#define R01S_UI_STATUS_ROW_H 16

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

/* ui_draw_chip.c */
void pin_level_rgb(R01sLevel lvl, R01sPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb);
/* Board-space tip of a named pin (1 = ok). Works for DIP and glyph packages. */
int r01s_ui_chip_pin_tip(const R01sEntity *e, const char *pin_name, int *out_bx, int *out_by);
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
void compact_btn_rect(const R01sUi *ui, SDL_Rect *rc);
void save_btn_rect(const R01sUi *ui, SDL_Rect *rc);
void ui_save_layout_now(R01sUi *ui);
void ui_draw_connections(R01sUi *ui, SDL_Renderer *r);
/* Ctrl+click in Connections mode: insert a route vertex. Returns 1 if handled. */
int ui_conn_ctrl_click(R01sUi *ui, int board_x, int board_y);
R01sWireRoute *ui_wire_find(R01sUi *ui, const char *ref_a, const char *pin_a, const char *ref_b,
                            const char *pin_b);
R01sWireRoute *ui_wire_ensure(R01sUi *ui, const R01sNetEdge *edge);
int ui_lcd_scale_2x(const R01sUi *ui);
int ui_screen_render_mode(const R01sUi *ui);
void ui_set_lcd_scale(R01sUi *ui, int scale_2x);
void ui_set_screen_render_mode(R01sUi *ui, int mode);
void ui_toggle_lcd_scale(R01sUi *ui);
int ui_chip_is_cart_flash(const R01sEntity *e);
int ui_chip_is_cart_eeprom(const R01sEntity *e);
int ui_chip_hidden(const R01sUi *ui, const R01sEntity *e);

/* ui_draw.c */
int radio_hit(const SDL_Rect *rc, int mx, int my);
void sidebar_probe_quiet_btn_rect(const R01sUi *ui, int probe_py, SDL_Rect *rc);
void sidebar_cart_btn_rect(const R01sUi *ui, int which, SDL_Rect *rc);
void sidebar_scale_btn_rect(const R01sUi *ui, int which, SDL_Rect *rc);
int sidebar_hit(int lx, int ly);
void sidebar_clamp_scroll(R01sUi *ui);
int sidebar_probe_content_y(const R01sUi *ui);
int sidebar_sy(const R01sUi *ui, int content_y);
int gp_hit_any(const R01sUi *ui, int lx, int ly, int *player_out, int *btn_out);
int hit_board_top(const R01sUi *ui, int lx, int ly, int *chip_out, int *island_out, int *corner_out);
void gp_stick_from_point(R01sUi *ui, R01sGamepadInput *gp, int player, int lx, int ly);
int ui_health_copy_at(R01sUi *ui, int lx, int ly);

#endif
