#include "attiny85.h"
#include "pad_uart.h"
#include "pads.h"
#include "test_common.h"

int main(void) {
    R01sPads pads;
    R01sAttiny85 p1;
    R01sAttiny85 p2;

    r01s_pads_init(&pads, "PAD");
    r01s_attiny85_init(&p1, "UPAD1", 0x55u);
    r01s_attiny85_init(&p2, "UPAD2", 0xAAu);

    r01s_attiny85_set_buttons(&p1, 0xA5);
    r01s_attiny85_set_buttons(&p2, 0x5A);
    r01s_pad_uart_service(&pads, &p1, &p2);
    expect_true(r01s_pads_get(&pads, 0) == 0xA5, "P1 via 0x55");
    expect_true(r01s_pads_get(&pads, 1) == 0x5A, "P2 via 0xAA");

    /* Hold last on miss: service with no reply-capable pad for P1. */
    r01s_attiny85_set_buttons(&p1, 0x11);
    r01s_attiny85_set_buttons(&p2, 0x22);
    r01s_pad_uart_service(&pads, NULL, &p2);
    expect_true(r01s_pads_get(&pads, 0) == 0xA5, "P1 holds last on miss");
    expect_true(r01s_pads_get(&pads, 1) == 0x22, "P2 updates");

    r01s_pad_uart_service(&pads, &p1, NULL);
    expect_true(r01s_pads_get(&pads, 0) == 0x11, "P1 updates");
    expect_true(r01s_pads_get(&pads, 1) == 0x22, "P2 holds last on miss");

    return test_done("test_pad_uart");
}
