#ifndef retr01_SIM_FLASHER_BUS_H
#define retr01_SIM_FLASHER_BUS_H

#include "sn74hc595.h"
#include "sst39sf040.h"

#include <stdint.h>

struct R01sAtmega32u4;

typedef struct R01sFlasherBus {
    struct R01sAtmega32u4 *mcu;
    R01sSn74hc595 *shift_lo;
    R01sSn74hc595 *shift_hi;
    R01sSst39sf040 *flash;
} R01sFlasherBus;

void r01s_flasher_bus_init(R01sFlasherBus *bus, struct R01sAtmega32u4 *mcu, R01sSn74hc595 *lo, R01sSn74hc595 *hi,
                           R01sSst39sf040 *flash);

void r01s_flasher_bus_spi_shift(R01sFlasherBus *bus, uint8_t byte);
void r01s_flasher_bus_latch_addr(R01sFlasherBus *bus);
void r01s_flasher_bus_set_addr_ext(R01sFlasherBus *bus, uint32_t addr);

int r01s_flasher_bus_jedec_program(R01sFlasherBus *bus, uint32_t addr, uint8_t data);
int r01s_flasher_bus_jedec_chip_erase(R01sFlasherBus *bus);

uint32_t r01s_flasher_bus_addr(const R01sFlasherBus *bus);

#endif
