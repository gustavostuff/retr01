#ifndef R01_INPUT_H
#define R01_INPUT_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;

#define R01_PAD_RIGHT 0x01u
#define R01_PAD_LEFT 0x02u
#define R01_PAD_DOWN 0x04u
#define R01_PAD_UP 0x08u
#define R01_PAD_X 0x10u
#define R01_PAD_Y 0x20u
#define R01_PAD_COIN 0x40u
#define R01_PAD_START 0x80u

void r01_pad_poll(R01GameCtx *ctx);
uint8_t r01_pad_down(const R01GameCtx *ctx, uint8_t mask);

#endif
