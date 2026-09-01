#ifndef retr01_SIM_ATMEGA328P_H
#define retr01_SIM_ATMEGA328P_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_APU_REGS 0x20u

/*
 * Island K -- ATmega328P APU stub (behavioral; not a full AVR core).
 *
 * Soft bus window $FE40-$FE5F (A0-A4). Sim register map:
 *   [0] $FE40  bit0=enable, bits4-7=volume (0-15)
 *   [1] $FE41  period low
 *   [2] $FE42  period high (bits0-2) -> 11-bit period
 *   [3..]      storage only
 *
 * Tick synthesizes a digital square on PWM (duty from volume).
 * Pinout: simplified port decode -- GPIO<->$FE4x schematic still TBD (hw/md).
 */
typedef struct R01sAtmega328p {
    R01sEntity base;
    uint8_t regs[R01S_APU_REGS];
    uint16_t phase;
    uint32_t pwm_hi_samples;
    uint32_t pwm_edges;
    R01sLevel pwm_prev;
} R01sAtmega328p;

void r01s_atmega328p_init(R01sAtmega328p *chip, const char *refdes);
R01sEntity *r01s_atmega328p_entity(R01sAtmega328p *chip);

uint8_t r01s_atmega328p_peek(const R01sAtmega328p *chip, unsigned reg);
void r01s_atmega328p_poke(R01sAtmega328p *chip, unsigned reg, uint8_t data);

int r01s_atmega328p_enabled(const R01sAtmega328p *chip);
uint16_t r01s_atmega328p_period(const R01sAtmega328p *chip);
uint32_t r01s_atmega328p_pwm_edges(const R01sAtmega328p *chip);
uint32_t r01s_atmega328p_pwm_hi_samples(const R01sAtmega328p *chip);

#endif
