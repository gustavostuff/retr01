#ifndef retr01_SIM_BG_FETCH_H
#define retr01_SIM_BG_FETCH_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_BG_SCREEN_PX_W 128
#define R01S_BG_SCREEN_PX_H 120
#define R01S_BG_SCREEN_TILES_X 16
#define R01S_BG_SLOT_BYTES 512
#define R01S_BG_ATTR_OFF 0xF0

/*
 * Island I — behavioral BG nametable fetch PLD stub (needs G + H).
 * SCALE 2x (default): beam → logical via /2; odd beam X = attr cycle.
 * SCALE 1x: beam → centered 128×120; odd logical X = attr cycle.
 * On PPU phase (PHI2 low) and playfield dots, drives VA to tile or attr.
 * Board muxes VA onto VRAM; DQ capture latches TILE/ATTR.
 */
typedef struct R01sBgFetch {
    R01sEntity base;
    int beam_x;
    int beam_y;
    int hblank;
    int vblank;
    int cpu_phase; /* 1 = PHI2 high (CPU owns VRAM) */
    int scale_2x;  /* 1 = 2x (default), 0 = 1x centered */
    uint8_t scroll_x; /* 0..127 */
    uint8_t scroll_y; /* 0..119 */
    uint16_t va;
    int fetching;
    int attr_cycle;
    uint8_t last_tile;
    uint8_t last_attr;
    uint32_t fetch_count;
} R01sBgFetch;

void r01s_bg_fetch_init(R01sBgFetch *chip, const char *refdes);
R01sEntity *r01s_bg_fetch_entity(R01sBgFetch *chip);

void r01s_bg_fetch_set_beam(R01sBgFetch *chip, int x, int y, int hblank, int vblank);
void r01s_bg_fetch_set_scale_2x(R01sBgFetch *chip, int scale_2x);
void r01s_bg_fetch_set_scroll(R01sBgFetch *chip, uint8_t sx, uint8_t sy);
void r01s_bg_fetch_set_cpu_phase(R01sBgFetch *chip, int cpu_phase);
void r01s_bg_fetch_capture_dq(R01sBgFetch *chip, uint8_t data);

uint16_t r01s_bg_fetch_va(const R01sBgFetch *chip);
int r01s_bg_fetch_active(const R01sBgFetch *chip);
int r01s_bg_fetch_attr_cycle(const R01sBgFetch *chip);
uint8_t r01s_bg_fetch_last_tile(const R01sBgFetch *chip);
uint8_t r01s_bg_fetch_last_attr(const R01sBgFetch *chip);
uint32_t r01s_bg_fetch_count(const R01sBgFetch *chip);

#endif
