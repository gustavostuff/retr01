#ifndef retr01_SIM_AD724_H
#define retr01_SIM_AD724_H

#include "retr01_sim/entity.h"

/*
 * Analog Devices AD724 RGB-to-NTSC/PAL encoder (U725).
 * PCB footprint is DIP-16 (SOIC-16-to-DIP adapter). The canvas also draws DIP-16.
 * Logic stub: COMP is a video-present flag. No NTSC waveform.
 * Pin names match docs/ic_behavior/AD724.md.
 */
typedef struct R01sAd724 {
    R01sEntity base;
} R01sAd724;

void r01s_ad724_init(R01sAd724 *chip, const char *refdes);
R01sEntity *r01s_ad724_entity(R01sAd724 *chip);

#endif
