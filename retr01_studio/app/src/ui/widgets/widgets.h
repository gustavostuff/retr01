#ifndef retr01_STUDIO_UI_WIDGETS_H
#define retr01_STUDIO_UI_WIDGETS_H

#include "ui/ui.h"

#include <SDL.h>

struct R01World;

typedef enum UiPalPlane {
    UI_PAL_PLANE_BG = 0,
    UI_PAL_PLANE_SPR = 1
} UiPalPlane;

/* Exclusive picker: ui_dot.png strip. unlocked_count gates hits (and dims locked slots). */
void ui_dot_strip_draw(SDL_Renderer *r, int x, int y, int count, int selected, int unlocked_count);
int ui_dot_strip_hit(int lx, int ly, int x, int y, int count, int *out_idx);

void ui_radio_draw(SDL_Renderer *r, int dx, int dy, int selected);
void ui_checkbox_draw(SDL_Renderer *r, int dx, int dy, int checked);

void ui_palette_grid_draw(SDL_Renderer *r, const R01Project *p, int row, int pal_x, int pal_y, int sel_pal,
                          int sel_color, UiPalPlane plane);
int ui_palette_grid_hit(int lx, int ly, int pal_x, int pal_y, int *out_pal, int *out_color);

void ui_button_draw(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover);

void ui_modal_scrim(SDL_Renderer *r);
void ui_modal_panel(SDL_Renderer *r, int mx, int my, int w, int h, const char *title);
void ui_modal_save_cancel(SDL_Renderer *r, int x, int y, int save_w, int cancel_w, int mouse_x, int mouse_y);
int ui_modal_save_hit(int lx, int ly, int x, int y, int save_w);
int ui_modal_cancel_hit(int lx, int ly, int x, int y, int save_w, int cancel_w);

int ui_compose_clamp_part(int v);
int ui_compose_clamp_origin(int v);
void ui_compose_draw_grid(SDL_Renderer *r, int ox, int oy, int size_px);
void ui_compose_draw_part(SDL_Renderer *r, const R01Project *p, const struct R01World *w, const R01EntityPart *pt,
                          int ox, int oy, int scale, int selected);
void ui_compose_draw_frame(SDL_Renderer *r, const R01Project *p, const struct R01World *w, const R01EntityFrame *fr,
                           int ox, int oy, int scale, int sel_part);
int ui_compose_part_at(const R01EntityFrame *fr, int px, int py, int prefer_sel);
void ui_compose_paint_part(R01Project *p, struct R01World *w, R01EntityPart *pt, int cx, int cy, int paint_color);

/* Compatibility aliases (existing call sites). */
#define draw_dot_strip ui_dot_strip_draw
#define dot_strip_hit ui_dot_strip_hit
#define draw_radio_sprite ui_radio_draw
#define draw_checkbox_sprite ui_checkbox_draw
#define draw_button ui_button_draw
#define draw_spr_palette_grid(r, proj, row, px, py, sp, sc) \
    ui_palette_grid_draw((r), (proj), (row), (px), (py), (sp), (sc), UI_PAL_PLANE_SPR)
#define spr_palette_hit ui_palette_grid_hit

#endif
