#ifndef RETR01_SIM_GAMEPAD_H
#define RETR01_SIM_GAMEPAD_H

#include <stdint.h>

/*
 * Retr01 pad byte @ $FE60 (P1) / $FE61 (P2). 1 = pressed.
 * docs/02_graphics_worlds_memory.md
 */
#define R01S_PAD_RIGHT  (1u << 0)
#define R01S_PAD_LEFT   (1u << 1)
#define R01S_PAD_DOWN   (1u << 2)
#define R01S_PAD_UP     (1u << 3)
#define R01S_PAD_X      (1u << 4)
#define R01S_PAD_Y      (1u << 5)
#define R01S_PAD_COIN   (1u << 6) /* cabinet coin / console select */
#define R01S_PAD_START  (1u << 7)

#define R01S_GAMEPAD_STICK_RADIUS 28
#define R01S_GAMEPAD_STICK_DEAD  6

typedef struct R01sGamepadInput {
    int stick_x; /* offset from stick center, pixels */
    int stick_y;
    int btn_x;
    int btn_y;
    int btn_coin;
    int btn_start;
} R01sGamepadInput;

void r01s_gamepad_input_clear(R01sGamepadInput *gp);

/* 8-way stick: diagonals set two direction bits (two microswitches). */
uint8_t r01s_gamepad_stick_bits(int stick_x, int stick_y);

uint8_t r01s_gamepad_encode(const R01sGamepadInput *gp);

/* Clamp stick deflection to a circular gate. */
void r01s_gamepad_stick_clamp(int *stick_x, int *stick_y, int radius);

#endif
