#ifndef DISCRETE_IC_PIN_HEADER_H
#define DISCRETE_IC_PIN_HEADER_H

#include "discrete_ic/entity.h"

#include <SDL.h>

/* Male pin header: fused NxM plastic, one level-colored pin pixel per cell.
 * Cell size matches breadboard pitch so multi-pin headers seat on the hole lattice. */
#ifdef R01A_BB_3PX
#define NS_PIN_HDR_CELL_PX 6
#else
#define NS_PIN_HDR_CELL_PX 5
#endif

void ns_entity_set_pin_header(NsEntity *e, int cols, int rows);
void ns_pin_header_grid(const NsEntity *e, int *cols, int *rows);
void ns_pin_header_refresh_body(NsEntity *e);

void ns_pin_header_level_rgb(NsLevel lvl, NsPinDir dir, Uint8 *r, Uint8 *g, Uint8 *b);

void ns_pin_header_draw(SDL_Renderer *r, const NsEntity *e, int sx, int sy, int selected);

int ns_pin_header_hit(const NsEntity *e, int bx, int by);
int ns_pin_header_pin_tip_board(const NsEntity *e, int pin_num, int *tbx, int *tby);

#endif
