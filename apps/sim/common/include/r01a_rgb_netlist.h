#ifndef R01A_RGB_NETLIST_H
#define R01A_RGB_NETLIST_H

#include "discrete_ic/entity.h"

typedef void (*R01aNetLinkFn)(NsEntity *a, const char *an, NsEntity *b, const char *bn);

void r01a_netlist_link_dac_rgbs(R01aNetLinkFn link, NsEntity *y2, NsEntity *u24, NsEntity *j2,
                                NsEntity *r1, NsEntity *r2, NsEntity *r3, NsEntity *r4, NsEntity *r5,
                                NsEntity *r6, NsEntity *r7, NsEntity *r8, NsEntity *r9, NsEntity *r10,
                                NsEntity *r11);

#endif
