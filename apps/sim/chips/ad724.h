#ifndef retr01_SIM_AD724_H
#define retr01_SIM_AD724_H

#include "retr01_sim/entity.h"

/*
 * Sony CXA2075M RGB encoder (U2075). SOP-24.
 * Used pins: GND1, RIN, GIN, BIN, SCIN, NPIN, SYNCIN, VCC1, VCC2, CVOUT, GND2.
 * Logic stub: CVOUT is a video-present flag. No NTSC waveform.
 */
typedef struct R01sAd724 {
    R01sEntity base;
} R01sAd724;

void r01s_ad724_init(R01sAd724 *chip, const char *refdes);
R01sEntity *r01s_ad724_entity(R01sAd724 *chip);

#endif
