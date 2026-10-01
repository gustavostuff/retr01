#ifndef retr01_SIM_RCA_JACK_H
#define retr01_SIM_RCA_JACK_H

#include "retr01_sim/entity.h"

/*
 * CUI RCJ-01x edge RCA (J8 audio / J9 composite).
 * Pads match Retr01_Lib:CUI_RCJ-014: 1A/1B/1C shell, 2 tip.
 */
typedef struct R01sRcaJack {
    R01sEntity base;
} R01sRcaJack;

void r01s_rca_jack_init(R01sRcaJack *jack, const char *refdes, const char *part);
R01sEntity *r01s_rca_jack_entity(R01sRcaJack *jack);

#endif
