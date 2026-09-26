#ifndef R01C_S1_LAB_H
#define R01C_S1_LAB_H

#include "as6c62256.h"

#include <stdint.h>

#define R01C_BG0_LINE_BASE 0x4000u
#define R01C_BG0_PING_STRIDE 128u

void r01c_s1_vblank_field(R01aAs6c62256 *field, uint32_t frame);
void r01c_s1_hblank_bg0_line(R01aAs6c62256 *field, int line_y, int ping);

#endif
