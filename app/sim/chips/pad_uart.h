#ifndef retr01_SIM_PAD_UART_H
#define retr01_SIM_PAD_UART_H

#include "attiny85.h"
#include "pads.h"

/*
 * Half-duplex pad UART host stand-in (1284 USART firmware).
 * Polls P1 (0x55) then P2 (0xAA); writes replies into pads.port[].
 * Missed reply keeps the previous port value (docs/controllers.md).
 */
void r01s_pad_uart_service(R01sPads *pads, R01sAttiny85 *p1, R01sAttiny85 *p2);

#endif
