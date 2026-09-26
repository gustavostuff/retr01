#ifndef R01A_AVR128DB28_S1_H
#define R01A_AVR128DB28_S1_H

#include "netlist_sim/entity.h"

#include <stdint.h>

typedef struct R01aAvr128db28S1 {
    NsEntity base;
    uint32_t frame;
    int alive;
} R01aAvr128db28S1;

void r01a_avr128db28_s1_init(R01aAvr128db28S1 *chip, const char *refdes);
NsEntity *r01a_avr128db28_s1_entity(R01aAvr128db28S1 *chip);
uint32_t r01a_avr128db28_s1_frame(const R01aAvr128db28S1 *chip);
void r01a_avr128db28_s1_bump_frame(R01aAvr128db28S1 *chip);

#endif
