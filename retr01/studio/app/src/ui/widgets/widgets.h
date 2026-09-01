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
/* Nudge selected slot master index on the conceptual 16x4 kit (wheel=rows, Shift+wheel=cols). */
void ui_palette_grid_nudge(R01Project *p, int row, UiPalPlane plane, int pal, int color, int wheel_y,
                           int shift);

void ui_button_draw(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover);

#define UI_TABS_MAX 16
#define UI_TABS_SUB_W 16
#define UI_TABS_SUB_H 8

typedef struct UiTabsLayout {
    int x, y;
    int tab_w;
    int tab_h;
    int count;
    const char *label[UI_TABS_MAX];
    int use_dot; /* draw ui_dot.png instead of label text */
    /* Dual-view (opt-in): sub-button only under selected tab. */
    int dual_view;
    int view; /* 0 = A, 1 = B */
    const uint8_t *sub_rgba[2];
    const char *sub_label[2];
    int sub_w;  /* slot width (usually 16) */
    int sub_h;  /* slot height (always UI_TABS_SUB_H = 8) */
    int sub_iw[2]; /* asset width per view */
    int sub_ih[2]; /* asset height per view */
} UiTabsLayout;

void ui_tabs_layout(const char *const *labels, int count, int x, int y, int tab_w, UiTabsLayout *out);
void ui_tabs_set_dual(UiTabsLayout *lo, int enabled, int view, const uint8_t *rgba_a, int wa, int ha,
                      const uint8_t *rgba_b, int wb, int hb);
void ui_tabs_set_dot(UiTabsLayout *lo, int enabled);
void ui_tabs_set_sub_labels(UiTabsLayout *lo, const char *label_a, const char *label_b);
int ui_tabs_body_y(const UiTabsLayout *lo);
void ui_tabs_draw(SDL_Renderer *r, const UiTabsLayout *lo, int selected, int mouse_x, int mouse_y);
/* selected = active tab index (needed so inactive dual tabs keep full 16x16 hit). */
int ui_tabs_hit(const UiTabsLayout *lo, int selected, int lx, int ly, int *out_idx);
/* 1 if (lx,ly) hits the sub-button under selected tab (dual-view only). */
int ui_tabs_sub_hit(const UiTabsLayout *lo, int selected, int lx, int ly);

void ui_modal_scrim(SDL_Renderer *r, const UiState *ui);
void ui_modal_panel(SDL_Renderer *r, int mx, int my, int w, int h, const char *title);
void ui_modal_save_cancel(SDL_Renderer *r, int x, int y, int save_w, int cancel_w, int mouse_x, int mouse_y);
int ui_modal_save_hit(int lx, int ly, int x, int y, int save_w);
int ui_modal_cancel_hit(int lx, int ly, int x, int y, int save_w, int cancel_w);
/* 1 if click is outside the panel (dismiss overlay). */
int ui_modal_overlay_hit(int lx, int ly, int mx, int my, int w, int h);

/* Text fields (caret, selection, scroll). field_id must be > 0 while focused. */
void ui_text_blur(UiState *ui);
void ui_text_focus(UiState *ui, char *buf, int cap, int field_id);
int ui_text_active(const UiState *ui, int field_id);
void ui_text_draw(UiState *ui, SDL_Renderer *r, int x, int y, int w, const char *text, int field_id);
int ui_text_mouse_down(UiState *ui, int lx, int ly, int x, int y, int w, char *buf, int cap, int field_id);
void ui_text_mouse_up(UiState *ui);
void ui_text_mouse_drag(UiState *ui, int lx, int x, int w);
int ui_text_key(UiState *ui, SDL_Keycode sym, Uint16 mod);
int ui_text_input(UiState *ui, const char *utf8);

int ui_compose_clamp_part(int v);
int ui_compose_clamp_origin(int v);
void ui_compose_draw_grid(SDL_Renderer *r, int ox, int oy, int size_px, int cell_px);
void ui_compose_draw_part(SDL_Renderer *r, const R01Project *p, const struct R01World *w, const R01EntityPart *pt,
                          int ox, int oy, int scale, int selected);
void ui_compose_draw_frame(SDL_Renderer *r, const R01Project *p, const struct R01World *w, const R01EntityFrame *fr,
                           int ox, int oy, int scale, int sel_part);
/* Center parts on bbox mid-point inside icon_size x icon_size (clipped). */
void ui_compose_draw_frame_icon(SDL_Renderer *r, const R01Project *p, const struct R01World *w,
                                const R01EntityFrame *fr, int dx, int dy, int icon_size);
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
