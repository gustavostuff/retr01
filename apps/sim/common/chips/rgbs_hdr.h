#ifndef R01A_RGBS_HDR_H
#define R01A_RGBS_HDR_H

#include "discrete_ic/entity.h"

/* J2-style 1x6 male pin header (docs/general/hardware.md default). */
typedef struct R01aRgbsHdr {
    NsEntity base;
} R01aRgbsHdr;

void r01a_rgbs_hdr_init(R01aRgbsHdr *hdr, const char *refdes);
NsEntity *r01a_rgbs_hdr_entity(R01aRgbsHdr *hdr);

#endif
