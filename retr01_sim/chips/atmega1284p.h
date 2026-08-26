#ifndef retr01_SIM_ATMEGA1284P_H
#define retr01_SIM_ATMEGA1284P_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_OAM_BYTES 256u
#define R01S_MCU1284_EEPROM_MAILBOX 3u

/*
 * Island L — ATmega1284P stub (behavioral; not a full AVR core).
 *
 * Soft ports (board decode):
 *   $FE20  OAM address latch (A0=0)
 *   $FE21  OAM data + auto-inc (A0=1) — 64 entries Y,tile,attr,X
 *   $FE70–$FE72 mailbox stub (board poke; protocol TBD)
 *
 * Pads stay on Island E until N merges 1284 pad path.
 * Line-buffer fill is Island M/N.
 *
 * Tick advances a 20 MHz domain counter (sim step stub; LCM master later).
 * Pinout: simplified port decode — GPIO↔$FExx schematic TBD (hw/md).
 */
typedef struct R01sAtmega1284p {
    R01sEntity base;
    uint8_t oam[R01S_OAM_BYTES];
    uint8_t oam_addr;
    uint8_t eeprom_mb[R01S_MCU1284_EEPROM_MAILBOX];
    uint8_t last_dq;
    int we_prev; /* WE# was low */
    int oe_prev; /* OE# was low */
    uint32_t clk_ticks;
    uint8_t alive;
} R01sAtmega1284p;

void r01s_atmega1284p_init(R01sAtmega1284p *chip, const char *refdes);
R01sEntity *r01s_atmega1284p_entity(R01sAtmega1284p *chip);

uint8_t r01s_atmega1284p_oam_addr(const R01sAtmega1284p *chip);
uint8_t r01s_atmega1284p_oam_peek(const R01sAtmega1284p *chip, uint8_t addr);
void r01s_atmega1284p_oam_poke(R01sAtmega1284p *chip, uint8_t addr, uint8_t data);

uint8_t r01s_atmega1284p_eeprom_peek(const R01sAtmega1284p *chip, unsigned i);
void r01s_atmega1284p_eeprom_poke(R01sAtmega1284p *chip, unsigned i, uint8_t data);

uint32_t r01s_atmega1284p_clk_ticks(const R01sAtmega1284p *chip);
int r01s_atmega1284p_alive(const R01sAtmega1284p *chip);

#endif
