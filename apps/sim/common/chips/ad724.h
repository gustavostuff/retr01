#ifndef R01A_AD724_H
#define R01A_AD724_H

#include "discrete_ic/entity.h"

/*
 * AD724 RGB-to-composite encoder. Digital shell only.
 * COMP is H when encode is live (ENCD, supplies, FSC toggling).
 */
typedef struct R01aAd724 {
    NsEntity base;
    NsLevel fin_prev;
    int saw_fin_edge;
    int encode_ok;
} R01aAd724;

void r01a_ad724_init(R01aAd724 *chip, const char *refdes);
NsEntity *r01a_ad724_entity(R01aAd724 *chip);
int r01a_ad724_encode_ok(const R01aAd724 *chip);

#endif
