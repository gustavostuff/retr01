#ifndef R01_INPUT_H
#define R01_INPUT_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
uint8_t r01_pad_pressed(const R01GameCtx *ctx, uint8_t btn);
uint8_t r01_pad_just_pressed(R01GameCtx *ctx, uint8_t btn);
int r01_pad_down(const R01GameCtx *ctx, uint8_t mask);
#define R01_BTN_X 0
#define R01_BTN_Y 1
#define R01_PAD_RIGHT 0x01u
#define R01_PAD_LEFT 0x02u
#define R01_PAD_DOWN 0x04u
#define R01_PAD_UP 0x08u
#define R01_PAD_X 0x10u
#define R01_PAD_Y 0x20u
#define R01_PAD_COIN 0x40u
#define R01_PAD_START 0x80u

#endif
