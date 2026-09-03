#include "flasher_bus.h"

#include "atmega32u4.h"
#include "retr01_sim/bus.h"

#include <stdio.h>

static void drive_addr_pin(R01sEntity *flash, const char *name, int bit) {
    r01s_entity_drive(flash, name, bit ? R01S_LVL_H : R01S_LVL_L);
}

static void flasher_apply_addr_pins(R01sFlasherBus *bus, uint32_t addr) {
    R01sEntity *flash;
    static const char *const lo_names[8] = {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7"};
    static const char *const hi_names[8] = {"A8", "A9", "A10", "A11", "A12", "A13", "A14", "A15"};
    int i;

    if (!bus || !bus->flash) {
        return;
    }
    flash = r01s_sst39sf040_entity(bus->flash);
    for (i = 0; i < 8; i++) {
        drive_addr_pin(flash, lo_names[i], (addr >> i) & 1);
        drive_addr_pin(flash, hi_names[i], (addr >> (i + 8)) & 1);
    }
    drive_addr_pin(flash, "A16", (addr >> 16) & 1);
    drive_addr_pin(flash, "A17", (addr >> 17) & 1);
    drive_addr_pin(flash, "A18", (addr >> 18) & 1);
}

static void flasher_spi_pulse(R01sFlasherBus *bus, int mosi_bit) {
    R01sEntity *mcu;
    R01sEntity *lo;
    R01sEntity *hi;

    if (!bus || !bus->mcu || !bus->shift_lo || !bus->shift_hi) {
        return;
    }
    mcu = r01s_atmega32u4_entity(bus->mcu);
    lo = r01s_sn74hc595_entity(bus->shift_lo);
    hi = r01s_sn74hc595_entity(bus->shift_hi);

    r01s_entity_drive(mcu, "MOSI", mosi_bit ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(lo, "SER", mosi_bit ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(mcu, "SCK", R01S_LVL_L);
    r01s_entity_drive(lo, "SRCLK", R01S_LVL_L);
    r01s_entity_drive(hi, "SRCLK", R01S_LVL_L);
    r01s_entity_eval(lo);
    r01s_entity_eval(hi);

    r01s_entity_drive(mcu, "SCK", R01S_LVL_H);
    r01s_entity_drive(lo, "SRCLK", R01S_LVL_H);
    r01s_entity_drive(hi, "SRCLK", R01S_LVL_H);
    r01s_entity_eval(lo);
    r01s_entity_eval(hi);

    if (bus->shift_lo->base.pin_count > 0) {
        r01s_entity_drive(hi, "SER", r01s_entity_sense(lo, "Q7S"));
    }

    r01s_entity_drive(mcu, "SCK", R01S_LVL_L);
    r01s_entity_drive(lo, "SRCLK", R01S_LVL_L);
    r01s_entity_drive(hi, "SRCLK", R01S_LVL_L);
    r01s_entity_eval(lo);
    r01s_entity_eval(hi);
}

static void flasher_drive_parallel(R01sFlasherBus *bus, uint8_t data) {
    R01sEntity *mcu;
    R01sEntity *flash;
    int i;
    char mcu_name[4];
    char flash_name[4];

    if (!bus || !bus->mcu || !bus->flash) {
        return;
    }
    mcu = r01s_atmega32u4_entity(bus->mcu);
    flash = r01s_sst39sf040_entity(bus->flash);
    for (i = 0; i < 8; i++) {
        snprintf(mcu_name, sizeof(mcu_name), "D%d", i);
        snprintf(flash_name, sizeof(flash_name), "DQ%d", i);
        r01s_entity_drive(mcu, mcu_name, (data & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
        r01s_entity_drive(flash, flash_name, (data & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

static void flasher_we_pulse(R01sFlasherBus *bus) {
    R01sEntity *mcu;
    R01sEntity *flash;

    if (!bus || !bus->mcu || !bus->flash) {
        return;
    }
    mcu = r01s_atmega32u4_entity(bus->mcu);
    flash = r01s_sst39sf040_entity(bus->flash);
    r01s_entity_drive(mcu, "WE#", R01S_LVL_L);
    r01s_entity_drive(flash, "WE#", R01S_LVL_L);
    r01s_entity_eval(flash);
    r01s_entity_drive(mcu, "WE#", R01S_LVL_H);
    r01s_entity_drive(flash, "WE#", R01S_LVL_H);
    r01s_entity_eval(flash);
}

void r01s_flasher_bus_init(R01sFlasherBus *bus, R01sAtmega32u4 *mcu, R01sSn74hc595 *lo, R01sSn74hc595 *hi,
                           R01sSst39sf040 *flash) {
    if (!bus) {
        return;
    }
    bus->mcu = mcu;
    bus->shift_lo = lo;
    bus->shift_hi = hi;
    bus->flash = flash;
    if (lo) {
        r01s_entity_drive(r01s_sn74hc595_entity(lo), "OE#", R01S_LVL_L);
        r01s_entity_drive(r01s_sn74hc595_entity(lo), "SRCLR#", R01S_LVL_H);
    }
    if (hi) {
        r01s_entity_drive(r01s_sn74hc595_entity(hi), "OE#", R01S_LVL_L);
        r01s_entity_drive(r01s_sn74hc595_entity(hi), "SRCLR#", R01S_LVL_H);
    }
}

void r01s_flasher_bus_spi_shift(R01sFlasherBus *bus, uint8_t byte) {
    int bit;
    for (bit = 7; bit >= 0; bit--) {
        flasher_spi_pulse(bus, (byte >> bit) & 1);
    }
}

void r01s_flasher_bus_latch_addr(R01sFlasherBus *bus) {
    R01sEntity *lo;
    R01sEntity *hi;
    if (!bus || !bus->shift_lo || !bus->shift_hi) {
        return;
    }
    lo = r01s_sn74hc595_entity(bus->shift_lo);
    hi = r01s_sn74hc595_entity(bus->shift_hi);
    r01s_entity_drive(lo, "RCLK", R01S_LVL_L);
    r01s_entity_drive(hi, "RCLK", R01S_LVL_L);
    r01s_entity_eval(lo);
    r01s_entity_eval(hi);
    r01s_entity_drive(lo, "RCLK", R01S_LVL_H);
    r01s_entity_drive(hi, "RCLK", R01S_LVL_H);
    r01s_entity_eval(lo);
    r01s_entity_eval(hi);
    r01s_entity_drive(lo, "RCLK", R01S_LVL_L);
    r01s_entity_drive(hi, "RCLK", R01S_LVL_L);
    r01s_entity_eval(lo);
    r01s_entity_eval(hi);
}

void r01s_flasher_bus_set_addr_ext(R01sFlasherBus *bus, uint32_t addr) {
    R01sEntity *mcu;
    if (!bus || !bus->mcu) {
        return;
    }
    mcu = r01s_atmega32u4_entity(bus->mcu);
    r01s_entity_drive(mcu, "A16", (addr & (1u << 16)) ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(mcu, "A17", (addr & (1u << 17)) ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(mcu, "A18", (addr & (1u << 18)) ? R01S_LVL_H : R01S_LVL_L);
    flasher_apply_addr_pins(bus, addr);
}

uint32_t r01s_flasher_bus_addr(const R01sFlasherBus *bus) {
    uint32_t addr = 0;
    if (!bus) {
        return 0;
    }
    if (bus->shift_lo) {
        addr |= r01s_sn74hc595_latched(bus->shift_lo);
    }
    if (bus->shift_hi) {
        addr |= (uint32_t)r01s_sn74hc595_latched(bus->shift_hi) << 8;
    }
    if (bus->mcu) {
        R01sEntity *mcu = r01s_atmega32u4_entity(bus->mcu);
        if (r01s_level_is_high(r01s_entity_sense(mcu, "A16"))) {
            addr |= 1u << 16;
        }
        if (r01s_level_is_high(r01s_entity_sense(mcu, "A17"))) {
            addr |= 1u << 17;
        }
        if (r01s_level_is_high(r01s_entity_sense(mcu, "A18"))) {
            addr |= 1u << 18;
        }
    }
    return addr;
}

int r01s_flasher_bus_jedec_program(R01sFlasherBus *bus, uint32_t addr, uint8_t data) {
    if (!bus || !bus->flash) {
        return -1;
    }
    flasher_apply_addr_pins(bus, addr);
    flasher_drive_parallel(bus, data);
    flasher_we_pulse(bus);
    r01s_sst39sf040_write_cycle(bus->flash, 0x5555u, 0xAAu);
    r01s_sst39sf040_write_cycle(bus->flash, 0x2AAAu, 0x55u);
    r01s_sst39sf040_write_cycle(bus->flash, 0x5555u, 0xA0u);
    r01s_sst39sf040_write_cycle(bus->flash, addr, data);
    return 0;
}

int r01s_flasher_bus_jedec_chip_erase(R01sFlasherBus *bus) {
    if (!bus || !bus->flash) {
        return -1;
    }
    flasher_we_pulse(bus);
    r01s_sst39sf040_write_cycle(bus->flash, 0x5555u, 0xAAu);
    r01s_sst39sf040_write_cycle(bus->flash, 0x2AAAu, 0x55u);
    r01s_sst39sf040_write_cycle(bus->flash, 0x5555u, 0x80u);
    r01s_sst39sf040_write_cycle(bus->flash, 0x5555u, 0xAAu);
    r01s_sst39sf040_write_cycle(bus->flash, 0x2AAAu, 0x55u);
    r01s_sst39sf040_write_cycle(bus->flash, 0x5555u, 0x10u);
    return 0;
}
