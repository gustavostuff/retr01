#ifndef retr01_STUDIO_UI_INTERNAL_H
#define retr01_STUDIO_UI_INTERNAL_H

#include "ui.h"

#include <SDL.h>
#include <stdint.h>

typedef struct AccordionLayout {
    int worlds_hdr_y;
    int worlds_btns_y;
    int worlds_grid_y;
    int worlds_open;
    int pals_hdr_y;
    int pals_body_y;
    int pals_open;
} AccordionLayout;

typedef struct TileModalLayout {
    int mx, my;
    int pal_x, pal_label_y, pal_y;
    int canvas_x, canvas_y;
    int btn_y, save_w, cancel_w;
} TileModalLayout;

typedef struct PalModalLayout {
    int mx, my;
    int master_x, master_y;
    int bg_label_y, bg_x, bg_y;
    int spr_label_y, spr_x, spr_y;
    int btn_y, save_w, cancel_w;
} PalModalLayout;

extern uint8_t *g_radio_rgba;
extern int g_radio_w;
extern int g_radio_h;

extern SDL_Cursor *g_cursor_arrow;
extern SDL_Cursor *g_cursor_hand;

/* ui_primitives.c */
int ui_load_png_rgba(const char *path, uint8_t **out_px, int *out_w, int *out_h);
int snap8(int v);
void ui_toast(UiState *ui, const char *msg, int is_error);
void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B);
void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B);
void hover_overlay(SDL_Renderer *r, int x, int y, int w, int h);
int point_in_rect(int lx, int ly, int x, int y, int w, int h);
int label_width(const char *text);
void draw_radio_sprite(SDL_Renderer *r, int dx, int dy, int selected);
void draw_label(SDL_Renderer *r, int x, int y, const char *text);
void draw_button(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover);
void draw_chess_grid(SDL_Renderer *r, int x0, int y0, int cols, int rows, int cell);

/* ui_layout.c */
void ui_editor_layout(const UiState *ui, int *screen_x, int *screen_y, int *mode_x, int *mode_y0);
int screen_mode_hit(const UiState *ui, int lx, int ly, int *out_row);
int screen_mode_row_hit(const UiState *ui, int lx, int ly, int row);
int ui_mode_label_x(int mode_x);
int ui_mode_panel_w(void);
void screen_origin(const UiState *ui, int *ox, int *oy);
int screen_hit(const UiState *ui, int lx, int ly, int *out_tx, int *out_ty);
void accordion_layout(const UiState *ui, AccordionLayout *lo);
int world_cell_hit(const UiState *ui, int lx, int ly, int *out_col, int *out_row);
int world_btn_hit(const UiState *ui, int lx, int ly, int *out_wi);
int accordion_header_hit(const UiState *ui, int lx, int ly, int *out_section);
void accordion_toggle(UiState *ui, int section);
void draw_accordion_header(SDL_Renderer *r, int y, const char *title, int open, int hover);
void tile_modal_layout(TileModalLayout *lo);
void pal_modal_layout(PalModalLayout *lo);
int play_btn_w(const UiState *ui);
int play_btn_x(const UiState *ui);
int play_btn_y(void);
int play_button_hit(const UiState *ui, int lx, int ly);

/* ui_pal_edit.c */
int palette_strip_hit(const UiState *ui, int lx, int ly);
int palette_row_btn_hit(const UiState *ui, int lx, int ly, int *out_row);
void pal_edit_close(UiState *ui);
void pal_edit_cancel(UiState *ui);
void pal_edit_save(UiState *ui);
void pal_edit_open(UiState *ui);
void pal_edit_set_row(UiState *ui, int row, int commit_default);
void pal_edit_nudge_master(UiState *ui, int wheel_y, int shift);
int pal_modal_master_hit(int lx, int ly, int *out_col, int *out_row);
int pal_modal_plane_hit(int lx, int ly, int plane, int *out_pal, int *out_color);
int pal_modal_handle(UiState *ui, int lx, int ly, int down);
void draw_pal_modal(UiState *ui, SDL_Renderer *r);

/* ui_menu.c */
void menu_close(UiState *ui);
void menu_open_tile(UiState *ui, int x, int y, int tx, int ty);
void menu_open_world_cell(UiState *ui, int x, int y, int screen_idx);
int menu_hit(const UiState *ui, int lx, int ly, int *out_item, int *out_sub);
void menu_update_hover(UiState *ui, int lx, int ly);
void handle_menu_pick(UiState *ui, int item, int is_sub);

/* ui_screen.c */
void screen_sel_set(UiState *ui, int x0, int y0, int x1, int y1);
void screen_sel_clear(UiState *ui);
int screen_sel_valid(const UiState *ui);
void screen_sel_bounds(const UiState *ui, int *min_x, int *min_y, int *max_x, int *max_y);
void screen_refresh_sel(UiState *ui);
int screen_sel_is_multi(const UiState *ui);
void ui_paint_stamp_set(UiState *ui, uint8_t tile, uint8_t attr);
void ui_paint_stamp_from_cell(UiState *ui, int tx, int ty);
int ui_paint_stamp_from_sel(const UiState *ui, uint8_t *out_tile, uint8_t *out_attr);
void ui_paint_tile(UiState *ui, int tx, int ty);
void ui_flood_fill(UiState *ui, int tx, int ty);
void ui_toggle_play(UiState *ui);
void screen_set_sel_bank(UiState *ui, int bank);
void screen_set_sel_pal(UiState *ui, int pal);
void screen_toggle_sel_flag(UiState *ui, uint8_t flag);
void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s);

/* ui_tile_edit.c */
void tile_edit_open(UiState *ui, int tx, int ty);
void tile_edit_open_new(UiState *ui, int tx, int ty);
int tile_modal_handle(UiState *ui, int lx, int ly, int down);
void draw_tile_modal(UiState *ui, SDL_Renderer *r);

/* ui_draw.c */
void ui_update_cursor(const UiState *ui);
void draw_sidebar(UiState *ui, SDL_Renderer *r);
void draw_screen_mode(UiState *ui, SDL_Renderer *r);
void draw_play_view(UiState *ui, SDL_Renderer *r);
void draw_menu(UiState *ui, SDL_Renderer *r);

/* ui_input.c */
void ui_reset_after_project_load(UiState *ui);
void ui_save(UiState *ui);
void ui_export(UiState *ui);
int ui_screen_nav(UiState *ui, int dcol, int drow);
void handle_world_click(UiState *ui, int col, int row, int ctrl, int dbl);

#endif
