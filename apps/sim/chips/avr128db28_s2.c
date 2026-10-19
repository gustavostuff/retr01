#include "avr128db28_s2.h"

#include "avr128db28_pins.h"
#include "retr01_sim/bus.h"

#include <string.h>

void r01s_avr128db28_s2_init(R01sAvr128db28S2 *chip, const char *refdes) {
    r01s_atmega328p_init(chip, refdes ? refdes : "US2");
    if (!chip) {
        return;
    }
    chip->base.part = "AVR128DB28";
    chip->base.pin_count = 0;
    chip->base.pin_hash_built = 0;

    r01s_entity_add_pin(&chip->base, R01S_AVR_PA7, "PAD7", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PC0, "P2_RIGHT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PC1, "P2_LEFT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PC2, "P2_DOWN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PC3, "P2_UP", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_VDDIO2, "VDDIO2", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD1, "P2_X", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD2, "P2_Y", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD3, "P2_COIN", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD4, "SPI_MOSI", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD5, "SPI_MISO", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD6, "SPI_SCK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PD7, "/SS_S2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_AVDD, "AVDD", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, R01S_AVR_GND1, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PF0, "PAD_DATA", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PF1, "AUDIO_PWM", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PF6, "P2_START", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_UPDI, "UPDI", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_VDD, "VDD", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, R01S_AVR_GND2, "GND2", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA0, "PAD0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA1, "PAD1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA2, "PAD2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA3, "PAD3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA4, "PAD4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA5, "PAD5", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_PA6, "PAD6", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, R01S_AVR_SIM_CLK, "CLK", R01S_PIN_IN);
    r01s_entity_set_dip_mm(&chip->base, 28, 35, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_avr128db28_s2_entity(R01sAvr128db28S2 *chip) {
    return r01s_atmega328p_entity(chip);
}
