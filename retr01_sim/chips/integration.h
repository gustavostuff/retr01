#ifndef RETR01_SIM_INTEGRATION_H
#define RETR01_SIM_INTEGRATION_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Island P — system integration stub (needs A–E + G–O + N; F deferred).
 * Board wires beam NMI# → CPU NMIB and tracks VBlank NMI pulses (~60 Hz class).
 */
typedef struct R01sIntegration {
    R01sEntity base;
    uint32_t nmi_pulses;
    uint8_t last_nmi_low;
} R01sIntegration;

void r01s_integration_init(R01sIntegration *chip, const char *refdes);
R01sEntity *r01s_integration_entity(R01sIntegration *chip);

void r01s_integration_note_nmi(R01sIntegration *chip);

uint32_t r01s_integration_nmi_pulses(const R01sIntegration *chip);

#endif
