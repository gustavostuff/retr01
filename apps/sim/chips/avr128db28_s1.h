#ifndef retr01_SIM_AVR128DB28_S1_H
#define retr01_SIM_AVR128DB28_S1_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_OAM_ENTRIES 128u
#define R01S_OAM_ENTRY_BYTES 4u
#define R01S_OAM_BYTES (R01S_OAM_ENTRIES * R01S_OAM_ENTRY_BYTES)

/*
 * MCU-S1 (AVR128DB28) -- OAM apply (mailbox) + field SRAM via ALE/HC573.
 * Soft $7Fxx / cart I2C stay on MCU-M. Behavioral shell. 24 MHz domain.
 */
typedef struct R01sAvr128db28S1 {
    R01sEntity base;
    uint8_t oam[R01S_OAM_BYTES];
    uint16_t oam_addr;
    uint32_t clk_ticks;
    uint8_t alive;
} R01sAvr128db28S1;

void r01s_avr128db28_s1_init(R01sAvr128db28S1 *chip, const char *refdes);
R01sEntity *r01s_avr128db28_s1_entity(R01sAvr128db28S1 *chip);

uint16_t r01s_avr128db28_s1_oam_addr(const R01sAvr128db28S1 *chip);
uint8_t r01s_avr128db28_s1_oam_peek(const R01sAvr128db28S1 *chip, uint16_t addr);
void r01s_avr128db28_s1_oam_poke(R01sAvr128db28S1 *chip, uint16_t addr, uint8_t data);

uint32_t r01s_avr128db28_s1_clk_ticks(const R01sAvr128db28S1 *chip);
int r01s_avr128db28_s1_alive(const R01sAvr128db28S1 *chip);

#endif
