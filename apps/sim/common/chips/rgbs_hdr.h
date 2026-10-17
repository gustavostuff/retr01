#ifndef R01A_RGBS_HDR_H
#define R01A_RGBS_HDR_H

#include "discrete_ic/entity.h"

/* Breadboard RGBS sprite. Product J2 is 1x8 (docs/hardware.md). */
typedef struct R01aRgbsHdr {
    NsEntity base;
} R01aRgbsHdr;

void r01a_rgbs_hdr_init(R01aRgbsHdr *hdr, const char *refdes);
NsEntity *r01a_rgbs_hdr_entity(R01aRgbsHdr *hdr);

#endif
