#include "r01_pad_keys.h"

#include <SDL.h>

uint8_t r01_pad_bits_p1(const uint8_t *keys) {
    uint8_t b = 0;
    if (!keys) {
        return 0;
    }
    if (keys[SDL_SCANCODE_D]) {
        b |= R01_PAD_RIGHT;
    }
    if (keys[SDL_SCANCODE_A]) {
        b |= R01_PAD_LEFT;
    }
    if (keys[SDL_SCANCODE_S]) {
        b |= R01_PAD_DOWN;
    }
    if (keys[SDL_SCANCODE_W]) {
        b |= R01_PAD_UP;
    }
    if (keys[SDL_SCANCODE_W] && keys[SDL_SCANCODE_S]) {
        b = (uint8_t)(b & (uint8_t)~(R01_PAD_UP | R01_PAD_DOWN));
    }
    if (keys[SDL_SCANCODE_A] && keys[SDL_SCANCODE_D]) {
        b = (uint8_t)(b & (uint8_t)~(R01_PAD_LEFT | R01_PAD_RIGHT));
    }
    if (keys[SDL_SCANCODE_G]) {
        b |= R01_PAD_X;
    }
    if (keys[SDL_SCANCODE_H]) {
        b |= R01_PAD_Y;
    }
    if (keys[SDL_SCANCODE_1]) {
        b |= R01_PAD_COIN;
    }
    if (keys[SDL_SCANCODE_2]) {
        b |= R01_PAD_START;
    }
    return b;
}

uint8_t r01_pad_bits_p2(const uint8_t *keys) {
    uint8_t b = 0;
    if (!keys) {
        return 0;
    }
    if (keys[SDL_SCANCODE_RIGHT]) {
        b |= R01_PAD_RIGHT;
    }
    if (keys[SDL_SCANCODE_LEFT]) {
        b |= R01_PAD_LEFT;
    }
    if (keys[SDL_SCANCODE_DOWN]) {
        b |= R01_PAD_DOWN;
    }
    if (keys[SDL_SCANCODE_UP]) {
        b |= R01_PAD_UP;
    }
    if (keys[SDL_SCANCODE_UP] && keys[SDL_SCANCODE_DOWN]) {
        b = (uint8_t)(b & (uint8_t)~(R01_PAD_UP | R01_PAD_DOWN));
    }
    if (keys[SDL_SCANCODE_LEFT] && keys[SDL_SCANCODE_RIGHT]) {
        b = (uint8_t)(b & (uint8_t)~(R01_PAD_LEFT | R01_PAD_RIGHT));
    }
    if (keys[SDL_SCANCODE_COMMA] || keys[SDL_SCANCODE_KP_1]) {
        b |= R01_PAD_X;
    }
    if (keys[SDL_SCANCODE_PERIOD] || keys[SDL_SCANCODE_KP_2]) {
        b |= R01_PAD_Y;
    }
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
        b |= R01_PAD_COIN;
    }
    if (keys[SDL_SCANCODE_RETURN]) {
        b |= R01_PAD_START;
    }
    return b;
}
