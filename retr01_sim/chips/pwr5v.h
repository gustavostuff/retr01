#ifndef RETR01_SIM_PWR5V_H
#define RETR01_SIM_PWR5V_H

#include "retr01_sim/entity.h"

/*
 * Island A — board 5 V rail / regulator model.
 * Not a BOM line item; represents clean VDD for proto bring-up.
 *
 * Pins (SIP-4 style):
 *   1 VIN   2 EN   3 VDD   4 GND
 * Eval: VDD = H iff VIN=H and EN!=L (EN Z treated as enabled).
 */
typedef struct R01sPwr5v {
    R01sEntity base;
    int power_ok; /* 1 when VDD driven high */
} R01sPwr5v;

void r01s_pwr5v_init(R01sPwr5v *chip, const char *refdes);
R01sEntity *r01s_pwr5v_entity(R01sPwr5v *chip);

#endif
