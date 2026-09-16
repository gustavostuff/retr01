#ifndef R01_ENTITY_H
#define R01_ENTITY_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

/* Soft contract: general_docs/software-api.md (4x4x6 parts, 16 live). */
#define R01_ENTITY_ONSCREEN_MAX 16

int  r01_entity_spawn(uint8_t type, int wx, int wy);
void r01_entity_remove(int inst);
void r01_entity_set_active(int inst, int on);

void r01_entity_set_xy(int inst, int wx, int wy);
void r01_entity_get_xy(int inst, int *wx, int *wy);
void r01_entity_set_state(int inst, uint8_t state);
void r01_entity_set_frame(int inst, uint8_t frame);
void r01_entity_set_flip(int inst, int flip_h, int flip_v);
void r01_entity_get_flip(int inst, int *flip_h, int *flip_v);
void r01_entity_set_rot90(int inst, uint8_t quad);

int  r01_entity_alive(int inst);
int  r01_entity_count_live(void);
uint8_t r01_entity_type(int inst);

void r01_world_warp_screen(int col, int row);

#endif
