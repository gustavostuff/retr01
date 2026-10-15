#ifndef retr01_SIM_AD724_H
#define retr01_SIM_AD724_H

#include "retr01_sim/entity.h"

/*
 * Analog Devices AD724 RGB-to-NTSC/PAL encoder (U725).
 * Silicon is SOIC-16. The motherboard footprint is DIP-8. The adapter joins
 * the two supplies and the two grounds, holds STND, ENCD, and VSYNC high,
 * holds SELECT low, and leaves CRMA and LUMA off the board.
 * Logic stub: COMP is a video-present flag. No NTSC waveform.
 */
typedef struct R01sAd724 {
    R01sEntity base;
} R01sAd724;

void r01s_ad724_init(R01sAd724 *chip, const char *refdes);
R01sEntity *r01s_ad724_entity(R01sAd724 *chip);

#endif
