/*
 * Tier A-C link the tier-H canvas without the motherboard sim.
 * These symbols are referenced by that canvas and are unused when there is no R01sBoard.
 */
#include "retr01_sim/frame_log.h"

#include <stddef.h>
#include <stdint.h>

struct R01sBoard;
struct R01sIslandGroup;
struct R01sBgFetch;
struct R01sAtmega328p;
struct R01sApuVoice;

struct R01sBoard *r01s_board_from_group(struct R01sIslandGroup *group) {
    (void)group;
    return NULL;
}

void r01s_board_netlist_rebuild(struct R01sBoard *board) {
    (void)board;
}

void r01s_bg_fetch_set_scale_2x(struct R01sBgFetch *chip, int scale_2x) {
    (void)chip;
    (void)scale_2x;
}

int r01s_frame_log_enabled(void) {
    return 0;
}

void r01s_frame_log_page_delta(int delta) {
    (void)delta;
}

int r01s_atmega328p_scope_copy(const struct R01sAtmega328p *chip, uint8_t *dst, int max_n) {
    (void)chip;
    (void)dst;
    (void)max_n;
    return 0;
}

const struct R01sApuVoice *r01s_atmega328p_voice(const struct R01sAtmega328p *chip, int ch) {
    (void)chip;
    (void)ch;
    return NULL;
}

int r01s_apu_voice_wave_y(const struct R01sApuVoice *v, int x, int width) {
    (void)v;
    (void)x;
    (void)width;
    return 0;
}
