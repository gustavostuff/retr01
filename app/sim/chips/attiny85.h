#ifndef retr01_SIM_ATTINY85_H
#define retr01_SIM_ATTINY85_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Retr01-C pad MCU (behavioral). Speaks half-duplex open-drain UART on DATA:
 * host poll 0x55 (P1) / 0xAA (P2) -> 1-byte button bitfield reply.
 * See docs/controllers.md.
 */
typedef struct R01sAttiny85 {
    R01sEntity base;
    uint8_t poll_addr;   /* 0x55 or 0xAA */
    uint8_t buttons;     /* $FE60/$FE61 layout; 1 = pressed */
    uint8_t reply_armed; /* 1 = reply_byte ready for host take */
    uint8_t reply_byte;
    uint8_t driving_low; /* 1 = pulling DATA low (OD active) */
} R01sAttiny85;

void r01s_attiny85_init(R01sAttiny85 *chip, const char *refdes, uint8_t poll_addr);
R01sEntity *r01s_attiny85_entity(R01sAttiny85 *chip);

void r01s_attiny85_set_buttons(R01sAttiny85 *chip, uint8_t bits);
uint8_t r01s_attiny85_get_buttons(const R01sAttiny85 *chip);

/* Host TX: deliver one UART byte on the shared DATA bus. */
void r01s_attiny85_rx_byte(R01sAttiny85 *chip, uint8_t byte);

/* Host RX: 1 if this pad armed a reply (clears arm). */
int r01s_attiny85_take_reply(R01sAttiny85 *chip, uint8_t *out);

#endif
