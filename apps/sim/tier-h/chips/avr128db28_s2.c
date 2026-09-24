#include "avr128db28_s2.h"

#include "retr01_sim/bus.h"

#include <string.h>

void r01s_avr128db28_s2_init(R01sAvr128db28S2 *chip, const char *refdes) {
    r01s_atmega328p_init(chip, refdes ? refdes : "US2");
    if (!chip) {
        return;
    }
    /* Retarget part + SoT pin names; APU regs still via poke (mailbox). */
    chip->base.part = "AVR128DB28";
    chip->base.pin_count = 0;
    chip->base.pin_hash_built = 0;
    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "PAD0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "PAD1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "PAD2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "PAD3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "PAD4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 8, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 9, "PAD5", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "PAD6", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "PAD7", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "SPI_MOSI", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 13, "SPI_MISO", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 14, "SPI_SCK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 15, "/SS_S2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 16, "PAD_DATA", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 17, "AUDIO_PWM", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 18, "P2_START", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 19, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 20, "UPDI", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 21, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 22, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 23, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 24, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 25, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 26, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 27, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 28, "AVCC", R01S_PIN_PWR);
    r01s_entity_set_dip_mm(&chip->base, 28, 35, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_avr128db28_s2_entity(R01sAvr128db28S2 *chip) {
    return r01s_atmega328p_entity(chip);
}
