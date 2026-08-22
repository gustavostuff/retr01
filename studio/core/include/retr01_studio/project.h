#ifndef RETR01_STUDIO_PROJECT_H
#define RETR01_STUDIO_PROJECT_H

#include "retr01_studio/types.h"

void r01_project_init(R01Project *p, const char *name);

R01World *r01_project_world(R01Project *p, int world_index);
R01Screen *r01_project_active_screen(R01Project *p);

/* UI world tab 1..8 <-> hardware 0..7 */
static inline int r01_ui_world_to_hw(int ui_tab_1_based) {
    return ui_tab_1_based - 1;
}
static inline int r01_hw_world_to_ui(int hw_0_based) {
    return hw_0_based + 1;
}

/* Toggle screen at grid cell. Returns 0 ok, -1 if at cap when adding. */
int r01_world_toggle_screen(R01World *w, int col, int row);

/* Find screen index by col/row, or -1. */
int r01_world_find_screen(const R01World *w, int col, int row);

void r01_screen_clear_pixels(R01Screen *s, uint8_t color);
void r01_screen_plot(R01Screen *s, int x, int y, uint8_t color);
uint8_t r01_screen_get_pixel(const R01Screen *s, int x, int y);

void r01_screen_set_attr_bits(R01Screen *s, int tile_x, int tile_y, uint8_t set_mask, uint8_t clear_mask);
uint8_t r01_screen_get_attr(const R01Screen *s, int tile_x, int tile_y);

#endif
