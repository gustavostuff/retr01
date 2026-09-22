#ifndef R01_ENTITY_H
#define R01_ENTITY_H

#include <stdint.h>

extern uint8_t r01_live_n;
extern uint8_t r01_live_type[16];
extern uint8_t r01_live_state[16];
extern uint16_t r01_live_x[16];
extern uint16_t r01_live_y[16];

#define r01_entity_count() (r01_live_n)
#define r01_entity_type(id) (r01_live_type[(id)])
#define r01_entity_state(id) (r01_live_state[(id)])
#define r01_entity_set_state(id, st) (r01_live_state[(id)] = (st))

static inline void r01_entity_get_pos(uint8_t id, uint16_t *x, uint16_t *y) {
    if (x) {
        *x = r01_live_x[id];
    }
    if (y) {
        *y = r01_live_y[id];
    }
}

static inline void r01_entity_set_pos(uint8_t id, uint16_t x, uint16_t y) {
    r01_live_x[id] = x;
    r01_live_y[id] = y;
}

#endif
