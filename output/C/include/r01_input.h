#ifndef R01_INPUT_H
#define R01_INPUT_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
uint8_t r01_pad_pressed(const R01GameCtx *ctx, uint8_t btn);
uint8_t r01_pad_just_pressed(R01GameCtx *ctx, uint8_t btn);
#define R01_BTN_X 0
#define R01_BTN_Y 1

#endif
