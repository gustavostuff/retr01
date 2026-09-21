#ifndef R01_PAD_HOST_H
#define R01_PAD_HOST_H

#include "r01_pad_keys.h"

#include <SDL.h>

#include <stdint.h>

/* PC SDL Game Controller host. Community DB + SDL built-in mappings.
 * First two detected pads are P1 / P2. Keyboard bits OR with pad bits.
 * Guide / Home opens a 4-button overlay: Reset, Quit, 1x/2x present scale, Mute On/Off.
 */

enum {
    R01_PAD_MENU_NONE = 0,
    R01_PAD_MENU_RESET = 1,
    R01_PAD_MENU_QUIT = 2,
    R01_PAD_MENU_SCALE = 3,
    R01_PAD_MENU_MUTE = 4
};

void r01_pad_host_preinit(void);
int r01_pad_host_init(void);
void r01_pad_host_shutdown(void);
void r01_pad_host_event(const SDL_Event *e);

uint8_t r01_pad_host_bits(int player);

int r01_pad_host_menu_open(void);
int r01_pad_host_muted(void);
void r01_pad_host_menu_set_open(int open);
int r01_pad_host_tick(void);
int r01_pad_host_menu_keydown(int key, int repeat);
int r01_pad_host_menu_click(int x, int y, int ox, int oy, int vw, int vh);

void r01_pad_host_draw_menu(SDL_Renderer *ren, int ox, int oy, int vw, int vh, int scale_x);

#endif
