#include "pad_uart.h"

static int poll_one(R01sAttiny85 *target, R01sAttiny85 *other, uint8_t poll, uint8_t *reply) {
    if (!target) {
        return 0;
    }
    r01s_attiny85_rx_byte(target, poll);
    if (other) {
        r01s_attiny85_rx_byte(other, poll);
    }
    if (r01s_attiny85_take_reply(target, reply)) {
        /* Ensure the other pad did not also arm (wrong addr should be silent). */
        if (other) {
            uint8_t discard;
            (void)r01s_attiny85_take_reply(other, &discard);
        }
        return 1;
    }
    if (other) {
        uint8_t discard;
        (void)r01s_attiny85_take_reply(other, &discard);
    }
    return 0;
}

void r01s_pad_uart_service(R01sPads *pads, R01sAttiny85 *p1, R01sAttiny85 *p2) {
    uint8_t reply;

    if (!pads) {
        return;
    }

    if (poll_one(p1, p2, 0x55u, &reply)) {
        r01s_pads_set(pads, 0, reply);
    }
    if (poll_one(p2, p1, 0xAAu, &reply)) {
        r01s_pads_set(pads, 1, reply);
    }
    r01s_pads_refresh_preview(pads);
}
