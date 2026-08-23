#ifndef RETR01_SIM_PADS_H
#define RETR01_SIM_PADS_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Island E — pad ports $FE60 / $FE61 stand-in (pre-ATmega1284).
 * Bits 0-7: right, left, down, up, X, Y, coin/select, start (1 = pressed).
 *
 * CE# low + OE# low => DQ driven from selected port (A0: 0=$FE60, 1=$FE61).
 */
typedef struct R01sPads {
    R01sEntity base;
    uint8_t port[2];
} R01sPads;

void r01s_pads_init(R01sPads *chip, const char *refdes);
R01sEntity *r01s_pads_entity(R01sPads *chip);

void r01s_pads_set(R01sPads *chip, int port, uint8_t bits);
uint8_t r01s_pads_get(const R01sPads *chip, int port);

/* Sim UI: reflect host port[] on DQ pins when the chip is not selected on the bus. */
void r01s_pads_refresh_preview(R01sPads *chip);

#endif
