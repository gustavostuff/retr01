#ifndef RETR01_UI_WIDGETS_H
#define RETR01_UI_WIDGETS_H

#include "retr01_ui/chrome.h"
#include "retr01_ui/draw.h"
#include "retr01_ui/metrics.h"
#include "retr01_ui/text.h"

#include <SDL.h>
#include <stdint.h>

void ui_dot_strip_draw(SDL_Renderer *r, int x, int y, int count, int selected, int unlocked_count);
int ui_dot_strip_hit(int lx, int ly, int x, int y, int count, int *out_idx);

void ui_radio_draw(SDL_Renderer *r, int dx, int dy, int selected);
void ui_checkbox_draw(SDL_Renderer *r, int dx, int dy, int checked);

void ui_button_draw(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover);
void ui_button_draw_ex(SDL_Renderer *r, int x, int y, int w, const char *text, int active, int hover, int enabled);
void ui_button_draw_fill(SDL_Renderer *r, int x, int y, int w, const char *text, Uint8 fr, Uint8 fg, Uint8 fb,
                         int hover, int enabled);

void ui_slider_discrete_draw(SDL_Renderer *r, int x, int y, int w, int value, int count);
int ui_slider_discrete_hit(int lx, int ly, int x, int y, int w, int count, int *out_value);

int ui_multi_state_pref_width(const char *const *labels, int count);
void ui_multi_state_draw(SDL_Renderer *r, int x, int y, int w, const char *const *labels, int count, int selected,
                         int mouse_x, int mouse_y);
int ui_multi_state_hit(int lx, int ly, int x, int y, int w, int count, int selected, int *out_idx);

typedef struct UiTabsLayout {
    int x, y;
    int tab_w; /* default width when tab_ws[i] is 0 */
    int tab_ws[UI_TABS_MAX];
    int tab_h;
    int count;
    const char *label[UI_TABS_MAX];
    int use_dot;
    int dual_view;
    int view; /* 0 = A, 1 = B */
    const uint8_t *sub_rgba[2];
    const char *sub_label[2];
    int sub_w;
    int sub_h;
    int sub_iw[2];
    int sub_ih[2];
} UiTabsLayout;

void ui_tabs_layout(const char *const *labels, int count, int x, int y, int tab_w, UiTabsLayout *out);
void ui_tabs_set_dual(UiTabsLayout *lo, int enabled, int view, const uint8_t *rgba_a, int wa, int ha,
                      const uint8_t *rgba_b, int wb, int hb);
void ui_tabs_set_dot(UiTabsLayout *lo, int enabled);
void ui_tabs_set_sub_labels(UiTabsLayout *lo, const char *label_a, const char *label_b);
int ui_tabs_body_y(const UiTabsLayout *lo);
void ui_tabs_draw(SDL_Renderer *r, const UiTabsLayout *lo, int selected, int mouse_x, int mouse_y);
int ui_tabs_hit(const UiTabsLayout *lo, int selected, int lx, int ly, int *out_idx);
int ui_tabs_sub_hit(const UiTabsLayout *lo, int selected, int lx, int ly);

/* Pager tabs: prev 16, N/M 32, next 16. Optional plane slot is 64px, right-aligned.
 * Tab content starts at ui_tab_pager_content_y. Existing ui_tabs_* stay. */
typedef struct UiTabPager {
    int x, y, w, h;
    int count;
    int selected;
    int prev_x, prev_w;
    int next_x, next_w;
    int label_x, label_w;
    int plane_x, plane_w;
} UiTabPager;

void ui_tab_pager_layout(int x, int y, int w, int count, int selected, int plane_w, UiTabPager *out);
int ui_tab_pager_content_y(const UiTabPager *lo);
void ui_tab_pager_draw(SDL_Renderer *r, const UiTabPager *lo, int mouse_x, int mouse_y);
int ui_tab_pager_hit(const UiTabPager *lo, int lx, int ly);
int ui_tab_pager_step(const UiTabPager *lo, int hit);

typedef struct UiPanelCell {
    int id;
    int col;
    int row;
    int colspan;
    int rowspan;
} UiPanelCell;

typedef struct UiPanel {
    int x, y;
    int cols, rows;
    int cell_w, cell_h;
    int col_w[UI_PANEL_TRACKS_MAX];
    int row_h[UI_PANEL_TRACKS_MAX];
    const UiPanelCell *cells;
    int cell_count;
    int out_x[UI_PANEL_CELLS_MAX];
    int out_y[UI_PANEL_CELLS_MAX];
    int out_w[UI_PANEL_CELLS_MAX];
    int out_h[UI_PANEL_CELLS_MAX];
    int total_w, total_h;
} UiPanel;

void ui_panel_init(UiPanel *p, int cols, int rows, int cell_w, int cell_h);
void ui_panel_set_cells(UiPanel *p, const UiPanelCell *cells, int count);
void ui_panel_set_col_w(UiPanel *p, int col, int w);
void ui_panel_set_row_h(UiPanel *p, int row, int h);
void ui_panel_layout(UiPanel *p, int x, int y);
int ui_panel_cell(const UiPanel *p, int id, int *x, int *y, int *w, int *h);
void ui_panel_debug_draw(SDL_Renderer *r, const UiPanel *p);

void ui_modal_scrim(SDL_Renderer *r, int logic_w, int logic_h);
void ui_modal_panel(SDL_Renderer *r, int mx, int my, int w, int h, const char *title);
void ui_modal_save_cancel(SDL_Renderer *r, int x, int y, int save_w, int cancel_w, int mouse_x, int mouse_y);
int ui_modal_save_hit(int lx, int ly, int x, int y, int save_w);
int ui_modal_cancel_hit(int lx, int ly, int x, int y, int save_w, int cancel_w);
int ui_modal_overlay_hit(int lx, int ly, int mx, int my, int w, int h);

/* Compatibility aliases. */
#define draw_dot_strip ui_dot_strip_draw
#define dot_strip_hit ui_dot_strip_hit
#define draw_radio_sprite ui_radio_draw
#define draw_checkbox_sprite ui_checkbox_draw
#define draw_button ui_button_draw

#endif
