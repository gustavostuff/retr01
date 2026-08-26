#ifndef retr01_SIM_COMPOSITOR_H
#define retr01_SIM_COMPOSITOR_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Island O — compositor PLD stub (BG/sprite priority mux -> 6-bit PROM index).
 * Sprite path from Island N linebuf; transparent (0) defers to BG.
 * Output indexes Color PROM on the 256×240 RGBS path.
 */
typedef struct R01sCompositor {
    R01sEntity base;
    uint8_t bg_index;
    uint8_t spr_index;
    int spr_enable;
    uint8_t out_index;
} R01sCompositor;

void r01s_compositor_init(R01sCompositor *chip, const char *refdes);
R01sEntity *r01s_compositor_entity(R01sCompositor *chip);

void r01s_compositor_set_bg(R01sCompositor *chip, uint8_t index);
void r01s_compositor_set_sprite(R01sCompositor *chip, uint8_t index, int enable);
uint8_t r01s_compositor_out(const R01sCompositor *chip);

#endif
