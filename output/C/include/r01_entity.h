#ifndef R01_ENTITY_H
#define R01_ENTITY_H

#include <stdint.h>
int r01_entity_spawn(uint8_t type, int wx, int wy);
void r01_entity_remove(int inst);
void r01_world_warp_screen(int col, int row);

#endif
