#ifndef retr01_STUDIO_UI_WIDGETS_H
#define retr01_STUDIO_UI_WIDGETS_H

#include "ui/ui.h"

#include "retr01_ui/widgets.h"

#include <SDL.h>

struct R01World;

typedef enum UiPalPlane {
    UI_PAL_PLANE_BG = 0,
    UI_PAL_PLANE_SPR = 1
} UiPalPlane;

void ui_palette_grid_draw(SDL_Renderer *r, const R01Project *p, int row, int pal_x, int pal_y, int sel_pal,
                          int sel_color, UiPalPlane plane);
int ui_palette_grid_hit(int lx, int ly, int pal_x, int pal_y, int *out_pal, int *out_color);
void ui_palette_grid_nudge(R01Project *p, int row, UiPalPlane plane, int pal, int color, int wheel_y,
                           int shift);

int ui_compose_clamp_part(int v);
int ui_compose_clamp_origin(int v);
void ui_compose_draw_grid(SDL_Renderer *r, int ox, int oy, int size_px, int cell_px);
void ui_compose_draw_part(SDL_Renderer *r, const R01Project *p, const struct R01World *w, const R01EntityPart *pt,
                          int ox, int oy, int scale, int selected, int outline, Uint8 alpha);
void ui_compose_draw_frame(SDL_Renderer *r, const R01Project *p, const struct R01World *w, const R01EntityFrame *fr,
                           int ox, int oy, int scale, unsigned sel_mask, int show_outlines, Uint8 alpha);
void ui_compose_clamp_hitbox(int *x, int *y, int *w, int *h);
void ui_compose_draw_frame_icon(SDL_Renderer *r, const R01Project *p, const struct R01World *w,
                                const R01EntityFrame *fr, int dx, int dy, int icon_size);
int ui_compose_part_at(const R01EntityFrame *fr, int px, int py, int prefer_sel);
int ui_compose_sample_part(R01Project *p, struct R01World *w, const R01EntityPart *pt, int cx, int cy,
                           int *out_color);
int ui_compose_paint_part(R01Project *p, struct R01World *w, R01EntityPart *pt, int cx, int cy, int paint_color);
int ui_compose_paint_brush(R01Project *p, struct R01World *w, R01EntityPart *pt, int cx, int cy, int paint_color,
                           int brush_size);
void ui_compose_brush_stamp(int brush_size, int *out_w, int *out_h, const uint8_t **out_bits);

#define draw_spr_palette_grid(r, proj, row, px, py, sp, sc) \
    ui_palette_grid_draw((r), (proj), (row), (px), (py), (sp), (sc), UI_PAL_PLANE_SPR)
#define spr_palette_hit ui_palette_grid_hit

#endif
