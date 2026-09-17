#ifndef retr01_STUDIO_UI_INTERNAL_H
#define retr01_STUDIO_UI_INTERNAL_H

#include "ui/ui.h"
#include "ui/widgets/widgets.h"

/* fill_rect / snap8 / point_in_rect / draw_label / UiClipStack: retr01_ui */

#include <SDL.h>
#include <stdint.h>

typedef struct AccordionLayout {
    int worlds_hdr_y;
    int worlds_btns_y;
    int worlds_grid_y;
    int worlds_open;
    int worlds_body_h;
    int pals_hdr_y;
    int pals_body_y;
    int pals_open;
    int pals_body_h;
    int sprites_hdr_y;
    int sprites_body_y;
    int sprites_open;
    int sprites_body_h;
    int global_banks_hdr_y;
    int global_banks_body_y;
    int global_banks_open;
    int global_banks_body_h;
    int metatiles_hdr_y;
    int metatiles_body_y;
    int metatiles_open;
    int metatiles_body_h;
    int metasprites_hdr_y;
    int metasprites_body_y;
    int metasprites_open;
    int metasprites_body_h;
    int entities_hdr_y;
    int entities_body_y;
    int entities_open;
    int entities_body_h;
} AccordionLayout;

typedef struct SoundEditorLayout {
    int content_y;
    int plane_tabs_x, plane_tabs_y, plane_tab_w;
    int track_list_y, track_row_h;
    int add_x, add_y, add_w;
    int hdr_y;
    int lane_label_x, lane_label_w;
    int timeline_x, timeline_y, timeline_w, timeline_h;
    int lane_h, lane_gap;
    int px_per_tick;
    int visible_ticks;
    int ruler_h;
    int minimap_y, minimap_h;
    int insp_x, insp_y;
    int play_x, play_y, play_w;
    int pause_x, pause_y, pause_w;
    int stop_x, stop_y, stop_w;
    int ch_radio_y0;
} SoundEditorLayout;

static inline void ui_world_btn_pos(int wi, int btns_y, int *out_x, int *out_y) {
    if (out_x) {
        *out_x = UI_WORLDS_X + wi * UI_WORLD_BTN;
    }
    if (out_y) {
        *out_y = btns_y;
    }
}

typedef struct TileModalLayout {
    int mx, my, mw, mh;
    int pal_x, pal_label_y, pal_y;
    int canvas_x, canvas_y;
    int btn_y, save_w, cancel_w;
    int left_btn_x;
#if UI_PANEL_DEBUG_GRID
    UiPanel dbg_panel;
#endif
} TileModalLayout;

typedef struct PalModalLayout {
    int mx, my, mw, mh;
    int master_x, master_y;
    int bg_label_y, bg_x, bg_y;
    int spr_label_y, spr_x, spr_y;
    int btn_y, save_w, cancel_w;
    int left_btn_x;
#if UI_PANEL_DEBUG_GRID
    UiPanel dbg_panel;
#endif
} PalModalLayout;

typedef struct SpriteModalLayout {
    int mx, my, mw, mh;
    int pal_x, pal_label_y, pal_y;
    int canvas_x, canvas_y;
    int btn_y, save_w, cancel_w;
    int left_btn_x;
#if UI_PANEL_DEBUG_GRID
    UiPanel dbg_panel;
#endif
} SpriteModalLayout;

typedef struct MetaspriteModalLayout {
    int mx, my, mw, mh;
    int left_label_y;
    int left_dots_x, left_dots_y;
    int left_grid_x, left_grid_y;
    int right_name_x, right_name_y, right_name_w;
    int right_id_y, right_id_w;
    int right_grid_x, right_grid_y;
    int pal_label_x, pal_label_y;
    int pal_x, pal_y;
    int btn_y, save_w, cancel_w;
    int left_btn_x;
#if UI_PANEL_DEBUG_GRID
    UiPanel dbg_panel;
#endif
} MetaspriteModalLayout;

typedef struct ProjectIoModalLayout {
    int mx, my, mw, mh;
    int dir_x, dir_y, dir_w;
    int name_x, name_y, name_w;
    int list_x, list_y, list_w, list_h;
    int btn_y, save_w, cancel_w, open_w;
    int left_btn_x;
#if UI_PANEL_DEBUG_GRID
    UiPanel dbg_panel;
#endif
} ProjectIoModalLayout;

typedef struct EntityModalLayout {
    int mx, my, mw, mh;
    int name_x, name_y, name_w;
    int state_name_x, state_name_y, state_name_w;
    int state_y;
    int state_dots_x, state_dots_y;
    int frame_y;
    int frame_dots_x, frame_dots_y;
    int frame_id_x, frame_id_y, frame_id_w;
    int left_x, left_w; /* left body column (palette) */
    int pal_x, pal_y;
    int right_x, right_w; /* right body column (canvas) */
    int right_grid_x, right_grid_y;
    int add_spr_x, add_spr_y, add_spr_w;
    int rem_spr_x, rem_spr_y, rem_spr_w;
    int highlight_x, highlight_y, highlight_w; /* checkbox + "Highlight" */
    int brush_lab_x, brush_lab_y;
    int brush_x, brush_y, brush_w;
    int guides_x, guides_y;     /* "Mode" label */
    int mode_x, mode_y, mode_w; /* Sprite select | paint | Origin/hitbox */
    int btn_y, save_w, cancel_w;
    int left_btn_x;
#if UI_PANEL_DEBUG_GRID
    UiPanel dbg_panel;
#endif
} EntityModalLayout;

extern uint8_t *g_radio_rgba;
extern int g_radio_w;
extern int g_radio_h;
extern uint8_t *g_dot_rgba;
extern int g_dot_w;
extern int g_dot_h;
extern uint8_t *g_checkbox_rgba;
extern int g_checkbox_w;
extern int g_checkbox_h;
extern uint8_t *g_cross_rgba;
extern int g_cross_w;
extern int g_cross_h;
extern uint8_t *g_bg0_btn_rgba;
extern int g_bg0_btn_w;
extern int g_bg0_btn_h;
extern uint8_t *g_bg1_btn_rgba;
extern int g_bg1_btn_w;
extern int g_bg1_btn_h;
extern uint8_t *g_bg_bank_btn_rgba;
extern int g_bg_bank_btn_w;
extern int g_bg_bank_btn_h;
extern uint8_t *g_spr_bank_btn_rgba;
extern int g_spr_bank_btn_w;
extern int g_spr_bank_btn_h;

extern SDL_Cursor *g_cursor_arrow;
extern SDL_Cursor *g_cursor_hand;
extern SDL_Cursor *g_cursor_sizewe;

/* ui/primitives.c */
int ui_load_png_rgba(const char *path, uint8_t **out_px, int *out_w, int *out_h);

/* Paste clipboard PNG into an 8x8 CHR buffer (alpha->0, HSV Value match to pal). spr_plane=1 for SPR pals. */
int ui_paste_clipboard_png_tile(UiState *ui, uint8_t chr[R01_TILE_BYTES], int pal, int spr_plane);
void ui_toast(UiState *ui, const char *msg, int is_error);
void ui_tooltip_set(UiState *ui, int x, int y, const char *line1, const char *line2);
void ui_tooltip_hover(UiState *ui, int x, int y, const char *line1, const char *line2);
void ui_tooltip_frame_begin(UiState *ui);
void ui_tooltip_frame_end(UiState *ui);
void ui_tooltip_clear(UiState *ui);
void draw_tooltip(UiState *ui, SDL_Renderer *r);
void ui_focus_set(UiState *ui, int focus);
int ui_focus_get(const UiState *ui);
void ui_focus_clear(UiState *ui);
/* Animated 1px dashed selection border (phase from SDL_GetTicks). */
void draw_marching_ants(SDL_Renderer *r, int x, int y, int w, int h);
void draw_marching_ants_a(SDL_Renderer *r, int x, int y, int w, int h, Uint8 alpha);
/* Draw text clipped to a rectangle (scissor). */
void font_draw_clipped(SDL_Renderer *r, int x, int y, int clip_x, int clip_y, int clip_w, int clip_h,
                       const char *text, Uint8 R, Uint8 G, Uint8 B);
void draw_brush_preview(SDL_Renderer *r, const R01Project *p, int row, int pal, int color, int mx, int my);
void draw_paint_pixel_preview(SDL_Renderer *r, const R01Project *p, int row, UiPalPlane plane, int pal, int color,
                              int px, int py, int cell);
void draw_ui_cross(SDL_Renderer *r, int cx, int cy);
void draw_chess_grid(SDL_Renderer *r, int x0, int y0, int cols, int rows, int cell);

/* ui/layout.c */
void ui_editor_layout(const UiState *ui, int *screen_x, int *screen_y, int *layer_x, int *mode_x, int *mode_y0);
void ui_preview_size(const UiState *ui, int *out_w, int *out_h);
int screen_mode_hit(const UiState *ui, int lx, int ly, int *out_row);
int screen_mode_row_hit(const UiState *ui, int lx, int ly, int row);
int screen_layer_hit(const UiState *ui, int lx, int ly, int *out_layer);
int screen_layer_row_hit(const UiState *ui, int lx, int ly, int row);
int screen_hide_hit(const UiState *ui, int lx, int ly, int *out_hide_bg);
int screen_hide_row_hit(const UiState *ui, int lx, int ly, int row);
int ui_work_allows_bg(const UiState *ui);
int ui_work_allows_spr(const UiState *ui);
int ui_mode_label_x(int mode_x);
int ui_mode_panel_w(void);
int ui_layer_panel_w(void);
void screen_origin(const UiState *ui, int *ox, int *oy);
int screen_hit(const UiState *ui, int lx, int ly, int *out_tx, int *out_ty);
int screen_pixel_hit(const UiState *ui, int lx, int ly, int *out_px, int *out_py);
void accordion_init_heights(UiState *ui);
void accordion_anim_tick(UiState *ui);
void accordion_layout(const UiState *ui, AccordionLayout *lo);
int world_cell_hit(const UiState *ui, int lx, int ly, int *out_col, int *out_row);
int world_btn_hit(const UiState *ui, int lx, int ly, int *out_wi);
int world_sub_hit(const UiState *ui, int lx, int ly);
void worlds_tabs_prepare(const UiState *ui, UiTabsLayout *out);
int accordion_header_hit(const UiState *ui, int lx, int ly, int *out_section);
void accordion_toggle(UiState *ui, int section);
void draw_accordion_header(SDL_Renderer *r, int y, const char *title, int open, int hover);
void app_mode_tabs_prepare(const UiState *ui, UiTabsLayout *out);
int app_mode_tab_hit(const UiState *ui, int lx, int ly, int *out_idx);
void draw_app_mode_tabs(UiState *ui, SDL_Renderer *r);
void sound_editor_layout(const UiState *ui, SoundEditorLayout *lo);
void sound_plane_tabs_prepare(const UiState *ui, UiTabsLayout *out);
int sound_plane_tab_hit(const UiState *ui, int lx, int ly, int *out_idx);
int sound_track_hit(const UiState *ui, int lx, int ly, int *out_idx);
int sound_add_hit(const UiState *ui, int lx, int ly);
int sound_timeline_hit(const UiState *ui, int lx, int ly, int *out_ch, int *out_tick);
/* hit: 0 miss, 1 body, 2 left handle, 3 right handle; out_region set on hit */
int sound_region_hit(const UiState *ui, int lx, int ly, int *out_ch, int *out_region, int *out_handle);
int sound_channel_hit(const UiState *ui, int lx, int ly, int *out_ch);
int sound_play_hit(const UiState *ui, int lx, int ly);
int sound_pause_hit(const UiState *ui, int lx, int ly);
int sound_stop_hit(const UiState *ui, int lx, int ly);
void ui_sound_init(UiState *ui);
int ui_sound_audio_init(void);
void ui_sound_audio_shutdown(void);
void ui_sound_play_start(UiState *ui);
void ui_sound_play_stop(UiState *ui);
void ui_sound_play_pause(UiState *ui);
void ui_sound_play_toggle(UiState *ui);
void ui_sound_play_poll(UiState *ui);
int ui_sound_play_step(void);
void tile_modal_layout(const UiState *ui, TileModalLayout *lo);
void pal_modal_layout(const UiState *ui, PalModalLayout *lo);
void sprite_modal_layout(const UiState *ui, SpriteModalLayout *lo);
void metasprite_modal_layout(const UiState *ui, MetaspriteModalLayout *lo);
void entity_modal_layout(const UiState *ui, EntityModalLayout *lo);
int play_btn_w(const UiState *ui);
int play_btn_x(const UiState *ui);
int play_btn_y(const UiState *ui);
int play_button_hit(const UiState *ui, int lx, int ly);
int sprites_list_hit(const UiState *ui, int lx, int ly, int *out_catalog_idx);
int sprites_add_hit(const UiState *ui, int lx, int ly);
int banks_cell_hit(const UiState *ui, int lx, int ly, int *out_tile_id);
int banks_tab_hit(const UiState *ui, int lx, int ly, int *out_idx);
int banks_sub_hit(const UiState *ui, int lx, int ly);
void banks_tabs_prepare(const UiState *ui, UiTabsLayout *out);
void global_banks_tabs_prepare(const UiState *ui, UiTabsLayout *out);
int global_banks_tab_hit(const UiState *ui, int lx, int ly, int *out_idx);
int global_banks_sub_hit(const UiState *ui, int lx, int ly);
int global_banks_cell_hit(const UiState *ui, int lx, int ly, int *out_tile_id);
void bank_sel_set(UiState *ui, int plane, int bank, int tile_id);
void bank_sel_clear(UiState *ui);
int bank_sel_valid(const UiState *ui);
int metatiles_list_hit(const UiState *ui, int lx, int ly, int *out_idx);
int metatiles_add_hit(const UiState *ui, int lx, int ly);
int metasprites_list_hit(const UiState *ui, int lx, int ly, int *out_idx);
int metasprites_add_hit(const UiState *ui, int lx, int ly);
int entities_list_hit(const UiState *ui, int lx, int ly, int *out_type_idx);
int entities_add_hit(const UiState *ui, int lx, int ly);

/* ui/modals/pal_edit.c */
int palette_strip_hit(const UiState *ui, int lx, int ly);
int palette_row_btn_hit(const UiState *ui, int lx, int ly, int *out_row);
void pal_edit_close(UiState *ui);
void pal_edit_cancel(UiState *ui);
void pal_edit_save(UiState *ui);
void pal_edit_open(UiState *ui);
void pal_edit_set_row(UiState *ui, int row, int commit_default);
void pal_edit_nudge_master(UiState *ui, int wheel_y, int shift);
int pal_modal_master_hit(const UiState *ui, int lx, int ly, int *out_col, int *out_row);
int pal_modal_plane_hit(const UiState *ui, int lx, int ly, int plane, int *out_pal, int *out_color);
int pal_modal_handle(UiState *ui, int lx, int ly, int down);
void draw_pal_modal(UiState *ui, SDL_Renderer *r);

/* ui/menu/menu.c */
void menu_close(UiState *ui);
void menu_sync_tile_edit_label(UiState *ui);
void menu_open_tile(UiState *ui, int x, int y, int tx, int ty);
void menu_open_world_cell(UiState *ui, int x, int y, int screen_idx);
void menu_open_sprite(UiState *ui, int x, int y, int catalog_idx);
void menu_open_bank_cell(UiState *ui, int x, int y, int bank, int tile_id, int plane);
void menu_open_metasprite(UiState *ui, int x, int y, int meta_idx);
void menu_open_metatile(UiState *ui, int x, int y, int metatile_idx);
void menu_open_entity(UiState *ui, int x, int y, int type_idx);
void menu_open_entity_compose(UiState *ui, int x, int y, int compose_wx, int compose_wy);
void menu_open_instance(UiState *ui, int x, int y, int instance_idx);
int menu_hit(const UiState *ui, int lx, int ly, int *out_item, int *out_sub);
void menu_update_hover(UiState *ui, int lx, int ly);
void handle_menu_pick(UiState *ui, int item, int is_sub);

/* ui/screen/selection.c */
void screen_sel_set(UiState *ui, int x0, int y0, int x1, int y1);
void screen_sel_expand(UiState *ui, int tx, int ty);
void screen_sel_clear(UiState *ui);
int screen_sel_valid(const UiState *ui);
void screen_sel_bounds(const UiState *ui, int *min_x, int *min_y, int *max_x, int *max_y);
void screen_refresh_sel(UiState *ui);
int screen_sel_is_multi(const UiState *ui);

/* ui/screen/paint.c */
void ui_paint_stamp_set(UiState *ui, uint8_t tile, uint8_t attr);
void ui_paint_stamp_from_cell(UiState *ui, int tx, int ty);
void ui_paint_stamp_from_bank(UiState *ui, int plane, int bank, int tile_id);
void ui_paint_stamp_from_selection(UiState *ui);
int ui_paint_stamp_from_sel(const UiState *ui, uint8_t *out_tile, uint8_t *out_attr);
int ui_tile_selection_copy(UiState *ui);
int ui_tile_selection_paste(UiState *ui);
void ui_paint_tile(UiState *ui, int tx, int ty);
void ui_flood_fill(UiState *ui, int tx, int ty);

/* ui/screen/edit.c */
void ui_toggle_play(UiState *ui);
void ui_play_stop(UiState *ui);
void ui_play_boot_finish(UiState *ui, SDL_Renderer *ren);
int ui_play_screen_mark(const UiState *ui);
void screen_set_sel_bank(UiState *ui, int bank);
/* Clear selected screen cells to tile index 0 (keeps attrs; does not touch CHR). */
void screen_remove_sel_tiles(UiState *ui);
void screen_set_sel_pal(UiState *ui, int pal);
void screen_toggle_sel_flag(UiState *ui, uint8_t flag);
void screen_set_solid_by_hw(UiState *ui, int ref_tx, int ref_ty);

/* ui/screen/draw.c */
void draw_screen_editor(UiState *ui, SDL_Renderer *r, const R01Screen *s);
void draw_play_view(UiState *ui, SDL_Renderer *r);
void draw_preview_inspect(UiState *ui, SDL_Renderer *r);
void ui_preview_inspect_refresh(UiState *ui);
int preview_inspect_copy_hit(const UiState *ui, int lx, int ly);
void preview_inspect_copy(UiState *ui);
void draw_catalog_drag_ghost(UiState *ui, SDL_Renderer *r);
int instance_hit_on_screen(const UiState *ui, int lx, int ly, int *out_inst);

/* ui/modals/tile_edit.c */
void tile_edit_open(UiState *ui, int tx, int ty);
void tile_edit_open_all(UiState *ui, int tx, int ty);
void tile_edit_open_new(UiState *ui, int tx, int ty);
void tile_edit_open_bank(UiState *ui, int bank, int tile_id, int is_new);
void tile_edit_open_global_spr(UiState *ui, int bank, int tile_id, int is_new);
void tile_edit_open_global_bg(UiState *ui, int bank, int tile_id, int is_new);
int tile_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button);
void tile_modal_stroke_end(UiState *ui);
int tile_edit_undo(UiState *ui);
int tile_edit_redo(UiState *ui);
void draw_tile_modal(UiState *ui, SDL_Renderer *r);

/* ui/modals/sprite_edit.c */
void sprite_edit_open_new(UiState *ui);
void sprite_edit_open(UiState *ui, int catalog_idx);
void sprite_edit_open_slot(UiState *ui, int bank, int tile_id);
int sprite_modal_handle(UiState *ui, int lx, int ly, int down);
void draw_sprite_modal(UiState *ui, SDL_Renderer *r);

/* ui/modals/entity_edit.c */
void metasprite_edit_open_new(UiState *ui);
void metasprite_edit_open(UiState *ui, int meta_idx);
int metasprite_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button);
void metasprite_modal_drag(UiState *ui, int lx, int ly, Uint32 buttons);
void metasprite_modal_key(UiState *ui, SDL_Keycode sym);
void draw_metasprite_modal(UiState *ui, SDL_Renderer *r);

void entity_edit_open_new(UiState *ui);
void entity_edit_open(UiState *ui, int type_idx);
int entity_modal_handle(UiState *ui, int lx, int ly, int down, Uint8 button);
void entity_modal_drag(UiState *ui, int lx, int ly, Uint32 buttons);
void entity_modal_key(UiState *ui, SDL_Keycode sym);
int entity_modal_wheel(UiState *ui, int lx, int ly, int wheel_y, int shift);
/* Alloc blank SPR tile + place part at compose (wx, wy). Returns part idx or -1. */
int entity_edit_add_sprite_at(UiState *ui, int wx, int wy);
int entity_edit_paste_clipboard(UiState *ui);
void draw_entity_modal(UiState *ui, SDL_Renderer *r);

/* ui/draw/mode.c */
void ui_update_cursor(const UiState *ui);
void draw_screen_mode(UiState *ui, SDL_Renderer *r);
void draw_ctrl_sidebar(UiState *ui, SDL_Renderer *r);

/* ui/sound/draw.c */
void draw_sound_editor(UiState *ui, SDL_Renderer *r);

/* ui/draw/sidebar.c */
void draw_sidebar(UiState *ui, SDL_Renderer *r);

/* ui/menu/draw.c */
void draw_menu(UiState *ui, SDL_Renderer *r);

/* ui/input/project.c */
void ui_reset_after_project_load(UiState *ui);
void ui_save(UiState *ui);
void ui_export(UiState *ui);

/* ui/input/world.c */
int ui_screen_nav(UiState *ui, int dcol, int drow);
R01Screen *ui_edit_map_screen(const UiState *ui);
void handle_world_click(UiState *ui, int col, int row, int ctrl, int dbl);
int ui_world_screen_copy(UiState *ui);
int ui_world_screen_paste(UiState *ui);
int ui_world_screen_remove(UiState *ui);

#endif
