#ifndef R01_PLAY_SYS_H
#define R01_PLAY_SYS_H

#include "r01_cart_caps.h"

#include <stdint.h>

/* PRG sys block in RAM (packed C play). Emu mirrors on $02E8 ready. */
#define R01_SYS_PLAYER_X 0x02E0u
#define R01_SYS_PLAYER_Y 0x02E2u
#define R01_SYS_CAM_X 0x02E4u
#define R01_SYS_CAM_Y 0x02E6u
#define R01_SYS_READY 0x02E8u
#define R01_SYS_VID_FLAGS 0x02E9u

static inline uint8_t r01_sys_vid_flags_pack(uint8_t bg0_wrap_x, uint8_t bg0_wrap_y, uint8_t bg0_clip_bg1) {
    uint8_t f = 0;
    if (bg0_wrap_x) {
        f = (uint8_t)(f | R01_CART_WHDR_FLAG_BG0_WRAP_X);
    }
    if (bg0_wrap_y) {
        f = (uint8_t)(f | R01_CART_WHDR_FLAG_BG0_WRAP_Y);
    }
    if (bg0_clip_bg1) {
        f = (uint8_t)(f | R01_CART_WHDR_FLAG_BG0_CLIP_BG1);
    }
    return f;
}

#endif
