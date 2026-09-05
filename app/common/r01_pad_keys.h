#ifndef R01_PAD_KEYS_H
#define R01_PAD_KEYS_H

#include <stdint.h>

/* Keyboard -> $FE60/$FE61 bitfield. Matches Sim r01s_ui_sync_gamepads (ui_draw.c).
 *
 * P1: WASD move, G=X, H=Y, 1=Coin, 2=Start
 * P2: Arrows move, ,/KP1=X, ./KP2=Y, Shift=Coin, Enter=Start
 *
 * Bit layout: Right Left Down Up X Y Coin Start (same as R01E_PAD_*).
 */

#ifndef R01_PAD_RIGHT
#define R01_PAD_RIGHT 0x01u
#define R01_PAD_LEFT 0x02u
#define R01_PAD_DOWN 0x04u
#define R01_PAD_UP 0x08u
#define R01_PAD_X 0x10u
#define R01_PAD_Y 0x20u
#define R01_PAD_COIN 0x40u
#define R01_PAD_START 0x80u
#endif

/* keys = SDL_GetKeyboardState(NULL) */
uint8_t r01_pad_bits_p1(const uint8_t *keys);
uint8_t r01_pad_bits_p2(const uint8_t *keys);

#endif
